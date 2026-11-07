#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
using Detach=BOOL (WINAPI*)(HWND,UINT);
struct Module { HMODULE h;Attach attach;Detach detach; };
static HWND oldPanel,newPanel;
static BOOL CALLBACK Child(HWND h,LPARAM) {wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v3"))oldPanel=h;return TRUE;}
static BOOL CALLBACK Top(HWND h,LPARAM) {wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v21") && !GetWindow(h,GW_OWNER))newPanel=h;return TRUE;}
int wmain(int argc,wchar_t**argv) {
    if(argc!=21)return 1;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common))return 2;
    HWND owner=CreateWindowExW(0,L"STATIC",L"test",WS_OVERLAPPEDWINDOW,-30000,-30000,600,300,nullptr,nullptr,nullptr,nullptr);
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|TBSTYLE_FLAT,0,0,500,30,owner,nullptr,nullptr,nullptr);
    if(!owner||!bar)return 3;ShowWindow(owner,SW_SHOWNOACTIVATE);
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
    HIMAGELIST images=ImageList_Create(32,32,ILC_COLOR32|ILC_MASK,0,4);if(!images)return 4;
    SendMessageW(bar,TB_SETIMAGELIST,0,reinterpret_cast<LPARAM>(images));
    TBBUTTON buttons[2]{};for(int i=0;i<2;++i){buttons[i].iBitmap=I_IMAGENONE;buttons[i].idCommand=100+i;buttons[i].fsState=TBSTATE_ENABLED;}
    if(!SendMessageW(bar,TB_ADDBUTTONSW,2,reinterpret_cast<LPARAM>(buttons)))return 5;
    Module modules[21]{};wchar_t currentPath[32768]{};if(!GetFullPathNameW(L"SC2AIButton.dll",32768,currentPath,nullptr))return 6;
    for(int i=0;i<21;++i){modules[i].h=LoadLibraryW(i<20?argv[i+1]:currentPath);if(!modules[i].h)return 7;
        modules[i].attach=reinterpret_cast<Attach>(GetProcAddress(modules[i].h,"SC2AI_Attach"));
        modules[i].detach=reinterpret_cast<Detach>(GetProcAddress(modules[i].h,"SC2AI_Detach"));
        if(!modules[i].attach||!modules[i].detach||!modules[i].attach(bar,1,500))return 8;
        if(i==2){SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));EnumChildWindows(owner,Child,0);if(!IsWindow(oldPanel))return 9;}
    }
    EnumThreadWindows(GetCurrentThreadId(),Top,reinterpret_cast<LPARAM>(owner));
    if(IsWindow(oldPanel)||!IsWindow(newPanel)||!IsWindowVisible(newPanel)||SendMessageW(bar,TB_BUTTONCOUNT,0,0)!=3)return 10;
    SendMessageW(newPanel,WM_CLOSE,0,0);newPanel=nullptr;
    SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));EnumThreadWindows(GetCurrentThreadId(),Top,reinterpret_cast<LPARAM>(owner));
    if(!IsWindow(newPanel)||!IsWindowVisible(newPanel))return 11;
    for(int i=20;i>=0;--i)if(!modules[i].detach(bar,500))return 12;
    if(SendMessageW(bar,TB_BUTTONCOUNT,0,0)!=2)return 13;
    for(auto m:modules)FreeLibrary(m.h);DestroyWindow(owner);ImageList_Destroy(images);
    std::puts("PASS: v1 through v21 coexist; separate AI window opens and can reopen");return 0;
}
