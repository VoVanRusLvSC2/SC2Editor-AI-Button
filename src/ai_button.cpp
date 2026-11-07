#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <shellapi.h>
#include <commdlg.h>
#include <oleacc.h>
#include <oleauto.h>
#include <new>
#include <cwchar>
#include <algorithm>
#include <cwctype>
#include <vector>
#include <string>
#include "bridge_client.h"
#include "map_context.h"
#include "image_attachment.h"
#include "editor_window.h"

namespace {
constexpr wchar_t kLegacyProperty1[] = L"SC2AIButton.context.v1";
constexpr wchar_t kLegacyProperty2[] = L"SC2AIButton.context.v2";
constexpr wchar_t kLegacyProperty3[] = L"SC2AIButton.context.v3";
constexpr wchar_t kLegacyProperty4[] = L"SC2AIButton.context.v4";
constexpr wchar_t kLegacyProperty5[] = L"SC2AIButton.context.v5";
constexpr wchar_t kLegacyProperty6[] = L"SC2AIButton.context.v6";
constexpr wchar_t kLegacyProperty7[] = L"SC2AIButton.context.v7";
constexpr wchar_t kLegacyProperty8[] = L"SC2AIButton.context.v8";
constexpr wchar_t kLegacyProperty9[] = L"SC2AIButton.context.v9";
constexpr wchar_t kLegacyProperty10[] = L"SC2AIButton.context.v10";
constexpr wchar_t kLegacyProperty11[] = L"SC2AIButton.context.v11";
constexpr wchar_t kLegacyProperty12[] = L"SC2AIButton.context.v12";
constexpr wchar_t kLegacyProperty13[] = L"SC2AIButton.context.v13";
constexpr wchar_t kLegacyProperty14[] = L"SC2AIButton.context.v14";
constexpr wchar_t kLegacyProperty15[] = L"SC2AIButton.context.v15";
constexpr wchar_t kLegacyProperty16[] = L"SC2AIButton.context.v16";
constexpr wchar_t kLegacyProperty17[] = L"SC2AIButton.context.v17";
constexpr wchar_t kLegacyProperty18[] = L"SC2AIButton.context.v18";
constexpr wchar_t kLegacyProperty19[] = L"SC2AIButton.context.v19";
constexpr wchar_t kLegacyProperty20[] = L"SC2AIButton.context.v20";
constexpr wchar_t kLegacyProperty21[] = L"SC2AIButton.context.v21";
constexpr wchar_t kLegacyProperty22[] = L"SC2AIButton.context.v22";
constexpr wchar_t kProperty[] = L"SC2AIButton.context.v23";
constexpr wchar_t kLegacyWindowClass1[] = L"SC2AIButton.Window.v1";
constexpr wchar_t kLegacyWindowClass2[] = L"SC2AIButton.Window.v2";
constexpr wchar_t kLegacyWindowClass3[] = L"SC2AIButton.Window.v3";
constexpr wchar_t kLegacyWindowClass4[] = L"SC2AIButton.Window.v4";
constexpr wchar_t kLegacyWindowClass5[] = L"SC2AIButton.Window.v5";
constexpr wchar_t kLegacyWindowClass6[] = L"SC2AIButton.Window.v6";
constexpr wchar_t kLegacyWindowClass7[] = L"SC2AIButton.Window.v7";
constexpr wchar_t kLegacyWindowClass8[] = L"SC2AIButton.Window.v8";
constexpr wchar_t kLegacyWindowClass9[] = L"SC2AIButton.Window.v9";
constexpr wchar_t kLegacyWindowClass10[] = L"SC2AIButton.Window.v10";
constexpr wchar_t kLegacyWindowClass11[] = L"SC2AIButton.Window.v11";
constexpr wchar_t kLegacyWindowClass12[] = L"SC2AIButton.Window.v12";
constexpr wchar_t kLegacyWindowClass13[] = L"SC2AIButton.Window.v13";
constexpr wchar_t kLegacyWindowClass14[] = L"SC2AIButton.Window.v14";
constexpr wchar_t kLegacyWindowClass15[] = L"SC2AIButton.Window.v15";
constexpr wchar_t kLegacyWindowClass16[] = L"SC2AIButton.Window.v16";
constexpr wchar_t kLegacyWindowClass17[] = L"SC2AIButton.Window.v17";
constexpr wchar_t kLegacyWindowClass18[] = L"SC2AIButton.Window.v18";
constexpr wchar_t kLegacyWindowClass19[] = L"SC2AIButton.Window.v19";
constexpr wchar_t kLegacyWindowClass20[] = L"SC2AIButton.Window.v20";
constexpr wchar_t kLegacyWindowClass21[] = L"SC2AIButton.Window.v21";
constexpr wchar_t kLegacyWindowClass22[] = L"SC2AIButton.Window.v22";
constexpr wchar_t kWindowClass[] = L"SC2AIButton.Window.v23";
constexpr size_t kMaxImages=5;
constexpr ULONGLONG kMaxImagesBytes=20ull*1024*1024;
constexpr UINT kCommand = 60000;
HMODULE moduleHandle = nullptr;
HMODULE richEditModule = nullptr;
HHOOK threadHook = nullptr;
UINT scanMessage = 0;
volatile LONG started = 0;
volatile LONG activeButtons = 0;
HWND currentAIWindow = nullptr;
bool selectedClaude = false;

struct ImageAttachment { std::wstring path; HBITMAP bitmap; ULONGLONG bytes; };
struct ChatItem { std::wstring id,title,group,name,color; };
struct Context {
    HWND toolbar; HWND owner; HWND window; UINT command;
    int legacy; unsigned legacyMask; bool ownsImages; HIMAGELIST images;
    TBBUTTONINFOW original;
    HWND aiToolbar, codex, claude, heading, status, description, history, input, send;
    HWND settings, model, config, showAll, showEditor, deleteChat; bool claudeSelected, allChats;
    int fileCommands[3];
    HIMAGELIST aiImages;
    BridgeState* bridge; HWND chatList,mcp; HFONT chatFont;
    std::vector<std::wstring> modelIds,chatRows,collapsedGroups;std::vector<ChatItem> chats;std::wstring selectedId;bool ready,busy,awaitingAnswer,pendingSend,windowActive,russian,rich;
    HWND attach,previews[kMaxImages],removes[kMaxImages];std::vector<ImageAttachment> attachments;
};
const wchar_t* Tr(Context* ctx,const wchar_t* ru,const wchar_t* en){return ctx->russian?ru:en;}
bool RussianEditor(HWND owner){
    HMENU menu=GetMenu(owner);wchar_t first[128]{};
    if(menu && GetMenuStringW(menu,0,first,128,MF_BYPOSITION)>0){
        std::wstring label(first);return label.find(L"Файл")!=std::wstring::npos || label.find(L"файл")!=std::wstring::npos;
    }
    return PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_RUSSIAN;
}

// Draw beveled steel-blue lettering at the toolbar's native image dimensions.
HBITMAP CreateAIIcon(int width,int height) {
    if (width<12 || height<12 || width>256 || height>256) return nullptr;
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr; HDC dc=CreateCompatibleDC(nullptr);
    if (!dc) return nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if (!bitmap || !bits) { if (bitmap) DeleteObject(bitmap); DeleteDC(dc); return nullptr; }
    ZeroMemory(bits,static_cast<SIZE_T>(width)*height*4);
    auto oldBitmap=SelectObject(dc,bitmap);
    int fontHeight=height*72/100;
    HFONT font=CreateFontW(-fontHeight,0,0,0,FW_HEAVY,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Georgia");
    if (!font) { SelectObject(dc,oldBitmap); DeleteObject(bitmap); DeleteDC(dc); return nullptr; }
    auto oldFont=SelectObject(dc,font); SetBkMode(dc,TRANSPARENT);
    RECT base{0,0,width,height};
    SetTextColor(dc,RGB(255,255,255));
    DrawTextW(dc,L"AI",2,&base,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    GdiFlush();
    auto pixels=static_cast<DWORD*>(bits);
    std::vector<unsigned char> original(static_cast<size_t>(width)*height);
    int left=width,top=height,right=-1,bottom=-1;
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        auto value=static_cast<unsigned char>(pixels[y*width+x]&255); original[y*width+x]=value;
        if (value>40) { if(x<left)left=x; if(x>right)right=x; if(y<top)top=y; if(y>bottom)bottom=y; }
    }
    std::vector<unsigned char> mask(static_cast<size_t>(width)*height);
    if (right>=left && bottom>=top) {
        int sourceW=right-left+1,sourceH=bottom-top+1;
        int targetH=height-6,targetW=sourceW*targetH*85/(sourceH*100);
        if(targetW>width-4)targetW=width-4;
        if(targetW<1)targetW=1;
        int x0=(width-targetW)/2,y0=(height-targetH)/2;
        for(int y=0;y<targetH;++y)for(int x=0;x<targetW;++x)
            mask[(y0+y)*width+x0+x]=original[(top+y*sourceH/targetH)*width+left+x*sourceW/targetW];
    }
    auto ink=[&](int x,int y){return x>=0 && y>=0 && x<width && y<height && mask[y*width+x]>40;};
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        bool inside=ink(x,y); bool edge=false;
        if (!inside) for (int dy=-1;dy<=1 && !edge;++dy) for (int dx=-1;dx<=1;++dx) if (ink(x+dx,y+dy)) { edge=true;break; }
        if (!inside && !edge) { pixels[y*width+x]=0;continue; }
        int r=5,g=17,b=26;
        if (inside) {
            int shade=(y*70)/(height-1);
            r=28+shade/3;g=91+shade;b=131+shade;
            if (!ink(x-1,y-1)) { r=164;g=210;b=225; }
            else if (!ink(x+1,y+1)) { r=12;g=47;b=69; }
            else if ((x*3+y*5)%13==0) { r+=30;g+=25;b+=20; }
        }
        pixels[y*width+x]=0xff000000u|(static_cast<DWORD>(r)<<16)|(static_cast<DWORD>(g)<<8)|static_cast<DWORD>(b);
    }
    SelectObject(dc,oldFont); DeleteObject(font); SelectObject(dc,oldBitmap); DeleteDC(dc);
    return bitmap;
}

// Build the lettering from the editor's own Text-module icon. Its pixel colors,
// bevel and outline then follow the installed editor's artwork and theme.
HBITMAP CreateNativeAIIcon(HWND toolbar, HIMAGELIST images, int insertion, int width, int height) {
    if (insertion<9 || width<16 || height<16 || width>256 || height>256) return nullptr;
    for (int i=insertion-9;i<insertion;++i) {
        TBBUTTON b{};
        if (!SendMessageW(toolbar,TB_GETBUTTON,i,reinterpret_cast<LPARAM>(&b)) || (b.fsStyle&BTNS_SEP)) return nullptr;
    }
    TBBUTTON text{};
    if (!SendMessageW(toolbar,TB_GETBUTTON,insertion-6,reinterpret_cast<LPARAM>(&text)) ||
        text.iBitmap<0 || text.iBitmap>=ImageList_GetImageCount(images)) return nullptr;
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr); if(!dc)return nullptr;
    void* bits=nullptr; HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap || !bits){if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return nullptr;}
    auto old=SelectObject(dc,bitmap); auto pixels=static_cast<DWORD*>(bits);
    constexpr DWORD background=0x00f0f0f0;
    for(int i=0;i<width*height;++i)pixels[i]=background;
    bool drawn=ImageList_Draw(images,text.iBitmap,dc,0,0,ILD_TRANSPARENT)!=FALSE;
    GdiFlush();
    std::vector<DWORD> source(pixels,pixels+width*height);
    int left=width,right=-1,top=height,bottom=-1;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)if((source[y*width+x]&0xffffff)!=background){
        if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;
    }
    if(!drawn || right<left || bottom<top || right-left+1>=width || bottom-top+1>=height){
        SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return nullptr;
    }
    for(int i=0;i<width*height;++i)pixels[i]=0;
    int sourceW=right-left+1,sourceH=bottom-top+1;
    int targetH=sourceH; if(targetH>height-4)targetH=height-4;
    int iWidth=width/4; if(iWidth<5)iWidth=5;
    int aWidth=sourceW*targetH/sourceH;
    if(aWidth>width-iWidth-5)aWidth=width-iWidth-5;
    if(aWidth<3){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return nullptr;}
    int x0=(width-aWidth-iWidth-1)/2,y0=(height-targetH)/2;
    for(int y=0;y<targetH;++y){
        int sy=top+y*sourceH/targetH;
        int runEnd=right;
        while(runEnd>=left && (source[sy*width+runEnd]&0xffffff)==background)--runEnd;
        int runStart=runEnd;
        while(runStart>left && (source[sy*width+runStart-1]&0xffffff)!=background)--runStart;
        for(int x=0;x<aWidth;++x){
            DWORD p=source[sy*width+left+x*sourceW/aWidth]&0xffffff;
            if(p!=background)pixels[(y0+y)*width+x0+x]=0xff000000|p;
        }
        // Copy each horizontal slice of the native A's right-hand stroke into
        // the I. This preserves the actual textured highlight and dark bevel,
        // including their changes from one row to the next.
        if(runEnd>=runStart){
            bool bar=y<2 || y>=targetH-2;
            int begin=bar?0:1, end=bar?iWidth:iWidth-1;
            int runWidth=runEnd-runStart+1, targetWidth=end-begin;
            for(int x=begin;x<end;++x){
                int sample=runStart+(x-begin)*runWidth/targetWidth;
                DWORD p=source[sy*width+sample]&0xffffff;
                pixels[(y0+y)*width+x0+aWidth+1+x]=0xff000000|p;
            }
        }
    }
    SelectObject(dc,old);DeleteDC(dc);return bitmap;
}

