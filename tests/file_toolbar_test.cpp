#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include "test_desktop.h"
#include <cwchar>

using Attach=BOOL (WINAPI*)(HWND,int,UINT);
using Detach=BOOL (WINAPI*)(HWND,UINT);
static HWND ai,sourceToolbar;
static int forwarded=-1;
static LRESULT CALLBACK OwnerProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_COMMAND && reinterpret_cast<HWND>(l)==sourceToolbar && LOWORD(w)!=500){forwarded=LOWORD(w);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
static BOOL CALLBACK Find(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v37"))ai=h;return TRUE;}
int wmain(){
    if(!UsePrivateTestDesktop()){std::fprintf(stderr,"Could not isolate the test desktop.\n");return 90;}
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common))return 1;
    WNDCLASSW cls{};cls.lpfnWndProc=OwnerProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SC2AI.FileToolbarTest";if(!RegisterClassW(&cls))return 2;
    HWND owner=CreateWindowExW(0,cls.lpszClassName,L"test",WS_OVERLAPPEDWINDOW,-30000,-30000,900,600,nullptr,nullptr,cls.hInstance,nullptr);if(!owner)return 3;
    sourceToolbar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT,0,0,900,30,owner,nullptr,nullptr,nullptr);if(!sourceToolbar)return 4;
    SendMessageW(sourceToolbar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
    const wchar_t labels[]=L"Save (Ctrl+S)\0New (Ctrl+N)\0Open (Ctrl+O)\0Other\0\0";
    LRESULT first=SendMessageW(sourceToolbar,TB_ADDSTRINGW,0,reinterpret_cast<LPARAM>(labels));if(first<0)return 5;
    TBBUTTON items[18]{};
    for(int i=0;i<18;++i){items[i].iBitmap=I_IMAGENONE;items[i].idCommand=700+i;items[i].fsState=TBSTATE_ENABLED;items[i].iString=first+3;}
    items[0].iString=first;items[1].iString=first+1;items[2].iString=first+2;
    items[2].fsState=0; // Native Open starts disabled.
    items[7].fsStyle=BTNS_SEP;items[7].iBitmap=8;
    if(!SendMessageW(sourceToolbar,TB_ADDBUTTONSW,18,reinterpret_cast<LPARAM>(items)))return 6;
    HMODULE dll=LoadLibraryW(L"SC2AIButton.dll");if(!dll)return 7;
    auto attach=reinterpret_cast<Attach>(GetProcAddress(dll,"SC2AI_Attach"));auto detach=reinterpret_cast<Detach>(GetProcAddress(dll,"SC2AI_Detach"));
    if(!attach||!detach||!attach(sourceToolbar,16,500))return 8;
    SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(sourceToolbar));EnumThreadWindows(GetCurrentThreadId(),Find,0);if(!ai)return 9;
    HWND bar=GetDlgItem(ai,100);if(!bar)return 10;
    if(!(SendMessageW(bar,TB_GETSTATE,700,0)&TBSTATE_ENABLED) || !(SendMessageW(bar,TB_GETSTATE,701,0)&TBSTATE_ENABLED) || (SendMessageW(bar,TB_GETSTATE,702,0)&TBSTATE_ENABLED) || (SendMessageW(bar,TB_GETSTATE,703,0)&TBSTATE_ENABLED))return 11;
    SendMessageW(ai,WM_COMMAND,MAKEWPARAM(700,0),reinterpret_cast<LPARAM>(bar));if(forwarded!=700)return 12;
    forwarded=-1;SendMessageW(ai,WM_COMMAND,MAKEWPARAM(702,0),reinterpret_cast<LPARAM>(bar));if(forwarded!=-1)return 13;
    SendMessageW(sourceToolbar,TB_ENABLEBUTTON,702,MAKELONG(TRUE,0));SendMessageW(sourceToolbar,TB_ENABLEBUTTON,700,MAKELONG(FALSE,0));SendMessageW(ai,WM_TIMER,1,0);
    if(!(SendMessageW(bar,TB_GETSTATE,702,0)&TBSTATE_ENABLED) || (SendMessageW(bar,TB_GETSTATE,700,0)&TBSTATE_ENABLED))return 14;
    SendMessageW(ai,WM_COMMAND,MAKEWPARAM(702,0),reinterpret_cast<LPARAM>(bar));if(forwarded!=702)return 15;
    forwarded=-1;SendMessageW(ai,WM_COMMAND,MAKEWPARAM(701,0),reinterpret_cast<LPARAM>(bar));if(forwarded!=701)return 16;
    SetWindowTextW(owner,L"Terrain - [Other.SC2Map] - StarCraft II Editor");SendMessageW(ai,WM_TIMER,1,0);
    wchar_t title[256]{};GetWindowTextW(ai,title,256);if(!wcsstr(title,L"[Other.SC2Map]"))return 18;
    if(!detach(sourceToolbar,500))return 17;FreeLibrary(dll);DestroyWindow(owner);
    std::puts("PASS: native New, Open, Save actions are detected, routed and track availability");return 0;
}
