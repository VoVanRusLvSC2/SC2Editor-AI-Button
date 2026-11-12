#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <commdlg.h>
#include <cstdio>
#include "../src/bridge_client.h"
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
static HWND ai,diffWindow;
static BOOL CALLBACK FindDiff(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Diff.v39"))diffWindow=h;return TRUE;}
static BOOL CALLBACK Find(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v39"))ai=h;return TRUE;}
struct TranscriptProbe { int replaceAll=0;int rectChanges=0; };
static LRESULT CALLBACK CountTranscriptChanges(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR raw){
    auto probe=reinterpret_cast<TranscriptProbe*>(raw);
    if(msg==WM_SETTEXT)++probe->replaceAll;
    if(msg==EM_SETRECT)++probe->rectChanges;
    return DefSubclassProc(h,msg,w,l);
}
struct PopupProbe { DWORD uiThread;HDESK desktop;bool found,labels; };
static BOOL CALLBACK FindMenu(HWND h,LPARAM raw){wchar_t cls[80]{};GetClassNameW(h,cls,80);if(!lstrcmpW(cls,L"#32768")){*reinterpret_cast<HWND*>(raw)=h;return FALSE;}return TRUE;}
static DWORD WINAPI SelectFirstOutline(void* raw){
    auto probe=static_cast<PopupProbe*>(raw);if(!SetThreadDesktop(probe->desktop))return 1;HWND popup=nullptr;
    for(int i=0;i<100 && !popup;++i){EnumThreadWindows(probe->uiThread,FindMenu,reinterpret_cast<LPARAM>(&popup));if(!popup)Sleep(20);}
    if(!popup)return 2;probe->found=true;auto menu=reinterpret_cast<HMENU>(SendMessageW(popup,0x01e1,0,0));
    wchar_t first[256]{},second[256]{};if(menu && GetMenuItemCount(menu)==2){GetMenuStringW(menu,0,first,256,MF_BYPOSITION);GetMenuStringW(menu,1,second,256,MF_BYPOSITION);probe->labels=wcsstr(first,L"usertext") && wcsstr(second,L"secondrequest");}
    PostMessageW(ai,WM_KEYDOWN,VK_DOWN,0);PostMessageW(ai,WM_KEYDOWN,VK_RETURN,0);Sleep(500);if(IsWindow(popup))PostMessageW(popup,WM_CANCELMODE,0,0);return 0;
}
static bool SavePreview(HWND window,const wchar_t* path){
    RECT r{};if(!GetWindowRect(window,&r))return false;int width=r.right-r.left,height=r.bottom-r.top;if(width<=0 || height<=0)return false;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;info.bmiHeader.biSizeImage=static_cast<DWORD>(width*height*4);
    HDC dc=CreateCompatibleDC(nullptr);void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!dc || !bitmap || !bits){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return false;}
    auto old=SelectObject(dc,bitmap);RECT canvas{0,0,width,height};FillRect(dc,&canvas,GetSysColorBrush(COLOR_WINDOW));
    SendMessageW(window,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);bool captured=true;GdiFlush();
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info.bmiHeader);file.bfSize=file.bfOffBits+info.bmiHeader.biSizeImage;
    HANDLE out=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;
    bool ok=captured && out!=INVALID_HANDLE_VALUE && WriteFile(out,&file,sizeof(file),&written,nullptr) && written==sizeof(file) &&
        WriteFile(out,&info.bmiHeader,sizeof(info.bmiHeader),&written,nullptr) && written==sizeof(info.bmiHeader) &&
        WriteFile(out,bits,info.bmiHeader.biSizeImage,&written,nullptr) && written==info.bmiHeader.biSizeImage;
    if(out!=INVALID_HANDLE_VALUE)CloseHandle(out);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return ok;
}
// Exercise full and clipped owner-draw repaints in a memory bitmap. A partially
// invalidated button must retain the same complete native-style frame and centered glyph.
static bool TestSendDrawing(HWND window){
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=30;info.bmiHeader.biHeight=-30;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;info.bmiHeader.biSizeImage=30*30*4;
    HDC dc=CreateCompatibleDC(nullptr);void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!dc || !bitmap || !bits){if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);return false;}
    auto original=SelectObject(dc,bitmap);DRAWITEMSTRUCT item{};item.CtlType=ODT_BUTTON;item.CtlID=108;item.hwndItem=GetDlgItem(window,108);item.hDC=dc;item.rcItem={0,0,30,30};
    auto draw=[&](){SendMessageW(window,WM_DRAWITEM,108,reinterpret_cast<LPARAM>(&item));GdiFlush();};
    auto check=[&](){COLORREF background=GetPixel(dc,8,8);return background==GetPixel(dc,21,8) && background==GetPixel(dc,8,21) && background==GetPixel(dc,21,21) && GetPixel(dc,15,15)==RGB(0,91,145);};
    draw();bool ok=check();
    // Windows may repaint only part of an owner-drawn button after uncovering another window.
    const RECT clips[]={{0,0,15,15},{15,0,30,15},{0,15,15,30},{15,15,30,30}};
    for(const auto& r:clips){int saved=SaveDC(dc);IntersectClipRect(dc,r.left,r.top,r.right,r.bottom);draw();RestoreDC(dc,saved);if(!check())ok=false;}
    wchar_t path[32768]{};DWORD pathLength=GetEnvironmentVariableW(L"SC2_SEND_PREVIEW",path,32768);
    if(pathLength && pathLength<32768){
        BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+info.bmiHeader.biSizeImage;
        HANDLE out=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;
        if(out!=INVALID_HANDLE_VALUE){WriteFile(out,&header,sizeof(header),&written,nullptr);WriteFile(out,&info.bmiHeader,sizeof(info.bmiHeader),&written,nullptr);WriteFile(out,bits,info.bmiHeader.biSizeImage,&written,nullptr);CloseHandle(out);}
    }
    item.itemState=ODS_DISABLED;draw();if(GetPixel(dc,15,15)!=GetSysColor(COLOR_GRAYTEXT))ok=false;
    SelectObject(dc,original);DeleteObject(bitmap);DeleteDC(dc);return ok;
}
int wmain(){
    wchar_t name[80]{};swprintf_s(name,L"SC2AIMultiTest-%lu",GetCurrentProcessId());
    HWINSTA station=CreateWindowStationW(name,0,WINSTA_ALL_ACCESS,nullptr);if(!station||!SetProcessWindowStation(station))return 1;
    HDESK desktop=CreateDesktopW(L"ImageTest",nullptr,nullptr,0,GENERIC_ALL,nullptr);if(!desktop||!SetThreadDesktop(desktop))return 2;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common))return 3;
    HWND owner=CreateWindowExW(0,L"STATIC",L"test",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,nullptr,nullptr);
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT,0,0,900,30,owner,nullptr,nullptr,nullptr);if(!owner||!bar)return 4;
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);TBBUTTON buttons[2]{};for(int i=0;i<2;++i){buttons[i].iBitmap=I_IMAGENONE;buttons[i].idCommand=100+i;buttons[i].fsState=TBSTATE_ENABLED;}
    if(!SendMessageW(bar,TB_ADDBUTTONSW,2,reinterpret_cast<LPARAM>(buttons)))return 5;
    HMODULE dll=LoadLibraryW(L"SC2AIButton.dll");if(!dll)return 6;auto attach=reinterpret_cast<Attach>(GetProcAddress(dll,"SC2AI_Attach"));if(!attach||!attach(bar,1,500))return 7;
    SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));EnumThreadWindows(GetCurrentThreadId(),Find,0);if(!ai)return 8;
    auto event=[&](const char* kind,std::vector<std::wstring> fields={}){auto e=new BridgeEvent{nullptr,kind,fields};SendMessageW(ai,WM_AI_BRIDGE,0,reinterpret_cast<LPARAM>(e));};
    if(!TestSendDrawing(ai)){std::puts("send button repaint failed");return 49;}
    HWND edit=GetDlgItem(ai,105);
    HWND composer=GetDlgItem(ai,107),changes=GetDlgItem(ai,124),placeholder=GetDlgItem(ai,125);RECT smallRect{},largeRect{},shrunkRect{};
    if(!placeholder || !(GetWindowLongPtrW(placeholder,GWL_STYLE)&WS_VISIBLE))return 46;
    SendMessageW(composer,WM_SETFOCUS,0,0);if(GetWindowLongPtrW(placeholder,GWL_STYLE)&WS_VISIBLE)return 47;
    SendMessageW(composer,WM_KILLFOCUS,0,0);if(!(GetWindowLongPtrW(placeholder,GWL_STYLE)&WS_VISIBLE))return 48;
    GetWindowRect(composer,&smallRect);SetWindowTextW(composer,L"one\r\ntwo\r\nthree\r\nfour\r\nfive\r\nsix");SendMessageW(ai,WM_COMMAND,MAKEWPARAM(107,EN_CHANGE),reinterpret_cast<LPARAM>(composer));GetWindowRect(composer,&largeRect);
    if(largeRect.bottom-largeRect.top<=smallRect.bottom-smallRect.top || (GetWindowLongPtrW(placeholder,GWL_STYLE)&WS_VISIBLE))return 40;
    SetWindowTextW(composer,L"");SendMessageW(ai,WM_COMMAND,MAKEWPARAM(107,EN_CHANGE),reinterpret_cast<LPARAM>(composer));GetWindowRect(composer,&shrunkRect);if(shrunkRect.bottom-shrunkRect.top!=smallRect.bottom-smallRect.top)return 41;
    if(!changes || IsWindowEnabled(changes))return 42;
    event("changes_begin");event("change",{L"change-id",L"example.txt",L"2",L"1",L"0"});event("changes_end");
    if(!IsWindowEnabled(changes))return 43;
    event("selected",{L"test-thread"});event("change_diff",{L"test-thread",L"change-id",L"--- a/test\n+++ b/test\n-old\n+new\n"});
    EnumThreadWindows(GetCurrentThreadId(),FindDiff,0);HWND diffText=GetDlgItem(diffWindow,1);
    if(!IsWindow(diffWindow) || GetWindowTextLengthW(diffText)<20 || !(GetWindowLongPtrW(diffText,GWL_STYLE)&ES_READONLY))return 45;
    DestroyWindow(diffWindow);
    event("clear");if(IsWindowEnabled(changes))return 44;
    LOGFONTW inputFont{};auto font=reinterpret_cast<HFONT>(SendMessageW(GetDlgItem(ai,107),WM_GETFONT,0,0));
    if(!font || GetObjectW(font,sizeof(inputFont),&inputFont)!=sizeof(inputFont) || inputFont.lfHeight>-15)return 27;
    auto check=[&](const wchar_t* text,COLORREF expected){
        FINDTEXTEXW find{};find.chrg.cpMin=0;find.chrg.cpMax=-1;find.lpstrText=const_cast<wchar_t*>(text);
        if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&find))<0)return false;
        CHARRANGE selection{find.chrgText.cpMin,find.chrgText.cpMin+1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&selection));
        CHARFORMAT2W format{};format.cbSize=sizeof(format);SendMessageW(edit,EM_GETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&format));return (format.dwMask&CFM_COLOR) && format.crTextColor==expected;
    };
    event("history",{L"You: usertext question about the native editor\r\n\r\nAI: firsttext **boldtext**\r\n\r\nsecondtext\r\n\r\n[bracketed reply]\r\n\r\n[Command]\r\n\r\nAI: thirdtext\r\n\r\nYou: да\r\n\r\nAI: Готово.\r\n\r\nYou: secondrequest investigate the map\r\n\r\nAI: final answer"});
    if(!IsWindowEnabled(GetDlgItem(ai,123)))return 30;
    CHARRANGE atEnd{-1,-1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&atEnd));
    PopupProbe probe{GetCurrentThreadId(),desktop,false,false};HANDLE thread=CreateThread(nullptr,0,SelectFirstOutline,&probe,0,nullptr);if(!thread)return 31;
    SendMessageW(ai,WM_COMMAND,MAKEWPARAM(123,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(ai,123)));
    WaitForSingleObject(thread,3000);CloseHandle(thread);CHARRANGE jumped{};SendMessageW(edit,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&jumped));
    if(!probe.found || !probe.labels || jumped.cpMin!=0 || jumped.cpMax!=0){std::printf("outline test: popup=%d labels=%d range=%ld,%ld\n",probe.found,probe.labels,jumped.cpMin,jumped.cpMax);return 32;}
    if(!check(L"usertext",RGB(35,40,46)) || !check(L"firsttext",RGB(0,105,170)) || !check(L"secondtext",RGB(0,105,170)) || !check(L"[bracketed reply]",RGB(0,105,170)) || !check(L"thirdtext",RGB(0,105,170)) || check(L"[Command]",RGB(104,113,123)))return 9;
    wchar_t title[256]{};GetWindowTextW(ai,title,256);if(wcsncmp(title,L"AI Assistant - ",15))return 28;
    FINDTEXTEXW user{};user.chrg.cpMin=0;user.chrg.cpMax=-1;user.lpstrText=const_cast<wchar_t*>(L"usertext");
    if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&user))<0)return 19;
    CHARRANGE userSelection{user.chrgText.cpMin,user.chrgText.cpMin+1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&userSelection));
    PARAFORMAT2 para{};para.cbSize=sizeof(para);SendMessageW(edit,EM_GETPARAFORMAT,0,reinterpret_cast<LPARAM>(&para));if(para.wAlignment!=PFA_RIGHT)return 20;
    FINDTEXTEXW bold{};bold.chrg.cpMin=0;bold.chrg.cpMax=-1;bold.lpstrText=const_cast<wchar_t*>(L"boldtext");
    if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&bold))<0)return 11;
    CHARRANGE selection{bold.chrgText.cpMin,bold.chrgText.cpMin+1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&selection));
    CHARFORMAT2W fmt{};fmt.cbSize=sizeof(fmt);SendMessageW(edit,EM_GETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&fmt));if(!(fmt.dwEffects&CFE_BOLD))return 12;
    FINDTEXTEXW action{};action.chrg.cpMin=0;action.chrg.cpMax=-1;action.lpstrText=const_cast<wchar_t*>(L"Actions (1)");
    if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&action))<0)return 13;
    CHARRANGE actionSelection{action.chrgText.cpMin,action.chrgText.cpMin};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&actionSelection));SendMessageW(edit,EM_SCROLLCARET,0,0);
    POINTL point{};SendMessageW(edit,EM_POSFROMCHAR,reinterpret_cast<WPARAM>(&point),action.chrgText.cpMin);
    CHARRANGE linkSelection{action.chrgText.cpMin,action.chrgText.cpMin+1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&linkSelection));
    CHARFORMAT2W linkFormat{};linkFormat.cbSize=sizeof(linkFormat);SendMessageW(edit,EM_GETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&linkFormat));
    if(!(linkFormat.dwEffects&CFE_LINK)){std::printf("missing CFE_LINK: effects=%lu mask=%lu\n",linkFormat.dwEffects,linkFormat.dwMask);return 22;}
    SetFocus(edit);SendMessageW(edit,WM_MOUSEMOVE,0,MAKELPARAM(point.x+2,point.y+5));
    SendMessageW(edit,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x+2,point.y+5));SendMessageW(edit,WM_LBUTTONUP,0,MAKELPARAM(point.x+2,point.y+5));
    if(!check(L"[Command]",RGB(104,113,123))){std::printf("action click failed: x=%ld y=%ld start=%ld end=%ld mask=%lld\n",point.x,point.y,action.chrgText.cpMin,action.chrgText.cpMax,static_cast<long long>(SendMessageW(edit,EM_GETEVENTMASK,0,0)));return 14;}
    event("line",{L"AI: "});event("delta",{L"streamed **text"});event("delta",{L"**"});SendMessageW(ai,WM_TIMER,2,0);
    if(!check(L"streamed text",RGB(0,105,170)))return 10;
    CHARRANGE reading{0,4};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&reading));
    event("delta",{L" and **more"});event("delta",{L" text**"});SendMessageW(ai,WM_TIMER,2,0);
    CHARRANGE retained{};SendMessageW(edit,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&retained));
    if(retained.cpMin!=reading.cpMin || retained.cpMax!=reading.cpMax || !check(L"streamed text and more text",RGB(0,105,170)) || !check(L"[Command]",RGB(104,113,123)))return 34;
    FINDTEXTEXW streamedBold{};streamedBold.chrg.cpMin=0;streamedBold.chrg.cpMax=-1;streamedBold.lpstrText=const_cast<wchar_t*>(L"more text");
    if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&streamedBold))<0)return 36;
    CHARRANGE streamedSelection{streamedBold.chrgText.cpMin,streamedBold.chrgText.cpMin+1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&streamedSelection));
    CHARFORMAT2W streamedFormat{};streamedFormat.cbSize=sizeof(streamedFormat);SendMessageW(edit,EM_GETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&streamedFormat));if(!(streamedFormat.dwEffects&CFE_BOLD))return 37;
    RECT input{},model{},attachRect{},sendRect{};GetWindowRect(GetDlgItem(ai,107),&input);GetWindowRect(GetDlgItem(ai,110),&model);
    GetWindowRect(GetDlgItem(ai,119),&attachRect);GetWindowRect(GetDlgItem(ai,108),&sendRect);
    if(attachRect.left!=input.left || model.top<input.bottom || attachRect.right>model.left || model.right>sendRect.left || sendRect.right>input.right)return 15;
    int centerDifference=(model.top+model.bottom)-(sendRect.top+sendRect.bottom);
    if(centerDifference<-2 || centerDifference>2){std::printf("model and send centers differ: %d\n",centerDifference);return 50;}
    for(int width:{900,1200,1920}){
        SetWindowPos(ai,nullptr,0,0,width,700,SWP_NOZORDER|SWP_NOACTIVATE);
        GetWindowRect(GetDlgItem(ai,110),&model);GetWindowRect(GetDlgItem(ai,108),&sendRect);GetWindowRect(GetDlgItem(ai,107),&input);
        int difference=(model.top+model.bottom)-(sendRect.top+sendRect.bottom);
        if(difference<-2 || difference>2 || model.right>sendRect.left || sendRect.right>input.right)return 51;
    }
    for(int id:{108,109,119,120,121,122})if((GetWindowLongPtrW(GetDlgItem(ai,id),GWL_STYLE)&BS_TYPEMASK)!=BS_OWNERDRAW)return 26;
    event("clear");event("line",{L"AI: "});event("delta",{L"before drag"});SendMessageW(ai,WM_TIMER,2,0);
    SendMessageW(edit,WM_VSCROLL,MAKEWPARAM(SB_THUMBTRACK,0),0);
    event("delta",{L" deferred during drag"});SendMessageW(ai,WM_TIMER,2,0);
    FINDTEXTEXW deferred{};deferred.chrg.cpMin=0;deferred.chrg.cpMax=-1;deferred.lpstrText=const_cast<wchar_t*>(L"deferred during drag");
    if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&deferred))>=0)return 38;
    SendMessageW(edit,WM_VSCROLL,MAKEWPARAM(SB_ENDSCROLL,0),0);SendMessageW(ai,WM_TIMER,2,0);
    if(!check(L"deferred during drag",RGB(0,105,170)))return 39;
    event("clear");if(IsWindowEnabled(GetDlgItem(ai,123)))return 33;event("line",{L"AI: "});event("delta",{L"before action"});
    event("event",{L"MCP: sc2editor / first"});event("event",{L"MCP: sc2editor / second"});event("delta",{L"after action"});SendMessageW(ai,WM_TIMER,2,0);
    if(!check(L"before action",RGB(0,105,170)) || !check(L"after action",RGB(0,105,170)) || check(L"MCP: sc2editor / first",RGB(104,113,123)))return 24;
    FINDTEXTEXW grouped{};grouped.chrg.cpMin=0;grouped.chrg.cpMax=-1;grouped.lpstrText=const_cast<wchar_t*>(L"Actions (2)");
    if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&grouped))<0)return 25;
    event("history",{L"AI: ```galaxy\r\nprint(1);\r\n```"});
    FINDTEXTEXW copy{};copy.chrg.cpMin=0;copy.chrg.cpMax=-1;copy.lpstrText=const_cast<wchar_t*>(L"Copy code");
    if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&copy))<0)return 16;
    CHARRANGE copySelection{copy.chrgText.cpMin,copy.chrgText.cpMin};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&copySelection));SendMessageW(edit,EM_SCROLLCARET,0,0);
    POINTL copyPoint{};SendMessageW(edit,EM_POSFROMCHAR,reinterpret_cast<WPARAM>(&copyPoint),copy.chrgText.cpMin);
    SendMessageW(edit,WM_MOUSEMOVE,0,MAKELPARAM(copyPoint.x+2,copyPoint.y+5));
    SendMessageW(edit,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(copyPoint.x+2,copyPoint.y+5));SendMessageW(edit,WM_LBUTTONUP,0,MAKELPARAM(copyPoint.x+2,copyPoint.y+5));
    if(!OpenClipboard(ai))return 17;HANDLE clip=GetClipboardData(CF_UNICODETEXT);const wchar_t* copied=clip?static_cast<const wchar_t*>(GlobalLock(clip)):nullptr;
    bool copiedCode=copied && wcsstr(copied,L"print(1);")!=nullptr;if(copied)GlobalUnlock(clip);CloseClipboard();if(!copiedCode)return 18;
    std::wstring longHistory=L"AI: ";for(int i=0;i<120;++i)longHistory+=L"A long conversation line for wheel scrolling.\r\n";
    event("history",{longHistory});CHARRANGE endOfText{-1,-1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&endOfText));SendMessageW(edit,EM_SCROLLCARET,0,0);
    POINT wheelBefore{},wheelAfter{};SendMessageW(edit,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&wheelBefore));
    SetFocus(edit);RECT wheelRect{};GetWindowRect(edit,&wheelRect);
    SendMessageW(edit,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),MAKELPARAM(wheelRect.left+20,wheelRect.top+20));SendMessageW(edit,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&wheelAfter));
    UINT wheelLines=0;if(SystemParametersInfoW(SPI_GETWHEELSCROLLLINES,0,&wheelLines,0) && wheelLines && wheelBefore.y>0 && wheelAfter.y>=wheelBefore.y){SendMessageW(edit,WM_VSCROLL,SB_LINEUP,0);POINT line{};SendMessageW(edit,EM_GETSCROLLPOS,0,reinterpret_cast<LPARAM>(&line));std::printf("wheel scroll: before=%ld after=%ld line=%ld lines=%u\n",wheelBefore.y,wheelAfter.y,line.y,wheelLines);return 35;}
    // Typing a one-line prompt must not reformat the conversation control.
    TranscriptProbe transcriptProbe{};
    if(!SetWindowSubclass(edit,CountTranscriptChanges,77,reinterpret_cast<DWORD_PTR>(&transcriptProbe)))return 52;
    SetWindowTextW(composer,L"test");SendMessageW(ai,WM_COMMAND,MAKEWPARAM(107,EN_CHANGE),reinterpret_cast<LPARAM>(composer));
    if(transcriptProbe.rectChanges){std::printf("typing reformatted chat: %d\n",transcriptProbe.rectChanges);return 53;}
    SetWindowTextW(composer,L"");SendMessageW(ai,WM_COMMAND,MAKEWPARAM(107,EN_CHANGE),reinterpret_cast<LPARAM>(composer));
    CHARRANGE beginning{0,4};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&beginning));
    SendMessageW(edit,WM_VSCROLL,SB_TOP,0);SendMessageW(edit,EM_LINESCROLL,0,35);
    LRESULT firstLine=SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0);
    event("line",{L"You: a new question"});event("line",{L"AI: "});event("delta",{L"A new answer"});
    event("event",{L"MCP: first operation"});event("event",{L"MCP: second operation"});SendMessageW(ai,WM_TIMER,2,0);
    CHARRANGE kept{};SendMessageW(edit,EM_EXGETSEL,0,reinterpret_cast<LPARAM>(&kept));
    LRESULT afterLine=SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0);
    if(transcriptProbe.replaceAll || firstLine!=afterLine || kept.cpMin!=beginning.cpMin || kept.cpMax!=beginning.cpMax){
        std::printf("chat reset: replacements=%d first=%lld after=%lld selection=%ld,%ld\n",transcriptProbe.replaceAll,(long long)firstLine,(long long)afterLine,kept.cpMin,kept.cpMax);return 54;
    }
    FINDTEXTEXW newActions{};newActions.chrg.cpMin=0;newActions.chrg.cpMax=-1;newActions.lpstrText=const_cast<wchar_t*>(L"Actions (2)");
    if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&newActions))<0)return 55;
    SetWindowTextW(composer,L"one\r\ntwo\r\nthree\r\nfour");SendMessageW(ai,WM_COMMAND,MAKEWPARAM(107,EN_CHANGE),reinterpret_cast<LPARAM>(composer));
    if(SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0)!=firstLine){std::puts("resizing prompt moved conversation");return 57;}
    SetWindowTextW(composer,L"");SendMessageW(ai,WM_COMMAND,MAKEWPARAM(107,EN_CHANGE),reinterpret_cast<LPARAM>(composer));
    if(SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0)!=firstLine){std::puts("shrinking prompt moved conversation");return 58;}
    CHARRANGE newEnd{-1,-1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&newEnd));SendMessageW(edit,EM_SCROLLCARET,0,0);
    event("line",{L"AI: "});event("delta",{L"A follow-up reply"});SendMessageW(ai,WM_TIMER,2,0);
    SCROLLINFO bottomInfo{sizeof(bottomInfo),SIF_ALL};GetScrollInfo(edit,SB_VERT,&bottomInfo);
    if(bottomInfo.nPos+static_cast<int>(bottomInfo.nPage)<bottomInfo.nMax-3 || transcriptProbe.replaceAll){std::puts("new answer did not stay at the bottom");return 56;}
    // Rich Edit exposes only 16-bit pixel scroll coordinates; use line positions for long chats.
    std::wstring veryLong=L"AI: ";for(int i=0;i<5000;++i)veryLong+=L"A longer history line.\r\n";
    event("history",{veryLong});transcriptProbe.replaceAll=0;
    CHARRANGE longSelection{0,3};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&longSelection));
    SendMessageW(edit,WM_VSCROLL,SB_TOP,0);SendMessageW(edit,EM_LINESCROLL,0,4000);
    LRESULT deepLine=SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0);
    event("line",{L"You: test deep scroll"});event("line",{L"AI: "});event("delta",{L"new content"});SendMessageW(ai,WM_TIMER,2,0);
    if(deepLine<3000 || SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0)!=deepLine || transcriptProbe.replaceAll){std::puts("long history scroll was not preserved");return 59;}
    RemoveWindowSubclass(edit,CountTranscriptChanges,77);
    wchar_t previewPath[32768]{};DWORD previewLength=GetEnvironmentVariableW(L"SC2_CHAT_PREVIEW",previewPath,32768);
    if(previewLength && previewLength<32768){
        event("status",{L"Codex connected"});event("mcp",{L"SC2 MCP connected · map: Example.SC2Map"});
        event("models_begin");event("model",{L"test",L"GPT-6.1-Sol",L"1"});event("models_end");
        event("threads_begin");event("thread",{L"demo",L"Карта и триггеры",L"",L"sc2-ai:in-editor",L"IN_EDITOR",L"blue"});event("threads_end");event("selected",{L"demo"});event("ready",{L"1"});
        wchar_t modelName[128]{};GetWindowTextW(GetDlgItem(ai,110),modelName,128);if(lstrcmpW(modelName,L"GPT-6.1-Sol") || !IsWindowEnabled(GetDlgItem(ai,110)))return 23;
        event("history",{L"You: Покажи, как работает карта\r\n\r\nAI: Проверил открытую карту. **Доступные действия:**\r\n- Читать данные юнитов\r\n- Проверять настройки\r\n\r\n[MCP: sc2editor / modkit.tools — completed]\r\n\r\n[MCP: sc2editor / modkit.batch — completed]\r\n\r\nAI: Пример кода:\r\n```galaxy\r\nvoid Init() {\r\n    // Your map logic\r\n}\r\n```"});
        SetWindowTextW(GetDlgItem(ai,107),L"Напиши сообщение для AI...");
        SetWindowPos(ai,nullptr,0,0,1200,800,SWP_NOZORDER|SWP_NOACTIVATE);UpdateWindow(ai);
        if(!SavePreview(ai,previewPath))return 21;
    }
    DestroyWindow(ai);DestroyWindow(owner);CloseDesktop(desktop);CloseWindowStation(station);
    std::puts("PASS: formatted chat, collapsed actions, streaming colors and model placement");return 0;
}