int AddAIIcon(HWND toolbar,Context* ctx,int insertion) {
    auto images=reinterpret_cast<HIMAGELIST>(SendMessageW(toolbar,TB_GETIMAGELIST,0,0));
    int width=0,height=0;
    if (!images) {
        images=ImageList_Create(24,24,ILC_COLOR32|ILC_MASK,0,1);
        if (!images) return -1;
        SendMessageW(toolbar,TB_SETIMAGELIST,0,reinterpret_cast<LPARAM>(images));
        ctx->ownsImages=true;
    }
    ctx->images=images;
    if (!ImageList_GetIconSize(images,&width,&height)) return -1;
    HBITMAP icon=CreateNativeAIIcon(toolbar,images,insertion,width,height);
    if (!icon) icon=CreateAIIcon(width,height);
    if (!icon) return -1;
    int index=ImageList_Add(images,icon,nullptr); DeleteObject(icon); return index;
}

void ReleaseOwnedImages(Context* ctx) {
    if (!ctx->ownsImages || !ctx->images) return;
    if (IsWindow(ctx->toolbar) && reinterpret_cast<HIMAGELIST>(SendMessageW(ctx->toolbar,TB_GETIMAGELIST,0,0))==ctx->images)
        SendMessageW(ctx->toolbar,TB_SETIMAGELIST,0,0);
    ImageList_Destroy(ctx->images); ctx->images=nullptr;
}

enum : int { kToolbar=100, kCodex=101, kClaude=102, kHeading=103,
             kStatus=104, kDescription=105, kHistory=106, kInput=107, kSend=108,
             kSettings=109, kModel=110, kChatList=113, kNewChat=114, kRefresh=115, kStop=116, kMcp=117, kLogin=118,
             kAttach=119,kShowAll=120,kShowEditor=121,kDeleteChat=122,kPreviewBase=130,kRemoveImageBase=140 };
constexpr wchar_t kEditorGroup[]=L"sc2-ai:in-editor";

HWND Control(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int id) {
    HWND child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,
        parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),moduleHandle,nullptr);
    if(child) SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    return child;
}

void SelectProvider(Context* ctx,bool claude) {
    ctx->claudeSelected=claude;selectedClaude=claude;
    if(IsWindow(ctx->codex))SendMessageW(ctx->codex,BM_SETCHECK,claude?BST_UNCHECKED:BST_CHECKED,0);
    if(IsWindow(ctx->claude))SendMessageW(ctx->claude,BM_SETCHECK,claude?BST_CHECKED:BST_UNCHECKED,0);
    if(IsWindow(ctx->settings)){
        SetWindowTextW(GetDlgItem(ctx->settings,kLogin),claude?Tr(ctx,L"Открыть Claude Console",L"Open Claude Console"):Tr(ctx,L"Войти в Codex",L"Sign in to Codex"));
        SetWindowTextW(GetDlgItem(ctx->settings,112),claude?Tr(ctx,L"Войдите в Console, создайте ключ ANTHROPIC_API_KEY в переменных среды Windows и перезапустите редактор. API оплачивается отдельно.",L"Sign in to Console, set ANTHROPIC_API_KEY in Windows environment variables and restart the Editor. API is billed separately."):Tr(ctx,L"Вход откроется в браузере. Используйте свой аккаунт ChatGPT.",L"Sign in in your browser with your ChatGPT account."));
    }
    SetWindowTextW(ctx->heading,claude?L"Claude":L"Codex");
}
LRESULT CALLBACK SettingsProc(HWND window,UINT msg,WPARAM w,LPARAM l){
    auto ctx=reinterpret_cast<Context*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_CREATE){
        auto cs=reinterpret_cast<CREATESTRUCTW*>(l);ctx=static_cast<Context*>(cs->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ctx));ctx->settings=window;
        HWND label=Control(window,L"STATIC",Tr(ctx,L"Выберите помощника:",L"Choose an assistant:"),SS_LEFT,111);
        ctx->codex=Control(window,L"BUTTON",L"Codex",BS_AUTORADIOBUTTON|WS_GROUP,kCodex);
        ctx->claude=Control(window,L"BUTTON",L"Claude",BS_AUTORADIOBUTTON,kClaude);
        HWND note=Control(window,L"STATIC",L"",SS_LEFT,112);
        HWND login=Control(window,L"BUTTON",L"",BS_PUSHBUTTON,kLogin);
        if(!label||!ctx->codex||!ctx->claude||!note||!login)return -1;
        MoveWindow(label,20,20,330,24,TRUE);MoveWindow(ctx->codex,20,54,150,26,TRUE);
        MoveWindow(ctx->claude,20,86,150,26,TRUE);MoveWindow(login,20,123,220,30,TRUE);MoveWindow(note,20,166,340,65,TRUE);
        SelectProvider(ctx,ctx->claudeSelected);return 0;
    }
    if(msg==WM_COMMAND && ctx && LOWORD(w)==kLogin){
        if(ctx->claudeSelected){
            if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",L"https://platform.claude.com/settings/keys",nullptr,nullptr,SW_SHOWNORMAL))<=32)
                MessageBoxW(window,Tr(ctx,L"Не удалось открыть браузер.",L"Could not open the browser."),L"Claude",MB_OK|MB_ICONERROR);
        }else if(!SendBridge(ctx->bridge,"login",{}))MessageBoxW(window,Tr(ctx,L"Нет связи с Codex.",L"Codex is disconnected."),L"Codex",MB_OK|MB_ICONERROR);
        return 0;
    }
    if(msg==WM_COMMAND && ctx && (LOWORD(w)==kCodex || LOWORD(w)==kClaude)){
        bool chosen=LOWORD(w)==kClaude;
        if(ctx->busy){MessageBoxW(window,Tr(ctx,L"Дождитесь завершения ответа или остановите его.",L"Wait for the response or stop it first."),L"AI",MB_OK|MB_ICONINFORMATION);SelectProvider(ctx,ctx->claudeSelected);return 0;}
        SelectProvider(ctx,chosen);ctx->ready=false;EnableWindow(ctx->send,FALSE);EnableWindow(ctx->input,FALSE);
        if(!SendBridge(ctx->bridge,"provider",{chosen?L"claude":L"codex"}))SetWindowTextW(ctx->status,Tr(ctx,L"Ошибка связи",L"Connection error"));
        return 0;
    }
    if(msg==WM_CLOSE){DestroyWindow(window);return 0;}
    if(msg==WM_NCDESTROY && ctx){ctx->settings=nullptr;ctx->codex=nullptr;ctx->claude=nullptr;}
    return DefWindowProcW(window,msg,w,l);
}
void OpenSettings(Context* ctx,HWND owner){
    if(IsWindow(ctx->settings)){SetForegroundWindow(ctx->settings);return;}
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=SettingsProc;wc.hInstance=moduleHandle;
    wc.lpszClassName=L"SC2AIButton.Settings.v23";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;
    RECT r{};GetWindowRect(owner,&r);
    CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,Tr(ctx,L"Настройки AI",L"AI Settings"),WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        r.left+40,r.top+40,400,280,owner,nullptr,moduleHandle,ctx);
}

