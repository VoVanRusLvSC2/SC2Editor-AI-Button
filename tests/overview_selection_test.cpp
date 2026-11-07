#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
using Detach=BOOL (WINAPI*)(HWND,UINT);
static void Pump(){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
int wmain(){
    wchar_t name[80]{};swprintf_s(name,L"SC2SelectionTest-%lu",GetCurrentProcessId());
    HWINSTA station=CreateWindowStationW(name,0,WINSTA_ALL_ACCESS,nullptr);if(!station||!SetProcessWindowStation(station))return 1;
    HDESK desktop=CreateDesktopW(L"Selection",nullptr,nullptr,0,GENERIC_ALL,nullptr);if(!desktop||!SetThreadDesktop(desktop))return 2;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common))return 3;
    HWND owner=CreateWindowExW(0,L"STATIC",L"Data - [test]",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,nullptr,nullptr);
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT,0,0,900,30,owner,nullptr,nullptr,nullptr);if(!owner||!bar)return 4;
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
    const wchar_t names[]=L"Terrain (F5)\0Triggers (F6)\0Data (F7)\0Text (F8)\0Import (F9)\0AI (Shift+F5)\0UI (Shift+F6)\0Cutscenes (Shift+F7)\0Overview (F12)\0Test\0\0";
    LRESULT first=SendMessageW(bar,TB_ADDSTRINGW,0,reinterpret_cast<LPARAM>(names));if(first<0)return 5;
    TBBUTTON buttons[12]{};buttons[0].fsStyle=BTNS_SEP;buttons[0].iBitmap=8;
    for(int i=1;i<=9;++i){buttons[i].idCommand=100+i;buttons[i].iBitmap=I_IMAGENONE;buttons[i].fsState=TBSTATE_ENABLED;buttons[i].iString=first+i-1;}
    buttons[10].fsStyle=BTNS_SEP;buttons[10].iBitmap=8;buttons[11].idCommand=111;buttons[11].iBitmap=I_IMAGENONE;buttons[11].fsState=TBSTATE_ENABLED;buttons[11].iString=first+9;
    if(!SendMessageW(bar,TB_ADDBUTTONSW,12,reinterpret_cast<LPARAM>(buttons)))return 6;
    HMODULE dll=LoadLibraryW(L"SC2AIButton.dll");if(!dll)return 7;
    auto attach=reinterpret_cast<Attach>(GetProcAddress(dll,"SC2AI_Attach"));auto detach=reinterpret_cast<Detach>(GetProcAddress(dll,"SC2AI_Detach"));
    if(!attach || !detach || !attach(bar,9,500))return 8;
    SendMessageW(bar,TB_CHECKBUTTON,103,MAKELONG(TRUE,0));SendMessageW(bar,TB_CHECKBUTTON,111,MAKELONG(TRUE,0));
    // An ambiguous or non-Overview window must never have its native selection rewritten.
    SendMessageW(bar,TB_CHECKBUTTON,500,MAKELONG(TRUE,0));Pump();
    if(!(SendMessageW(bar,TB_GETSTATE,103,0)&TBSTATE_CHECKED) || (SendMessageW(bar,TB_GETSTATE,109,0)&TBSTATE_CHECKED))return 9;
    SendMessageW(bar,TB_CHECKBUTTON,500,MAKELONG(FALSE,0));SetWindowTextW(owner,L"Overview - [test]");
    // Simulate the real Editor selecting the inserted AI button on the Overview toolbar.
    SendMessageW(bar,TB_CHECKBUTTON,500,MAKELONG(TRUE,0));Pump();
    if((SendMessageW(bar,TB_GETSTATE,103,0)&TBSTATE_CHECKED) || (SendMessageW(bar,TB_GETSTATE,500,0)&TBSTATE_CHECKED) ||
       !(SendMessageW(bar,TB_GETSTATE,109,0)&TBSTATE_CHECKED) || !(SendMessageW(bar,TB_GETSTATE,111,0)&TBSTATE_CHECKED))return 10;
    SendMessageW(bar,TB_CHECKBUTTON,109,MAKELONG(FALSE,0));SendMessageW(bar,TB_SETSTATE,500,MAKELONG(TBSTATE_ENABLED|TBSTATE_CHECKED,0));Pump();
    if((SendMessageW(bar,TB_GETSTATE,500,0)&TBSTATE_CHECKED) || !(SendMessageW(bar,TB_GETSTATE,109,0)&TBSTATE_CHECKED))return 11;
    if(!detach(bar,500))return 12;FreeLibrary(dll);DestroyWindow(owner);CloseDesktop(desktop);CloseWindowStation(station);
    std::puts("PASS: only Overview is selected, non-module and ambiguous states stay intact");return 0;
}
