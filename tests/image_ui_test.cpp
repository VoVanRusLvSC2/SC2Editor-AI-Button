#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
static HWND ai;
static BOOL CALLBACK Find(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v38"))ai=h;return TRUE;}
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
    HWND input=GetDlgItem(ai,107);HBITMAP handles[3]{};
    for(int i=0;i<3;++i){HDC dc=GetDC(owner);HBITMAP image=CreateCompatibleBitmap(dc,4+i,4+i);ReleaseDC(owner,dc);if(!image||!OpenClipboard(ai))return 9;
        if(!EmptyClipboard()||!SetClipboardData(CF_BITMAP,image))return 10;CloseClipboard();SendMessageW(input,WM_PASTE,0,0);
        handles[i]=reinterpret_cast<HBITMAP>(SendMessageW(GetDlgItem(ai,130+i),STM_GETIMAGE,IMAGE_BITMAP,0));if(!handles[i])return 11;}
    SendMessageW(ai,WM_COMMAND,MAKEWPARAM(141,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(ai,141)));
    if(reinterpret_cast<HBITMAP>(SendMessageW(GetDlgItem(ai,130),STM_GETIMAGE,IMAGE_BITMAP,0))!=handles[0] || reinterpret_cast<HBITMAP>(SendMessageW(GetDlgItem(ai,131),STM_GETIMAGE,IMAGE_BITMAP,0))!=handles[2] || SendMessageW(GetDlgItem(ai,132),STM_GETIMAGE,IMAGE_BITMAP,0))return 12;
    SendMessageW(ai,WM_COMMAND,MAKEWPARAM(140,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(ai,140)));
    if(reinterpret_cast<HBITMAP>(SendMessageW(GetDlgItem(ai,130),STM_GETIMAGE,IMAGE_BITMAP,0))!=handles[2])return 13;
    SendMessageW(ai,WM_COMMAND,MAKEWPARAM(140,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(ai,140)));
    if(SendMessageW(GetDlgItem(ai,130),STM_GETIMAGE,IMAGE_BITMAP,0))return 14;
    DestroyWindow(ai);DestroyWindow(owner);CloseDesktop(desktop);CloseWindowStation(station);
    std::puts("PASS: three clipboard images, middle removal, remaining images removable individually");return 0;
}