void FindFileCommands(Context* ctx){
    for(auto& command:ctx->fileCommands)command=-1;
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    IAccessible* accessible=nullptr;
    if(SUCCEEDED(AccessibleObjectFromWindow(ctx->toolbar,static_cast<DWORD>(OBJID_CLIENT),IID_IAccessible,reinterpret_cast<void**>(&accessible))) && accessible){
        const wchar_t* shortcuts[]={L"(ctrl+n)",L"(ctrl+o)",L"(ctrl+s)"};
        int count=static_cast<int>(SendMessageW(ctx->toolbar,TB_BUTTONCOUNT,0,0));
        for(int i=0;i<count && i<150;++i){
            TBBUTTON button{};if(!SendMessageW(ctx->toolbar,TB_GETBUTTON,i,reinterpret_cast<LPARAM>(&button)) || (button.fsStyle&BTNS_SEP))continue;
            VARIANT child{};child.vt=VT_I4;child.lVal=i+1;BSTR name=nullptr;
            if(SUCCEEDED(accessible->get_accName(child,&name)) && name){
                std::wstring text(name,SysStringLen(name));SysFreeString(name);
                if(!text.empty())CharLowerBuffW(text.data(),static_cast<DWORD>(text.size()));
                for(int j=0;j<3;++j)if(text.find(shortcuts[j])!=std::wstring::npos){
                    // Ambiguous accelerator labels are never forwarded.
                    ctx->fileCommands[j]=ctx->fileCommands[j]==-1?button.idCommand:-2;
                }
            }
        }
        accessible->Release();
    }
    if(SUCCEEDED(initialized))CoUninitialize();
}
bool IsFileCommand(const Context* ctx,int command){for(int id:ctx->fileCommands)if(id>=0 && id==command)return true;return false;}
void SyncFileCommands(Context* ctx){
    if(!IsWindow(ctx->aiToolbar))return;
    for(int command:ctx->fileCommands)if(command>=0){
        LRESULT state=IsWindow(ctx->toolbar)?SendMessageW(ctx->toolbar,TB_GETSTATE,command,0):-1;
        SendMessageW(ctx->aiToolbar,TB_ENABLEBUTTON,command,MAKELONG(state!=-1 && (state&TBSTATE_ENABLED),0));
    }
}
void SyncEditorTitle(Context* ctx){
    if(!IsWindow(ctx->owner) || !IsWindow(ctx->window))return;
    wchar_t ownerTitle[1024]{},current[1100]{};GetWindowTextW(ctx->owner,ownerTitle,1024);GetWindowTextW(ctx->window,current,1100);
    const wchar_t* suffix=wcsstr(ownerTitle,L" - ");
    const std::wstring desired=std::wstring(L"AI")+(suffix?suffix:L" - StarCraft II Editor");
    if(desired!=current)SetWindowTextW(ctx->window,desired.c_str());
}
bool CopyToolbar(Context* ctx,HWND window) {
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT|CCS_TOP|CCS_NODIVIDER,
        0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolbar)),moduleHandle,nullptr);
    if(!bar)return false;
    ctx->aiToolbar=bar;
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
    HIMAGELIST source=reinterpret_cast<HIMAGELIST>(SendMessageW(ctx->toolbar,TB_GETIMAGELIST,0,0));
    if(source){ctx->aiImages=ImageList_Duplicate(source);if(!ctx->aiImages)return false;
        SendMessageW(bar,TB_SETIMAGELIST,0,reinterpret_cast<LPARAM>(ctx->aiImages));}
    int count=static_cast<int>(SendMessageW(ctx->toolbar,TB_BUTTONCOUNT,0,0));
    int ai=static_cast<int>(SendMessageW(ctx->toolbar,TB_COMMANDTOINDEX,ctx->command,0));
    if(count<1 || count>150 || ai<0)return false;
    FindFileCommands(ctx);
    RECT nativeButton{};
    int buttonWidth=40,buttonHeight=40;
    if(SendMessageW(ctx->toolbar,TB_GETITEMRECT,ai,reinterpret_cast<LPARAM>(&nativeButton))){
        int w=nativeButton.right-nativeButton.left,h=nativeButton.bottom-nativeButton.top;
        if(w>=20 && w<=80)buttonWidth=w;if(h>=20 && h<=80)buttonHeight=h;
    }
    SendMessageW(bar,TB_SETBUTTONSIZE,0,MAKELPARAM(buttonWidth,buttonHeight));
    for(int i=0;i<count;++i){
        TBBUTTON b{};if(!SendMessageW(ctx->toolbar,TB_GETBUTTON,i,reinterpret_cast<LPARAM>(&b)))return false;
        if(b.fsStyle&BTNS_SEP)continue;
        // Keep file commands and module navigation available in the AI workspace.
        b.iString=-1;b.dwData=0;b.fsStyle&=static_cast<BYTE>(~(BTNS_AUTOSIZE|BTNS_SHOWTEXT));b.fsState&=static_cast<BYTE>(~(TBSTATE_CHECKED|TBSTATE_PRESSED));
        if((i<ai-9 || i>ai) && !IsFileCommand(ctx,b.idCommand)) b.fsState&=static_cast<BYTE>(~TBSTATE_ENABLED);
        if(i==ai){b.fsStyle=BTNS_CHECK;b.fsState|=TBSTATE_ENABLED;}
        if(!SendMessageW(bar,TB_ADDBUTTONSW,1,reinterpret_cast<LPARAM>(&b)))return false;
    }
    SendMessageW(bar,TB_AUTOSIZE,0,0);SyncFileCommands(ctx);return true;
}
void LayoutControls(Context* ctx,HWND window) {
    RECT r{};GetClientRect(window,&r);int width=r.right,height=r.bottom;
    SendMessageW(ctx->aiToolbar,TB_AUTOSIZE,0,0);
    RECT bar{};GetWindowRect(ctx->aiToolbar,&bar);int top=bar.bottom-bar.top+16;
    int side=width<1100?220:280, sidebarX=18, mainX=side+22, mainW=width-mainX-22;
    if(mainW<160)mainW=160;
    MoveWindow(ctx->heading,mainX,top,90,24,TRUE);MoveWindow(ctx->status,mainX+100,top,190,24,TRUE);
    int modelWidth=mainW-430;if(modelWidth>200)modelWidth=200;if(modelWidth<90)modelWidth=90;
    MoveWindow(ctx->model,mainX+290,top-4,modelWidth,200,TRUE);
    MoveWindow(ctx->config,mainX+mainW-115,top-4,115,30,TRUE);
    MoveWindow(ctx->mcp,mainX,top+32,mainW,22,TRUE);
    int inputY=height-80;if(inputY<top+160)inputY=top+160;
    int attachmentSpace=ctx->attachments.empty()?0:64;
    MoveWindow(ctx->description,mainX,top+60,mainW,inputY-top-72-attachmentSpace,TRUE);
    bool narrow=side<250;int allX=sidebarX+(narrow?40:48),allW=narrow?32:38,editorX=allX+allW+2,editorW=narrow?72:86,deleteX=editorX+editorW+2;
    MoveWindow(ctx->history,sidebarX,top,narrow?38:44,24,TRUE);
    MoveWindow(ctx->showAll,allX,top-4,allW,26,TRUE);
    MoveWindow(ctx->showEditor,editorX,top-4,editorW,26,TRUE);
    MoveWindow(ctx->deleteChat,deleteX,top-4,narrow?38:50,26,TRUE);
    MoveWindow(ctx->chatList,sidebarX,top+30,side-30,height-top-44,TRUE);
    MoveWindow(ctx->attach,mainX,inputY,86,32,TRUE);
    MoveWindow(ctx->input,mainX+94,inputY,mainW>215?mainW-206:mainW,52,TRUE);
    MoveWindow(ctx->send,mainX+mainW-104,inputY,104,32,TRUE);
    for(size_t i=0;i<kMaxImages;++i){
        int x=mainX+static_cast<int>(i)*102;
        MoveWindow(ctx->previews[i],x,inputY-60,72,54,TRUE);
        MoveWindow(ctx->removes[i],x+74,inputY-60,24,24,TRUE);
        ShowWindow(ctx->previews[i],i<ctx->attachments.size()?SW_SHOW:SW_HIDE);
        ShowWindow(ctx->removes[i],i<ctx->attachments.size()?SW_SHOW:SW_HIDE);
    }
}
void RefreshImages(Context* ctx){
    for(size_t i=0;i<kMaxImages;++i)if(ctx->previews[i])SendMessageW(ctx->previews[i],STM_SETIMAGE,IMAGE_BITMAP,i<ctx->attachments.size()?reinterpret_cast<LPARAM>(ctx->attachments[i].bitmap):0);
    if(IsWindow(ctx->window))LayoutControls(ctx,ctx->window);
}
void ClearImages(Context* ctx,bool discard){
    for(auto preview:ctx->previews)if(preview)SendMessageW(preview,STM_SETIMAGE,IMAGE_BITMAP,0);
    for(auto& image:ctx->attachments){if(discard)DeleteFileW(image.path.c_str());DeleteObject(image.bitmap);}
    ctx->attachments.clear();RefreshImages(ctx);
}
void RemoveImage(Context* ctx,size_t index){
    if(index>=ctx->attachments.size())return;
    for(auto preview:ctx->previews)SendMessageW(preview,STM_SETIMAGE,IMAGE_BITMAP,0);
    auto& image=ctx->attachments[index];DeleteFileW(image.path.c_str());DeleteObject(image.bitmap);
    ctx->attachments.erase(ctx->attachments.begin()+index);RefreshImages(ctx);
}
bool AddImage(Context* ctx,const std::wstring& path,HBITMAP bitmap){
    WIN32_FILE_ATTRIBUTE_DATA data{};ULONGLONG size=0;
    if(GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&data))size=(static_cast<ULONGLONG>(data.nFileSizeHigh)<<32)|data.nFileSizeLow;
    ULONGLONG total=size;for(const auto& image:ctx->attachments)total+=image.bytes;
    if(!size || ctx->attachments.size()>=kMaxImages || total>kMaxImagesBytes){
        DeleteFileW(path.c_str());DeleteObject(bitmap);
        MessageBoxW(ctx->window,Tr(ctx,L"Можно прикрепить до 5 изображений общим размером до 20 МБ.",L"You can attach up to 5 images with a combined size of 20 MB."),Tr(ctx,L"Изображения",L"Images"),MB_OK|MB_ICONWARNING);return false;
    }
    ctx->attachments.push_back({path,bitmap,size});RefreshImages(ctx);return true;
}
void ImageError(Context* ctx){MessageBoxW(ctx->window,Tr(ctx,L"Не удалось добавить изображение. Используйте PNG, JPEG, GIF или BMP размером до 7 МБ после преобразования.",L"Could not attach the image. Use PNG, JPEG, GIF, or BMP up to 7 MB after conversion."),Tr(ctx,L"Изображение",L"Image"),MB_OK|MB_ICONERROR);}
LRESULT CALLBACK InputProc(HWND input,UINT msg,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR raw){
    auto ctx=reinterpret_cast<Context*>(raw);
    if(msg==WM_CHAR && w==L'\r' && !(GetKeyState(VK_SHIFT)&0x8000)){
        // The button also stops a running response. Enter must never stop it.
        if(ctx && ctx->ready && (!ctx->busy || ctx->awaitingAnswer) && IsWindowEnabled(ctx->send) && !(l & (static_cast<LPARAM>(1)<<30)))
            SendMessageW(ctx->window,WM_COMMAND,MAKEWPARAM(kSend,BN_CLICKED),reinterpret_cast<LPARAM>(ctx->send));
        return 0;
    }
    if(msg==WM_PASTE && ctx && !ctx->busy && (IsClipboardFormatAvailable(CF_BITMAP) || IsClipboardFormatAvailable(CF_HDROP))){
        if(!IsClipboardFormatAvailable(CF_BITMAP) && IsClipboardFormatAvailable(CF_HDROP)){
            std::vector<std::wstring> files;if(!ClipboardImageFiles(ctx->window,files)){ImageError(ctx);return 0;}
            for(const auto& file:files){std::wstring path;HBITMAP bitmap=nullptr;if(!ImageFromFile(file,path,bitmap)){ImageError(ctx);break;}if(!AddImage(ctx,path,bitmap))break;}
        }else{std::wstring path;HBITMAP bitmap=nullptr;if(ImageFromClipboard(ctx->window,path,bitmap))AddImage(ctx,path,bitmap);else ImageError(ctx);}return 0;
    }
    if(msg==WM_NCDESTROY)RemoveWindowSubclass(input,InputProc,id);
    return DefSubclassProc(input,msg,w,l);
}
void ChooseImage(Context* ctx){
    wchar_t file[32768]{};OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=ctx->window;
    dialog.lpstrFilter=Tr(ctx,L"Изображения (*.png;*.jpg;*.jpeg;*.gif;*.bmp)\0*.png;*.jpg;*.jpeg;*.gif;*.bmp\0Все файлы\0*.*\0\0",L"Images (*.png;*.jpg;*.jpeg;*.gif;*.bmp)\0*.png;*.jpg;*.jpeg;*.gif;*.bmp\0All files\0*.*\0\0");
    dialog.lpstrFile=file;dialog.nMaxFile=32768;dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER|OFN_ALLOWMULTISELECT;
    if(GetOpenFileNameW(&dialog)){
        std::wstring first(file);const wchar_t* next=file+first.size()+1;
        auto add=[&](const std::wstring& source){std::wstring path;HBITMAP bitmap=nullptr;if(!ImageFromFile(source,path,bitmap)){ImageError(ctx);return false;}return AddImage(ctx,path,bitmap);};
        if(!*next)add(first);else for(;*next;next+=wcslen(next)+1)if(!add(first+L"\\"+next))break;
    }
}
std::wstring Workspace(HWND owner){
    auto file=ReadMapContext(owner).path;size_t slash=file.find_last_of(L"\\/");
    return slash==std::wstring::npos?std::wstring{}:file.substr(0,slash);
}
void SendMapContext(Context* ctx){
    auto map=ReadMapContext(ctx->owner);
    SendBridge(ctx->bridge,"map",{map.path,map.dirty?L"1":L"0",map.caption,std::to_wstring(reinterpret_cast<UINT_PTR>(ctx->owner)),std::to_wstring(GetCurrentProcessId()),map.inferred?L"1":L"0"});
}
void Append(Context* ctx,const std::wstring& text,COLORREF color=RGB(35,40,46),bool bold=false){
    HWND edit=ctx->description;if(!edit || text.empty())return;int len=GetWindowTextLengthW(edit);SendMessageW(edit,EM_SETSEL,len,len);
    if(ctx->rich){CHARFORMAT2W fmt{};fmt.cbSize=sizeof(fmt);fmt.dwMask=CFM_COLOR|CFM_BOLD|CFM_SIZE|CFM_FACE;fmt.dwEffects=bold?CFE_BOLD:0;fmt.crTextColor=color;fmt.yHeight=220;wcscpy_s(fmt.szFaceName,L"Segoe UI");SendMessageW(edit,EM_SETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&fmt));}
    SendMessageW(edit,EM_REPLACESEL,FALSE,reinterpret_cast<LPARAM>(text.c_str()));SendMessageW(edit,EM_SCROLLCARET,0,0);
}
constexpr COLORREF kAIColor=RGB(0,105,170);
void AppendEntry(Context* ctx,const std::wstring& text,bool aiContinuation=false){
    if(text.empty())return;
    Append(ctx,L"\r\n");
    size_t prefix=0;COLORREF color=RGB(28,83,132);bool ai=aiContinuation;
    if(text.rfind(L"Вы: ",0)==0)prefix=4;
    else if(text.rfind(L"You: ",0)==0)prefix=5;
    else if(text.rfind(L"AI: ",0)==0){prefix=4;ai=true;}
    else if(text==L"AI:"){prefix=3;ai=true;}
    else if(text[0]==L'[' && !aiContinuation){Append(ctx,text,RGB(104,113,123));Append(ctx,L"\r\n");return;}
    if(prefix){Append(ctx,text.substr(0,prefix),ai?kAIColor:color,true);Append(ctx,text.substr(prefix),ai?kAIColor:RGB(35,40,46));}
    else Append(ctx,text,ai?kAIColor:RGB(35,40,46));
    Append(ctx,L"\r\n");
}
void RenderHistory(Context* ctx,const std::wstring& history){
    SetWindowTextW(ctx->description,L"");size_t start=0;bool ai=false;
    while(start<history.size()){
        size_t end=history.find(L"\r\n\r\n",start);auto entry=history.substr(start,end==std::wstring::npos?end:end-start);
        if(entry.rfind(L"AI:",0)==0)ai=true;
        else if(entry.rfind(L"Вы: ",0)==0 || entry.rfind(L"You: ",0)==0 || entry.rfind(L"[MCP:",0)==0 || entry.rfind(L"[Команда",0)==0 || entry.rfind(L"[Command",0)==0 || entry.rfind(L"[Изменения файлов",0)==0 || entry.rfind(L"[File changes",0)==0)ai=false;
        AppendEntry(ctx,entry,ai);if(end==std::wstring::npos)break;start=end+4;
    }
    SendMessageW(ctx->description,WM_VSCROLL,SB_BOTTOM,0);
}
void UpdateControls(Context* ctx){
    EnableWindow(ctx->input,ctx->ready && (!ctx->busy || ctx->awaitingAnswer));EnableWindow(ctx->send,ctx->ready || ctx->busy);
    SetWindowTextW(ctx->send,ctx->awaitingAnswer?Tr(ctx,L"Ответить",L"Reply"):(ctx->busy?Tr(ctx,L"Остановить",L"Stop"):Tr(ctx,L"Отправить",L"Send")));
    EnableWindow(ctx->model,ctx->ready && !ctx->busy && !ctx->modelIds.empty());
    EnableWindow(ctx->chatList,ctx->ready && !ctx->busy);
    EnableWindow(ctx->showAll,ctx->ready && !ctx->busy);EnableWindow(ctx->showEditor,ctx->ready && !ctx->busy);
    int row=static_cast<int>(SendMessageW(ctx->chatList,LB_GETCURSEL,0,0));
    bool selectedChat=row>=0 && static_cast<size_t>(row)<ctx->chatRows.size() && !ctx->selectedId.empty() && ctx->chatRows[row]==L"t"+ctx->selectedId;
    EnableWindow(ctx->deleteChat,ctx->ready && !ctx->busy && selectedChat);
    EnableWindow(ctx->attach,ctx->ready && !ctx->busy);for(auto button:ctx->removes)EnableWindow(button,ctx->ready && !ctx->busy);
}
void SelectChatRow(Context* ctx){
    int selected=ctx->selectedId.empty()?0:-1;
    for(size_t i=1;i<ctx->chatRows.size();++i)if(ctx->chatRows[i]==L"t"+ctx->selectedId){selected=static_cast<int>(i);break;}
    SendMessageW(ctx->chatList,LB_SETCURSEL,selected,0);
}
void RenderChats(Context* ctx){
    SendMessageW(ctx->chatList,LB_RESETCONTENT,0,0);ctx->chatRows.clear();
    auto add=[&](const std::wstring& text,const std::wstring& key){if(SendMessageW(ctx->chatList,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()))>=0)ctx->chatRows.push_back(key);};
    add(Tr(ctx,L"＋ Новый чат",L"＋ New chat"),L"n");
    std::vector<std::wstring> groups{kEditorGroup};
    for(const auto& chat:ctx->chats)if((ctx->allChats || chat.group==kEditorGroup) && std::find(groups.begin(),groups.end(),chat.group)==groups.end())groups.push_back(chat.group);
    for(const auto& group:groups){
        auto first=std::find_if(ctx->chats.begin(),ctx->chats.end(),[&](const ChatItem& chat){return chat.group==group;});
        bool collapsed=std::find(ctx->collapsedGroups.begin(),ctx->collapsedGroups.end(),group)!=ctx->collapsedGroups.end();
        auto title=group==kEditorGroup?std::wstring(L"IN_EDITOR"):(first->name.empty()?std::wstring(Tr(ctx,L"Другие чаты",L"Other chats")):first->name);
        add(std::wstring(collapsed?L"▸ ":L"▾ ")+title,L"g"+group);
        if(!collapsed)for(const auto& chat:ctx->chats)if(chat.group==group)add(L"    "+chat.title,L"t"+chat.id);
    }
    SelectChatRow(ctx);InvalidateRect(ctx->chatList,nullptr,TRUE);UpdateControls(ctx);
}
COLORREF FolderColor(const std::wstring& color){
    if(color==L"blue")return RGB(51,156,255);
    if(color==L"green")return RGB(0,162,64);
    if(color==L"orange")return RGB(226,85,7);
    if(color==L"pink")return RGB(225,75,135);
    if(color==L"purple")return RGB(146,79,247);
    if(color==L"red")return RGB(224,46,42);
    if(color==L"yellow")return RGB(220,165,0);
    return GetSysColor(COLOR_WINDOWTEXT);
}
void DrawChatItem(Context* ctx,const DRAWITEMSTRUCT* item){
    if(item->itemID==static_cast<UINT>(-1) || item->itemID>=ctx->chatRows.size())return;
    HDC dc=item->hDC;RECT row=item->rcItem;bool selected=(item->itemState&ODS_SELECTED)!=0;
    FillRect(dc,&row,GetSysColorBrush(selected?COLOR_HIGHLIGHT:COLOR_WINDOW));
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,GetSysColor(selected?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT));
    HFONT font=reinterpret_cast<HFONT>(SendMessageW(ctx->chatList,WM_GETFONT,0,0));auto old=font?SelectObject(dc,font):nullptr;
    LRESULT n=SendMessageW(ctx->chatList,LB_GETTEXTLEN,item->itemID,0);
    if(n>=0 && n<32000){
        std::wstring title(static_cast<size_t>(n)+1,L'\0');SendMessageW(ctx->chatList,LB_GETTEXT,item->itemID,reinterpret_cast<LPARAM>(title.data()));title.resize(static_cast<size_t>(n));
        const auto& key=ctx->chatRows[item->itemID];
        if(!key.empty() && key[0]==L'g' && key.size()>1){
            RECT arrow=row;arrow.left+=2;arrow.right=arrow.left+16;DrawTextW(dc,title.c_str(),1,&arrow,DT_SINGLELINE|DT_VCENTER|DT_CENTER|DT_NOPREFIX);
            auto it=std::find_if(ctx->chats.begin(),ctx->chats.end(),[&](const ChatItem& chat){return chat.group==key.substr(1);});
            COLORREF color=it==ctx->chats.end()?GetSysColor(COLOR_WINDOWTEXT):FolderColor(it->color);
            int x=row.left+20,y=(row.top+row.bottom)/2-5;
            HBRUSH brush=CreateSolidBrush(color);HPEN pen=CreatePen(PS_SOLID,1,color);
            if(brush && pen){auto oldBrush=SelectObject(dc,brush);auto oldPen=SelectObject(dc,pen);POINT points[]={{x,y+1},{x,y-1},{x+5,y-1},{x+7,y+1},{x+14,y+1},{x+14,y+9},{x,y+9}};Polygon(dc,points,7);SelectObject(dc,oldPen);SelectObject(dc,oldBrush);}
            if(pen)DeleteObject(pen);if(brush)DeleteObject(brush);
            row.left+=39;if(title.size()>2)title.erase(0,2);
        }else row.left+=3;
        row.right-=3;DrawTextW(dc,title.c_str(),static_cast<int>(title.size()),&row,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
    }
    if(old)SelectObject(dc,old);if(item->itemState&ODS_FOCUS)DrawFocusRect(dc,&item->rcItem);
}
void OnBridge(Context* ctx,const BridgeEvent& e){
    if(e.source!=ctx->bridge)return;
    auto value=[&](size_t n)->std::wstring{return n<e.fields.size()?e.fields[n]:L"";};
    if(e.kind=="login_url"){
        const std::wstring url=value(0);
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(ctx->window,L"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
            MessageBoxW(ctx->window,Tr(ctx,L"Не удалось открыть браузер для входа.",L"Could not open the browser for sign-in."),L"Codex",MB_OK|MB_ICONERROR);
    }
    else if(e.kind=="status")SetWindowTextW(ctx->status,value(0).c_str());
    else if(e.kind=="mcp")SetWindowTextW(ctx->mcp,value(0).c_str());
    else if(e.kind=="error"){Append(ctx,L"\r\n"+std::wstring(Tr(ctx,L"[Ошибка] ",L"[Error] "))+value(0)+L"\r\n",RGB(155,46,46));SetWindowTextW(ctx->status,Tr(ctx,L"Ошибка",L"Error"));}
    else if(e.kind=="ready"){ctx->ready=value(0)==L"1";UpdateControls(ctx);}
    else if(e.kind=="busy"){ctx->busy=value(0)==L"1";if(!ctx->busy){ctx->awaitingAnswer=false;ctx->pendingSend=false;}UpdateControls(ctx);}
    else if(e.kind=="accepted" && ctx->pendingSend){SetWindowTextW(ctx->input,L"");ClearImages(ctx,false);ctx->pendingSend=false;}
    else if(e.kind=="question"){ctx->awaitingAnswer=true;Append(ctx,L"\r\n"+std::wstring(Tr(ctx,L"[Вопрос AI] ",L"[AI question] "))+value(0)+L"\r\n",RGB(155,95,30));UpdateControls(ctx);SetFocus(ctx->input);}
    else if(e.kind=="clear")SetWindowTextW(ctx->description,L"");
    else if(e.kind=="history")RenderHistory(ctx,value(0));
    else if(e.kind=="line")AppendEntry(ctx,value(0));
    else if(e.kind=="delta")Append(ctx,value(0),kAIColor);
    else if(e.kind=="event")Append(ctx,L"\r\n["+value(0)+L"]\r\n",RGB(104,113,123));
    else if(e.kind=="threads_begin"){ctx->chats.clear();RenderChats(ctx);}
    else if(e.kind=="thread")ctx->chats.push_back({value(0),value(1),value(3),value(4),value(5)});
    else if(e.kind=="threads_end"){RenderChats(ctx);if(!value(0).empty())Append(ctx,L"\r\n["+value(0)+L"]\r\n",RGB(104,113,123));}
    else if(e.kind=="selected"){ctx->selectedId=value(0);SelectChatRow(ctx);UpdateControls(ctx);}
    else if(e.kind=="models_begin"){ctx->modelIds.clear();SendMessageW(ctx->model,CB_RESETCONTENT,0,0);}
    else if(e.kind=="model"){
        auto id=value(0),name=value(1);int i=static_cast<int>(SendMessageW(ctx->model,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str())));
        if(i>=0){ctx->modelIds.push_back(id);if(value(2)==L"1")SendMessageW(ctx->model,CB_SETCURSEL,i,0);}
    }
    else if(e.kind=="models_end"){if(SendMessageW(ctx->model,CB_GETCURSEL,0,0)==CB_ERR && !ctx->modelIds.empty())SendMessageW(ctx->model,CB_SETCURSEL,0,0);UpdateControls(ctx);}
    else if(e.kind=="approval"){
        int result=MessageBoxW(ctx->window,value(1).c_str(),Tr(ctx,L"Разрешить действие AI?",L"Allow AI action?"),MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
        SendBridge(ctx->bridge,"approve",{value(0),result==IDYES?L"yes":L"no"});
    }
    else if(e.kind=="disconnected"){ctx->ready=false;ctx->busy=false;SetWindowTextW(ctx->status,Tr(ctx,L"Нет соединения",L"Disconnected"));UpdateControls(ctx);}
}
LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM w, LPARAM l) {
    auto ctx=reinterpret_cast<Context*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_CREATE){
        auto cs=reinterpret_cast<CREATESTRUCTW*>(l);ctx=static_cast<Context*>(cs->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ctx));ctx->window=window;
        ctx->russian=RussianEditor(ctx->owner);
        if(!CopyToolbar(ctx,window))return -1;
        SetTimer(window,1,500,nullptr);
        ctx->heading=Control(window,L"STATIC",L"Codex",SS_LEFT,kHeading);
        ctx->status=Control(window,L"STATIC",L"",SS_LEFT,kStatus);
        if(!richEditModule)richEditModule=LoadLibraryW(L"Msftedit.dll");
        ctx->rich=richEditModule!=nullptr;
        ctx->description=Control(window,ctx->rich?MSFTEDIT_CLASS:L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,kDescription);
        SendMessageW(ctx->description,EM_SETLIMITTEXT,4*1024*1024,0);
        ctx->history=Control(window,L"STATIC",Tr(ctx,L"ЧАТЫ",L"CHATS"),SS_LEFT,kHistory);
        ctx->showAll=Control(window,L"BUTTON",Tr(ctx,L"Все",L"All"),BS_AUTORADIOBUTTON|BS_PUSHLIKE|WS_GROUP,kShowAll);
        ctx->showEditor=Control(window,L"BUTTON",L"IN_EDITOR",BS_AUTORADIOBUTTON|BS_PUSHLIKE,kShowEditor);
        ctx->deleteChat=Control(window,L"BUTTON",Tr(ctx,L"Удал.",L"Delete"),BS_PUSHBUTTON,kDeleteChat);
        SendMessageW(ctx->showEditor,BM_SETCHECK,BST_CHECKED,0);
        ctx->chatList=Control(window,L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,kChatList);
        ctx->chatFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        if(ctx->chatFont){SendMessageW(ctx->chatList,WM_SETFONT,reinterpret_cast<WPARAM>(ctx->chatFont),TRUE);SendMessageW(ctx->history,WM_SETFONT,reinterpret_cast<WPARAM>(ctx->chatFont),TRUE);}
        ctx->mcp=Control(window,L"STATIC",Tr(ctx,L"SC2 MCP: проверка...",L"SC2 MCP: checking..."),SS_LEFT,kMcp);
        ctx->model=Control(window,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,kModel);
        ctx->config=Control(window,L"BUTTON",Tr(ctx,L"Настройки",L"Settings"),BS_PUSHBUTTON,kSettings);
        ctx->input=Control(window,L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,kInput);
        SendMessageW(ctx->input,EM_SETLIMITTEXT,12000,0);
        ctx->attach=Control(window,L"BUTTON",Tr(ctx,L"Картинка",L"Image"),BS_PUSHBUTTON,kAttach);
        for(size_t i=0;i<kMaxImages;++i){
            ctx->previews[i]=Control(window,L"STATIC",L"",SS_BITMAP|SS_CENTERIMAGE,kPreviewBase+static_cast<int>(i));
            ctx->removes[i]=Control(window,L"BUTTON",L"×",BS_PUSHBUTTON,kRemoveImageBase+static_cast<int>(i));
            if(!ctx->previews[i] || !ctx->removes[i])return -1;
            ShowWindow(ctx->previews[i],SW_HIDE);ShowWindow(ctx->removes[i],SW_HIDE);
        }
        ctx->send=Control(window,L"BUTTON",Tr(ctx,L"Отправить",L"Send"),BS_PUSHBUTTON,kSend);
        if(!ctx->heading||!ctx->status||!ctx->description||!ctx->history||!ctx->input||!ctx->send||!ctx->model||!ctx->config||!ctx->chatList||!ctx->mcp||!ctx->attach||!ctx->showAll||!ctx->showEditor||!ctx->deleteChat)return -1;
        if(!SetWindowSubclass(ctx->input,InputProc,1,reinterpret_cast<DWORD_PTR>(ctx)))return -1;
        ctx->ready=false;ctx->busy=false;UpdateControls(ctx);
        ctx->bridge=StartBridge(window,moduleHandle,Workspace(ctx->owner),selectedClaude,ctx->russian);
        if(ctx->bridge)SendMapContext(ctx);
        if(!ctx->bridge){SetWindowTextW(ctx->status,Tr(ctx,L"Нет соединения",L"Disconnected"));SetWindowTextW(ctx->description,Tr(ctx,L"Не удалось запустить помощник. Проверьте установку Node.js и наличие папки bridge рядом с DLL.",L"Could not start the assistant. Check Node.js and the bridge folder next to the DLL."));}
        SelectProvider(ctx,selectedClaude);return 0;
    }
    if(msg==WM_AI_BRIDGE){auto e=reinterpret_cast<BridgeEvent*>(l);if(ctx && e)OnBridge(ctx,*e);delete e;return 0;}
    if(msg==WM_ACTIVATE && ctx){
        ctx->windowActive=LOWORD(w)!=WA_INACTIVE;
        if(IsWindow(ctx->aiToolbar))SendMessageW(ctx->aiToolbar,TB_CHECKBUTTON,ctx->command,MAKELPARAM(ctx->windowActive,0));
    }
    if(msg==WM_SIZE && ctx){LayoutControls(ctx,window);return 0;}
    if(msg==WM_TIMER && w==1 && ctx){SyncFileCommands(ctx);SyncEditorTitle(ctx);return 0;}
    if(msg==WM_MEASUREITEM && w==kChatList){auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(l);item->itemHeight=22;return TRUE;}
    if(msg==WM_DRAWITEM && ctx && w==kChatList){DrawChatItem(ctx,reinterpret_cast<DRAWITEMSTRUCT*>(l));return TRUE;}
    if(msg==WM_GETMINMAXINFO){auto mm=reinterpret_cast<MINMAXINFO*>(l);mm->ptMinTrackSize.x=900;mm->ptMinTrackSize.y=420;return 0;}
    if(msg==WM_PAINT){
        PAINTSTRUCT ps{};HDC dc=BeginPaint(window,&ps);RECT r{};GetClientRect(window,&r);
        int divider=r.right<1100?220:280;
        HPEN pen=CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DLIGHT));
        if(pen){auto old=SelectObject(dc,pen);MoveToEx(dc,divider,45,nullptr);LineTo(dc,divider,r.bottom);SelectObject(dc,old);DeleteObject(pen);}
        EndPaint(window,&ps);return 0;
    }
    if(msg==WM_CTLCOLORSTATIC){SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));}
    if(msg==WM_COMMAND && ctx){
        UINT id=LOWORD(w);HWND source=reinterpret_cast<HWND>(l);
        if(id==kSettings && source==ctx->config){OpenSettings(ctx,window);return 0;}
        if((id==kShowAll && source==ctx->showAll) || (id==kShowEditor && source==ctx->showEditor)){
            ctx->allChats=id==kShowAll;SendMessageW(ctx->showAll,BM_SETCHECK,ctx->allChats?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(ctx->showEditor,BM_SETCHECK,ctx->allChats?BST_UNCHECKED:BST_CHECKED,0);RenderChats(ctx);return 0;
        }
        if(id==kDeleteChat && source==ctx->deleteChat && ctx->ready && !ctx->busy){
            int row=static_cast<int>(SendMessageW(ctx->chatList,LB_GETCURSEL,0,0));
            if(row<0 || static_cast<size_t>(row)>=ctx->chatRows.size() || ctx->selectedId.empty() || ctx->chatRows[row]!=L"t"+ctx->selectedId)return 0;
            auto chat=std::find_if(ctx->chats.begin(),ctx->chats.end(),[&](const ChatItem& item){return item.id==ctx->selectedId;});
            if(chat==ctx->chats.end())return 0;
            auto message=std::wstring(Tr(ctx,L"Удалить чат навсегда? Это также удалит дочерние чаты.\n\n",L"Delete this chat permanently? Its child chats will also be deleted.\n\n"))+chat->title;
            if(MessageBoxW(window,message.c_str(),Tr(ctx,L"Удаление чата",L"Delete chat"),MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2)==IDYES)
                if(!SendBridge(ctx->bridge,"delete",{ctx->selectedId}))SetWindowTextW(ctx->status,Tr(ctx,L"Ошибка связи",L"Connection error"));
            return 0;
        }
        if(id==kAttach && source==ctx->attach && !ctx->busy){ChooseImage(ctx);return 0;}
        if(id>=kRemoveImageBase && id<kRemoveImageBase+kMaxImages && source==ctx->removes[id-kRemoveImageBase] && !ctx->busy){RemoveImage(ctx,id-kRemoveImageBase);return 0;}
        if(id==kSend && source==ctx->send){
            if(ctx->busy && !ctx->awaitingAnswer){SendBridge(ctx->bridge,"stop");return 0;}
            int length=GetWindowTextLengthW(ctx->input);if((length>0 || (!ctx->attachments.empty() && !ctx->awaitingAnswer)) && length<=12000){std::wstring text(static_cast<size_t>(length)+1,L'\0');
                GetWindowTextW(ctx->input,text.data(),length+1);text.resize(length);
                bool answer=ctx->awaitingAnswer;
                if(!answer)SendMapContext(ctx);
                std::vector<std::wstring> fields{text};if(!answer)for(const auto& image:ctx->attachments)fields.push_back(image.path);
                if(SendBridge(ctx->bridge,answer?"answer":"send",fields)){if(answer){SetWindowTextW(ctx->input,L"");AppendEntry(ctx,std::wstring(Tr(ctx,L"Вы: ",L"You: "))+text);ctx->awaitingAnswer=false;}else ctx->pendingSend=true;ctx->busy=true;UpdateControls(ctx);}else SetWindowTextW(ctx->status,Tr(ctx,L"Ошибка связи",L"Connection error"));}return 0;
        }
        if(id==kChatList && source==ctx->chatList && HIWORD(w)==LBN_SELCHANGE){
            int i=static_cast<int>(SendMessageW(source,LB_GETCURSEL,0,0));if(i<0 || static_cast<size_t>(i)>=ctx->chatRows.size())return 0;
            auto key=ctx->chatRows[i];if(key==L"n"){SendBridge(ctx->bridge,"new");ctx->selectedId.clear();}
            else if(key[0]==L't')SendBridge(ctx->bridge,"open",{key.substr(1)});
            else if(key[0]==L'g'){auto group=key.substr(1);auto found=std::find(ctx->collapsedGroups.begin(),ctx->collapsedGroups.end(),group);if(found==ctx->collapsedGroups.end())ctx->collapsedGroups.push_back(group);else ctx->collapsedGroups.erase(found);RenderChats(ctx);}UpdateControls(ctx);return 0;
        }
        if(id==kModel && source==ctx->model && HIWORD(w)==CBN_SELCHANGE){int i=static_cast<int>(SendMessageW(source,CB_GETCURSEL,0,0));if(i>=0 && static_cast<size_t>(i)<ctx->modelIds.size())SendBridge(ctx->bridge,"model",{ctx->modelIds[i]});return 0;}
        if(source==ctx->aiToolbar){
            if(id==ctx->command){
                // Toolbar check buttons toggle on click. AI represents the
                // active window, so clicking it cannot deselect that window.
                SendMessageW(ctx->aiToolbar,TB_CHECKBUTTON,ctx->command,MAKELPARAM(ctx->windowActive,0));
                return 0;
            }
            if(IsFileCommand(ctx,static_cast<int>(id))){
                LRESULT state=IsWindow(ctx->toolbar)?SendMessageW(ctx->toolbar,TB_GETSTATE,id,0):-1;
                if(state!=-1 && (state&TBSTATE_ENABLED)){
                    if(id!=static_cast<UINT>(ctx->fileCommands[2])){
                        if(IsIconic(ctx->owner))ShowWindow(ctx->owner,SW_RESTORE);
                        SetForegroundWindow(ctx->owner);
                    }
                    SendMessageW(ctx->owner,WM_COMMAND,MAKEWPARAM(id,0),reinterpret_cast<LPARAM>(ctx->toolbar));
                }
                SyncFileCommands(ctx);return 0;
            }
            // Bring the source editor forward before invoking its own module command.
            if(IsIconic(ctx->owner))ShowWindow(ctx->owner,SW_RESTORE);
            SetForegroundWindow(ctx->owner);
            SendMessageW(ctx->owner,WM_COMMAND,MAKEWPARAM(id,0),reinterpret_cast<LPARAM>(ctx->toolbar));
            return 0;
        }
        if(!source){
            if(IsIconic(ctx->owner))ShowWindow(ctx->owner,SW_RESTORE);
            SetForegroundWindow(ctx->owner);
            SendMessageW(ctx->owner,WM_COMMAND,w,0);return 0;
        }
    }
    if(msg==WM_CLOSE){DestroyWindow(window);return 0;}
    if(msg==WM_NCDESTROY && ctx){
        KillTimer(window,1);
        if(currentAIWindow==window)currentAIWindow=nullptr;
        StopBridge(ctx->bridge);ctx->bridge=nullptr;
        MSG pending{};while(PeekMessageW(&pending,window,WM_AI_BRIDGE,WM_AI_BRIDGE,PM_REMOVE))delete reinterpret_cast<BridgeEvent*>(pending.lParam);
        ctx->window=nullptr;ctx->aiToolbar=nullptr;
        ClearImages(ctx,true);
        if(ctx->aiImages){ImageList_Destroy(ctx->aiImages);ctx->aiImages=nullptr;}
        if(ctx->chatFont){DeleteObject(ctx->chatFont);ctx->chatFont=nullptr;}
    }
    return DefWindowProcW(window,msg,w,l);
}

