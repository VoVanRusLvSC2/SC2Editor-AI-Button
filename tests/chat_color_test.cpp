#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <commdlg.h>
#include <cstdio>
#include "../src/bridge_client.h"
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
static HWND ai;
static BOOL CALLBACK Find(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v33"))ai=h;return TRUE;}
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
    HWND edit=GetDlgItem(ai,105);
    LOGFONTW inputFont{};auto font=reinterpret_cast<HFONT>(SendMessageW(GetDlgItem(ai,107),WM_GETFONT,0,0));
    if(!font || GetObjectW(font,sizeof(inputFont),&inputFont)!=sizeof(inputFont) || inputFont.lfHeight>-15)return 27;
    auto check=[&](const wchar_t* text,COLORREF expected){
        FINDTEXTEXW find{};find.chrg.cpMin=0;find.chrg.cpMax=-1;find.lpstrText=const_cast<wchar_t*>(text);
        if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&find))<0)return false;
        CHARRANGE selection{find.chrgText.cpMin,find.chrgText.cpMin+1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&selection));
        CHARFORMAT2W format{};format.cbSize=sizeof(format);SendMessageW(edit,EM_GETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&format));return (format.dwMask&CFM_COLOR) && format.crTextColor==expected;
    };
    event("history",{L"You: usertext\r\n\r\nAI: firsttext **boldtext**\r\n\r\nsecondtext\r\n\r\n[bracketed reply]\r\n\r\n[Command]\r\n\r\nAI: thirdtext"});
    if(!check(L"usertext",RGB(35,40,46)) || !check(L"firsttext",RGB(0,105,170)) || !check(L"secondtext",RGB(0,105,170)) || !check(L"[bracketed reply]",RGB(0,105,170)) || !check(L"thirdtext",RGB(0,105,170)) || check(L"[Command]",RGB(104,113,123)))return 9;
    wchar_t title[256]{};GetWindowTextW(ai,title,256);if(wcsncmp(title,L"AI Assistant - ",15))return 28;
    RECT before{},after{};GetWindowRect(GetDlgItem(ai,113),&before);MapWindowPoints(HWND_DESKTOP,ai,reinterpret_cast<POINT*>(&before),2);
    int splitter=before.right+12,y=before.top+40;SendMessageW(ai,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(splitter,y));
    SendMessageW(ai,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(splitter+100,y));SendMessageW(ai,WM_LBUTTONUP,0,MAKELPARAM(splitter+100,y));
    GetWindowRect(GetDlgItem(ai,113),&after);MapWindowPoints(HWND_DESKTOP,ai,reinterpret_cast<POINT*>(&after),2);
    if(after.right-before.right!=100 || !check(L"firsttext",RGB(0,105,170)))return 29;
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
    RECT input{},model{},attachRect{},sendRect{};GetWindowRect(GetDlgItem(ai,107),&input);GetWindowRect(GetDlgItem(ai,110),&model);
    GetWindowRect(GetDlgItem(ai,119),&attachRect);GetWindowRect(GetDlgItem(ai,108),&sendRect);
    if(attachRect.left!=input.left || model.top<input.bottom || attachRect.right>model.left || model.right>sendRect.left || sendRect.right>input.right)return 15;
    for(int id:{108,109,119,120,121,122})if((GetWindowLongPtrW(GetDlgItem(ai,id),GWL_STYLE)&BS_TYPEMASK)!=BS_OWNERDRAW)return 26;
    event("clear");event("line",{L"AI: "});event("delta",{L"before action"});
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
