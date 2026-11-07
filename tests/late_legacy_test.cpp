#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
using Detach=BOOL (WINAPI*)(HWND,UINT);
static HWND legacy,latest;
static BOOL CALLBACK Find(HWND h,LPARAM owner){wchar_t cls[128]{};GetClassNameW(h,cls,128);
    if(!lstrcmpW(cls,L"SC2AIButton.Window.v6") && GetWindow(h,GW_OWNER)==reinterpret_cast<HWND>(owner))legacy=h;
    if(!lstrcmpW(cls,L"SC2AIButton.Window.v25") && !GetWindow(h,GW_OWNER))latest=h;return TRUE;}
int wmain(int argc,wchar_t** argv){
    if(argc!=3)return 1;INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common))return 2;
    HWND owner=CreateWindowExW(0,L"STATIC",L"Import test",WS_OVERLAPPEDWINDOW,-30000,-30000,700,500,nullptr,nullptr,nullptr,nullptr);
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|TBSTYLE_FLAT,0,0,700,30,owner,nullptr,nullptr,nullptr);if(!owner||!bar)return 3;ShowWindow(owner,SW_SHOWNOACTIVATE);
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);TBBUTTON buttons[2]{};for(int i=0;i<2;++i){buttons[i].iBitmap=I_IMAGENONE;buttons[i].idCommand=100+i;buttons[i].fsState=TBSTATE_ENABLED;}
    if(!SendMessageW(bar,TB_ADDBUTTONSW,2,reinterpret_cast<LPARAM>(buttons)))return 4;
    wchar_t currentPath[32768]{};if(!GetFullPathNameW(L"SC2AIButton.dll",32768,currentPath,nullptr))return 5;
    HMODULE v5=LoadLibraryW(argv[1]),v6=LoadLibraryW(argv[2]),current=LoadLibraryW(currentPath);if(!v5||!v6||!current)return 5;
    auto a5=reinterpret_cast<Attach>(GetProcAddress(v5,"SC2AI_Attach"));auto d5=reinterpret_cast<Detach>(GetProcAddress(v5,"SC2AI_Detach"));
    auto a6=reinterpret_cast<Attach>(GetProcAddress(v6,"SC2AI_Attach"));auto d6=reinterpret_cast<Detach>(GetProcAddress(v6,"SC2AI_Detach"));
    auto attach=reinterpret_cast<Attach>(GetProcAddress(current,"SC2AI_Attach"));auto detach=reinterpret_cast<Detach>(GetProcAddress(current,"SC2AI_Detach"));
    if(!a5||!d5||!a6||!d6||!attach||!detach||!a5(bar,1,500)||!attach(bar,1,500)||!a6(bar,1,500))return 6;
    SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));EnumThreadWindows(GetCurrentThreadId(),Find,reinterpret_cast<LPARAM>(owner));
    if(!IsWindow(legacy))return 7;HWND old=legacy;
    if(attach(bar,1,500) || IsWindow(old))return 8;
    legacy=nullptr;SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));EnumThreadWindows(GetCurrentThreadId(),Find,reinterpret_cast<LPARAM>(owner));
    if(!IsWindow(latest)||IsWindow(legacy))return 9;
    HWND keep=latest;if(attach(bar,1,500)||!IsWindow(keep))return 10;
    if(!d6(bar,500)||!detach(bar,500)||!d5(bar,500))return 11;
    FreeLibrary(current);FreeLibrary(v6);FreeLibrary(v5);DestroyWindow(owner);
    std::puts("PASS: late legacy hook is reconciled; old overlay closed; current AI remains usable");return 0;
}