// Copy the source module's normal menu; commands are handled by that module.
HMENU CopyMenuTree(HMENU source,bool root){
    HMENU copy=root?CreateMenu():CreatePopupMenu();if(!copy)return nullptr;
    int count=GetMenuItemCount(source);
    for(int i=0;i<count;++i){
        wchar_t label[1024]{};MENUITEMINFOW item{};item.cbSize=sizeof(item);
        item.fMask=MIIM_FTYPE|MIIM_STATE|MIIM_ID|MIIM_SUBMENU|MIIM_STRING;
        item.dwTypeData=label;item.cch=1023;
        if(!GetMenuItemInfoW(source,static_cast<UINT>(i),TRUE,&item)){DestroyMenu(copy);return nullptr;}
        if(item.fType&MFT_OWNERDRAW){item.fType&=~MFT_OWNERDRAW;item.fState|=MFS_DISABLED;}
        if(item.hSubMenu){item.hSubMenu=CopyMenuTree(item.hSubMenu,false);if(!item.hSubMenu){DestroyMenu(copy);return nullptr;}}
        if(!InsertMenuItemW(copy,static_cast<UINT>(i),TRUE,&item)){
            if(item.hSubMenu)DestroyMenu(item.hSubMenu);DestroyMenu(copy);return nullptr;
        }
    }
    return copy;
}

