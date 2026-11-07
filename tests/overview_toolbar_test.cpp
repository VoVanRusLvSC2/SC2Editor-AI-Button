#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
using Detach=BOOL (WINAPI*)(HWND,UINT);
static HWND oldWindow,newWindow;
static BOOL CALLBACK Find(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v23"))oldWindow=h;if(!lstrcmpW(cls,L"SC2AIButton.Window.v25"))newWindow=h;return TRUE;}
int wmain(int argc,wchar_t**argv){
    if(argc!=2)return 1;INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common))return 2;
    HWND owner=CreateWindowExW(0,L"STATIC",L"test",WS_OVERLAPPEDWINDOW,-30000,-30000,900,600,nullptr,nullptr,nullptr,nullptr);
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT,0,0,900,30,owner,nullptr,nullptr,nullptr);if(!owner||!bar)return 3;
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
    const wchar_t names[]=L"Terrain (F5)\0Triggers (F6)\0Data (F7)\0Text (F8)\0Import (F9)\0AI (Shift+F5)\0UI (Shift+F6)\0Cutscenes (Shift+F7)\0Overview (F12)\0Test\0\0";
    LRESULT first=SendMessageW(bar,TB_ADDSTRINGW,0,reinterpret_cast<LPARAM>(names));if(first<0)return 4;
    TBBUTTON buttons[12]{};buttons[0].fsStyle=BTNS_SEP;buttons[0].iBitmap=8;
    for(int i=1;i<=9;++i){buttons[i].idCommand=100+i;buttons[i].iBitmap=I_IMAGENONE;buttons[i].fsState=TBSTATE_ENABLED;buttons[i].iString=first+i-1;}
    buttons[10].fsStyle=BTNS_SEP;buttons[10].iBitmap=8;buttons[11].idCommand=111;buttons[11].iBitmap=I_IMAGENONE;buttons[11].fsState=TBSTATE_ENABLED;buttons[11].iString=first+9;
    if(!SendMessageW(bar,TB_ADDBUTTONSW,12,reinterpret_cast<LPARAM>(buttons)))return 5;
    wchar_t currentPath[32768]{};if(!GetFullPathNameW(L"SC2AIButton.dll",32768,currentPath,nullptr))return 6;
    HMODULE old=LoadLibraryW(argv[1]),current=LoadLibraryW(currentPath);if(!old||!current||old==current)return 6;
    auto oldAttach=reinterpret_cast<Attach>(GetProcAddress(old,"SC2AI_Attach"));auto oldDetach=reinterpret_cast<Detach>(GetProcAddress(old,"SC2AI_Detach"));
    auto attach=reinterpret_cast<Attach>(GetProcAddress(current,"SC2AI_Attach"));auto detach=reinterpret_cast<Detach>(GetProcAddress(current,"SC2AI_Detach"));
    if(!oldAttach||!oldDetach||!attach||!detach||!oldAttach(bar,10,500))return 7;
    if(SendMessageW(bar,TB_COMMANDTOINDEX,109,0)!=9 || SendMessageW(bar,TB_COMMANDTOINDEX,500,0)!=10)return 8;
    if(!attach(bar,10,500)){DWORD error=GetLastError();std::printf("attach failed: error=%lu ai=%lld overview=%lld\n",error,static_cast<long long>(SendMessageW(bar,TB_COMMANDTOINDEX,500,0)),static_cast<long long>(SendMessageW(bar,TB_COMMANDTOINDEX,109,0)));return 9;}
    if(SendMessageW(bar,TB_COMMANDTOINDEX,500,0)!=9 || SendMessageW(bar,TB_COMMANDTOINDEX,109,0)!=10)return 10;
    SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));EnumThreadWindows(GetCurrentThreadId(),Find,0);if(!newWindow || oldWindow)return 11;
    HWND copied=GetDlgItem(newWindow,100);if(!copied || !(SendMessageW(copied,TB_GETSTATE,109,0)&TBSTATE_ENABLED))return 12;
    if(!detach(bar,500) || SendMessageW(bar,TB_COMMANDTOINDEX,500,0)!=10 || SendMessageW(bar,TB_COMMANDTOINDEX,109,0)!=9)return 13;
    if(!oldDetach(bar,500))return 14;DestroyWindow(owner);FreeLibrary(current);FreeLibrary(old);
    std::puts("PASS: AI moves before Overview, Overview stays enabled, and detach restores the old order");return 0;
}