bool OpenWindow(Context* ctx) {
    if(IsWindow(currentAIWindow)){
        if(IsIconic(currentAIWindow))ShowWindow(currentAIWindow,SW_RESTORE);
        SetForegroundWindow(currentAIWindow);return true;
    }
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=WindowProc;
    wc.hInstance=moduleHandle;wc.lpszClassName=kWindowClass;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    wc.hIcon=reinterpret_cast<HICON>(SendMessageW(ctx->owner,WM_GETICON,ICON_BIG,0));
    if(!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    wchar_t ownerTitle[1024]{},title[1100]{};GetWindowTextW(ctx->owner,ownerTitle,1024);
    const wchar_t* suffix=wcsstr(ownerTitle,L" - ");
    swprintf_s(title,L"AI%ls",suffix?suffix:L" - StarCraft II Editor");
    RECT r{};GetWindowRect(ctx->owner,&r);
    HWND window=CreateWindowExW(WS_EX_APPWINDOW,kWindowClass,title,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,moduleHandle,ctx);
    if(!window)return false;
    HMENU sourceMenu=GetMenu(ctx->owner);
    if(sourceMenu){HMENU menu=CopyMenuTree(sourceMenu,true);if(menu && !SetMenu(window,menu))DestroyMenu(menu);}
    currentAIWindow=window;ShowWindow(window,IsZoomed(ctx->owner)?SW_SHOWMAXIMIZED:SW_SHOW);UpdateWindow(window);return true;
}

LRESULT CALLBACK OwnerProc(HWND owner, UINT message, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR ref) {
    auto ctx=reinterpret_cast<Context*>(ref);
    if (message==WM_COMMAND && LOWORD(w)==ctx->command && reinterpret_cast<HWND>(l)==ctx->toolbar) {
        OpenWindow(ctx); return 0;
    }
    if (message==WM_NCDESTROY) {
        RemoveWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
        RemovePropW(ctx->toolbar,kProperty);
        if (IsWindow(ctx->window)) DestroyWindow(ctx->window);
        ReleaseOwnedImages(ctx);
        InterlockedDecrement(&activeButtons);
        delete ctx;
    }
    return DefSubclassProc(owner,message,w,l);
}

bool IsOurThread(HWND window) {
    DWORD pid=0; DWORD thread=GetWindowThreadProcessId(window,&pid);
    return IsWindow(window) && pid==GetCurrentProcessId() && thread==GetCurrentThreadId();
}
struct LegacyClose { Context* ctx; bool wasOpen; };
BOOL CALLBACK CheckLegacyChatWindow(HWND window,LPARAM raw){
    wchar_t cls[128]{};GetClassNameW(window,cls,128);auto safe=reinterpret_cast<bool*>(raw);
    if(!lstrcmpW(cls,L"SC2AIButton.Settings.v21") || !lstrcmpW(cls,L"SC2AIButton.Settings.v22")){*safe=false;return FALSE;}
    if(lstrcmpW(cls,kLegacyWindowClass21) && lstrcmpW(cls,kLegacyWindowClass22))return TRUE;
    HWND input=GetDlgItem(window,kInput),history=GetDlgItem(window,kDescription),list=GetDlgItem(window,kChatList),send=GetDlgItem(window,kSend);
    wchar_t label[64]{};GetWindowTextW(send,label,64);
    if(!input || !history || !list || !send || GetWindowTextLengthW(input)!=0 || GetWindowTextLengthW(history)!=0 ||
       SendMessageW(list,LB_GETCURSEL,0,0)!=0 || (lstrcmpW(label,L"Send") && lstrcmpW(label,L"Отправить")) || !IsWindowEnabled(input)) *safe=false;
    for(int i=0;i<static_cast<int>(kMaxImages);++i){HWND preview=GetDlgItem(window,kPreviewBase+i);if(preview && (GetWindowLongPtrW(preview,GWL_STYLE)&WS_VISIBLE))*safe=false;}
    return *safe?TRUE:FALSE;
}
BOOL CALLBACK CloseLegacyWindow(HWND window, LPARAM raw) {
    auto state=reinterpret_cast<LegacyClose*>(raw);
    wchar_t cls[128]{}; GetClassNameW(window,cls,128);
    if (((!lstrcmpW(cls,kLegacyWindowClass7) || !lstrcmpW(cls,kLegacyWindowClass8) || !lstrcmpW(cls,kLegacyWindowClass9) || !lstrcmpW(cls,kLegacyWindowClass10) || !lstrcmpW(cls,kLegacyWindowClass11) || !lstrcmpW(cls,kLegacyWindowClass12) || !lstrcmpW(cls,kLegacyWindowClass13) || !lstrcmpW(cls,kLegacyWindowClass14) || !lstrcmpW(cls,kLegacyWindowClass15) || !lstrcmpW(cls,kLegacyWindowClass16) || !lstrcmpW(cls,kLegacyWindowClass17) || !lstrcmpW(cls,kLegacyWindowClass18) || !lstrcmpW(cls,kLegacyWindowClass19) || !lstrcmpW(cls,kLegacyWindowClass20) || !lstrcmpW(cls,kLegacyWindowClass21) || !lstrcmpW(cls,kLegacyWindowClass22)) && !GetWindow(window,GW_OWNER)) || ((!lstrcmpW(cls,kLegacyWindowClass1) || !lstrcmpW(cls,kLegacyWindowClass2) || !lstrcmpW(cls,kLegacyWindowClass4) || !lstrcmpW(cls,kLegacyWindowClass5) || !lstrcmpW(cls,kLegacyWindowClass6)) && GetWindow(window,GW_OWNER)==state->ctx->owner)) {
        state->wasOpen=true;
        DestroyWindow(window);
    }
    return TRUE;
}
BOOL CALLBACK CloseLegacyPanel(HWND window, LPARAM raw) {
    auto state=reinterpret_cast<LegacyClose*>(raw);
    wchar_t cls[128]{}; GetClassNameW(window,cls,128);
    if (!lstrcmpW(cls,kLegacyWindowClass3)) { state->wasOpen=true; DestroyWindow(window); }
    return TRUE;
}
BOOL CALLBACK CloseStaleOverlay(HWND window, LPARAM raw){
    auto ctx=reinterpret_cast<Context*>(raw);wchar_t cls[128]{};GetClassNameW(window,cls,128);
    if(GetWindow(window,GW_OWNER)==ctx->owner && (!lstrcmpW(cls,kLegacyWindowClass1) || !lstrcmpW(cls,kLegacyWindowClass2) || !lstrcmpW(cls,kLegacyWindowClass4) || !lstrcmpW(cls,kLegacyWindowClass5) || !lstrcmpW(cls,kLegacyWindowClass6)))DestroyWindow(window);
    return TRUE;
}
unsigned LegacyMask(HWND toolbar){
    const wchar_t* props[]={kLegacyProperty1,kLegacyProperty2,kLegacyProperty3,kLegacyProperty4,kLegacyProperty5,kLegacyProperty6,kLegacyProperty7,kLegacyProperty8,kLegacyProperty9,kLegacyProperty10,kLegacyProperty11,kLegacyProperty12,kLegacyProperty13,kLegacyProperty14,kLegacyProperty15,kLegacyProperty16,kLegacyProperty17,kLegacyProperty18,kLegacyProperty19,kLegacyProperty20,kLegacyProperty21,kLegacyProperty22};
    unsigned mask=0;for(unsigned i=0;i<22;++i)if(GetPropW(toolbar,props[i]))mask|=1u<<i;return mask;
}
void Reconcile(Context* ctx){
    unsigned mask=LegacyMask(ctx->toolbar);
    if(mask!=ctx->legacyMask){
        RemoveWindowSubclass(ctx->owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
        if(SetWindowSubclass(ctx->owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx),reinterpret_cast<DWORD_PTR>(ctx)))ctx->legacyMask=mask;
    }
    EnumThreadWindows(GetCurrentThreadId(),CloseStaleOverlay,reinterpret_cast<LPARAM>(ctx));
    LegacyClose state{ctx,false};EnumChildWindows(ctx->owner,CloseLegacyPanel,reinterpret_cast<LPARAM>(&state));
}
}

// Called on the toolbar's GUI thread. The module must stay loaded until detach.
extern "C" __declspec(dllexport) BOOL WINAPI SC2AI_Attach(HWND toolbar, int index, UINT command) {
    wchar_t cls[64]{};
    HWND owner=GetParent(toolbar);
    if (!IsOurThread(toolbar) || !IsOurThread(owner) ||
        !GetClassNameW(toolbar,cls,64) || lstrcmpiW(cls,TOOLBARCLASSNAMEW) ||
        command==0 || command>0xffff) return FALSE;
    if(auto existing=static_cast<Context*>(GetPropW(toolbar,kProperty))){Reconcile(existing);return FALSE;}
    LRESULT count=SendMessageW(toolbar,TB_BUTTONCOUNT,0,0);
    LRESULT current=SendMessageW(toolbar,TB_COMMANDTOINDEX,command,0);
    int legacy=0;unsigned legacyMask=LegacyMask(toolbar);for(int version=22;version>=1;--version)if(legacyMask&(1u<<(version-1))){legacy=version;break;}
    if(legacy>=21){bool safe=true;EnumThreadWindows(GetCurrentThreadId(),CheckLegacyChatWindow,reinterpret_cast<LPARAM>(&safe));if(!safe)return FALSE;}
    if (count<0 || index<0 || index>count || (legacy ? current!=index : current!=-1)) return FALSE;
    auto ctx=new(std::nothrow) Context{};
    if (!ctx) return FALSE;
    ctx->toolbar=toolbar;ctx->owner=owner;ctx->command=command;ctx->legacy=legacy;
    if (legacy) {
        ctx->original.cbSize=sizeof(ctx->original);
        ctx->original.dwMask=TBIF_IMAGE|TBIF_STYLE|TBIF_SIZE;
        if (SendMessageW(toolbar,TB_GETBUTTONINFOW,command,reinterpret_cast<LPARAM>(&ctx->original))<0) { delete ctx; return FALSE; }
    }
    if (!SetPropW(toolbar,kProperty,ctx)) { delete ctx; return FALSE; }
    if (!SetWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx),reinterpret_cast<DWORD_PTR>(ctx))) {
        RemovePropW(toolbar,kProperty); delete ctx; return FALSE;
    }
    int imageIndex=AddAIIcon(toolbar,ctx,index);
    bool inserted=false;
    if (imageIndex>=0 && legacy) {
        RECT previous{};
        int width=LOWORD(SendMessageW(toolbar,TB_GETBUTTONSIZE,0,0));
        if (index>0 && SendMessageW(toolbar,TB_GETITEMRECT,index-1,reinterpret_cast<LPARAM>(&previous))) width=previous.right-previous.left;
        if (width<12 || width>256) width=32;
        TBBUTTONINFOW info{}; info.cbSize=sizeof(info);
        info.dwMask=TBIF_IMAGE|TBIF_STYLE|TBIF_SIZE|TBIF_TEXT;
        info.iImage=imageIndex; info.fsStyle=BTNS_BUTTON; info.cx=static_cast<WORD>(width);
        wchar_t empty[]=L""; info.pszText=empty;
        inserted=SendMessageW(toolbar,TB_SETBUTTONINFOW,command,reinterpret_cast<LPARAM>(&info))!=FALSE;
    } else if (imageIndex>=0) {
        TBBUTTON button{}; button.iBitmap=imageIndex; button.idCommand=static_cast<int>(command);
        button.fsState=TBSTATE_ENABLED; button.fsStyle=BTNS_BUTTON; button.iString=-1;
        inserted=SendMessageW(toolbar,TB_INSERTBUTTONW,index,reinterpret_cast<LPARAM>(&button))!=FALSE;
    }
    if (!inserted) {
        RemoveWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
        RemovePropW(toolbar,kProperty); ReleaseOwnedImages(ctx); delete ctx; return FALSE;
    }
    SendMessageW(toolbar,TB_AUTOSIZE,0,0);ctx->legacyMask=LegacyMask(toolbar);
    InterlockedIncrement(&activeButtons);
    if (legacy) {
        LegacyClose previous{ctx,false};
        EnumThreadWindows(GetCurrentThreadId(),CloseLegacyWindow,reinterpret_cast<LPARAM>(&previous));
        EnumChildWindows(owner,CloseLegacyPanel,reinterpret_cast<LPARAM>(&previous));
        if (previous.wasOpen) OpenWindow(ctx);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL WINAPI SC2AI_Detach(HWND toolbar, UINT command) {
    if (!IsOurThread(toolbar)) return FALSE;
    auto ctx=static_cast<Context*>(GetPropW(toolbar,kProperty));
    if (!ctx || ctx->command!=command) return FALSE;
    LRESULT index=SendMessageW(toolbar,TB_COMMANDTOINDEX,command,0);
    if (index<0) return FALSE;
    if (ctx->legacy) {
        auto old=ctx->original;
        wchar_t text[]=L"AI"; wchar_t empty[]=L"";
        old.dwMask|=TBIF_TEXT; old.pszText=ctx->legacy==1?text:empty;
        if (!SendMessageW(toolbar,TB_SETBUTTONINFOW,command,reinterpret_cast<LPARAM>(&old))) return FALSE;
    } else if (!SendMessageW(toolbar,TB_DELETEBUTTON,index,0)) return FALSE;
    RemoveWindowSubclass(ctx->owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
    RemovePropW(toolbar,kProperty);
    if (IsWindow(ctx->window)) DestroyWindow(ctx->window);
    ReleaseOwnedImages(ctx);
    delete ctx;
    InterlockedDecrement(&activeButtons);
    SendMessageW(toolbar,TB_AUTOSIZE,0,0);
    return TRUE;
}

namespace {
// Only the observed module toolbar shape is accepted; ambiguous layouts are ignored.
bool FindInsertion(HWND toolbar, int& insertion) {
    int count=static_cast<int>(SendMessageW(toolbar,TB_BUTTONCOUNT,0,0));
    if (count<11 || count>150 || SendMessageW(toolbar,TB_COMMANDTOINDEX,kCommand,0)!=-1) return false;
    TBBUTTON sep{},last{},previous{};
    if (!SendMessageW(toolbar,TB_GETBUTTON,count-2,reinterpret_cast<LPARAM>(&sep)) ||
        !SendMessageW(toolbar,TB_GETBUTTON,count-1,reinterpret_cast<LPARAM>(&last)) ||
        !SendMessageW(toolbar,TB_GETBUTTON,count-3,reinterpret_cast<LPARAM>(&previous))) return false;
    if (!(sep.fsStyle&BTNS_SEP) || (last.fsStyle&BTNS_SEP) || (previous.fsStyle&BTNS_SEP)) return false;
    for (int i=0;i<count;++i) { TBBUTTON b{}; if (!SendMessageW(toolbar,TB_GETBUTTON,i,reinterpret_cast<LPARAM>(&b)) || b.idCommand!=i) return false; }
    insertion=count-2;
    return true;
}
BOOL CALLBACK InstallOnWindow(HWND owner, LPARAM) {
    if (!IsOurThread(owner)) return TRUE;
    HWND toolbar=EditorModuleToolbar(owner);
    if (!toolbar) return TRUE;
    if(auto ctx=static_cast<Context*>(GetPropW(toolbar,kProperty))){Reconcile(ctx);return TRUE;}
    wchar_t barClass[64]{}; GetClassNameW(toolbar,barClass,64);
    int index=0;
    if (lstrcmpiW(barClass,TOOLBARCLASSNAMEW)) return TRUE;
    if (LegacyMask(toolbar)) {
        auto oldIndex=SendMessageW(toolbar,TB_COMMANDTOINDEX,kCommand,0);
        if (oldIndex>=0) SC2AI_Attach(toolbar,static_cast<int>(oldIndex),kCommand);
    } else if (FindInsertion(toolbar,index)) SC2AI_Attach(toolbar,index,kCommand);
    return TRUE;
}
LRESULT CALLBACK GetMessageHook(int code, WPARAM w, LPARAM l) {
    auto msg=reinterpret_cast<MSG*>(l);
    if (code>=0 && msg && !msg->hwnd && msg->message==scanMessage)
        EnumThreadWindows(GetCurrentThreadId(),InstallOnWindow,0);
    return CallNextHookEx(threadHook,code,w,l);
}
BOOL CALLBACK FindEditorThread(HWND window, LPARAM data) {
    DWORD pid=0; DWORD tid=GetWindowThreadProcessId(window,&pid);
    if (pid!=GetCurrentProcessId()) return TRUE;
    if (EditorModuleToolbar(window)) {
        *reinterpret_cast<DWORD*>(data)=tid; return FALSE;
    }
    return TRUE;
}
DWORD WINAPI Worker(void*) {
    DWORD installedThread=0;
    for (;;) {
        DWORD tid=0; EnumWindows(FindEditorThread,reinterpret_cast<LPARAM>(&tid));
        if (tid && tid!=installedThread) {
            if (threadHook) UnhookWindowsHookEx(threadHook);
            // The target thread is in this process; no cross-process DLL hook is needed.
            threadHook=SetWindowsHookExW(WH_GETMESSAGE,GetMessageHook,nullptr,tid);
            installedThread=threadHook?tid:0;
        }
        if (installedThread) PostThreadMessageW(installedThread,scanMessage,0,0);
        Sleep(750);
    }
}
}

// The loader calls this once after loading the DLL into SC2Editor_x64.exe.
extern "C" __declspec(dllexport) DWORD WINAPI SC2AI_Start(void*) {
    wchar_t path[32768]{};DWORD length=GetModuleFileNameW(nullptr,path,32768);
    if (!length || length>=32768) return ERROR_BAD_EXE_FORMAT;
    const wchar_t* name=wcsrchr(path,L'\\'); name=name?name+1:path;
    if (lstrcmpiW(name,L"SC2Editor_x64.exe")) return ERROR_BAD_EXE_FORMAT;
    if (InterlockedCompareExchange(&started,1,0)!=0) return ERROR_ALREADY_EXISTS;
    scanMessage=RegisterWindowMessageW(L"SC2AIButton.Scan.v23");
    if (!scanMessage) { InterlockedExchange(&started,0); return GetLastError(); }
    HANDLE thread=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);
    if (!thread) { InterlockedExchange(&started,0); return GetLastError(); }
    CloseHandle(thread); return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SC2AI_Status(void*) {
    return InterlockedCompareExchange(&activeButtons,0,0)>0 ? ERROR_SUCCESS : ERROR_IO_PENDING;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) { moduleHandle=instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
