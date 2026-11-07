#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
static HWND ai;
static BOOL CALLBACK Find(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v28"))ai=h;return TRUE;}
static void Pump(){MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(20);}
static bool Wait(bool(*check)(void*),void* p,DWORD ms){DWORD end=GetTickCount()+ms;while(GetTickCount()<end){Pump();if(check(p))return true;}return false;}
static bool Ready(void* raw){auto w=static_cast<HWND>(raw);return IsWindowEnabled(GetDlgItem(w,107))&&SendMessageW(GetDlgItem(w,113),LB_GETCOUNT,0,0)>=3;}
static bool ErrorShown(void* raw){auto w=static_cast<HWND>(raw);wchar_t text[12000]{};GetWindowTextW(GetDlgItem(w,105),text,12000);return wcsstr(text,L"another Codex process")!=nullptr && IsWindowEnabled(GetDlgItem(w,107));}
static bool Cleared(void* raw){auto w=static_cast<HWND>(raw);return GetWindowTextLengthW(GetDlgItem(w,107))==0 && !SendMessageW(GetDlgItem(w,130),STM_GETIMAGE,IMAGE_BITMAP,0);}
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 1;wchar_t stationName[80]{};swprintf_s(stationName,L"SC2AILockTest-%lu",GetCurrentProcessId());
    HWINSTA station=CreateWindowStationW(stationName,0,WINSTA_ALL_ACCESS,nullptr);if(!station||!SetProcessWindowStation(station))return 2;
    HDESK desktop=CreateDesktopW(L"LockTest",nullptr,nullptr,0,GENERIC_ALL,nullptr);if(!desktop||!SetThreadDesktop(desktop))return 3;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common))return 4;
    HWND owner=CreateWindowExW(0,L"STATIC",L"Terrain - test",WS_OVERLAPPEDWINDOW,0,0,900,600,nullptr,nullptr,nullptr,nullptr);if(!owner)return 5;
    HMENU menu=CreateMenu();AppendMenuW(menu,MF_STRING,1,L"&File");SetMenu(owner,menu);
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT,0,0,900,30,owner,nullptr,nullptr,nullptr);if(!bar)return 6;
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);TBBUTTON buttons[2]{};for(int i=0;i<2;++i){buttons[i].iBitmap=I_IMAGENONE;buttons[i].idCommand=100+i;buttons[i].fsState=TBSTATE_ENABLED;}
    if(!SendMessageW(bar,TB_ADDBUTTONSW,2,reinterpret_cast<LPARAM>(buttons)))return 7;
    HMODULE dll=LoadLibraryW(argv[1]);if(!dll)return 8;auto attach=reinterpret_cast<Attach>(GetProcAddress(dll,"SC2AI_Attach"));if(!attach||!attach(bar,1,500))return 9;
    SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));EnumThreadWindows(GetCurrentThreadId(),Find,0);if(!ai)return 10;
    SendMessageW(ai,WM_COMMAND,MAKEWPARAM(120,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(ai,120)));if(!Wait(Ready,ai,30000))return 10;
    HWND list=GetDlgItem(ai,113),input=GetDlgItem(ai,107);SendMessageW(list,LB_SETCURSEL,3,0);SendMessageW(ai,WM_COMMAND,MAKEWPARAM(113,LBN_SELCHANGE),reinterpret_cast<LPARAM>(list));
    for(int i=0;i<100;++i)Pump();if(!IsWindowEnabled(input))return 11;
    HDC dc=GetDC(owner);HBITMAP bitmap=CreateCompatibleBitmap(dc,4,4);ReleaseDC(owner,dc);if(!bitmap||!OpenClipboard(ai))return 12;
    if(!EmptyClipboard()||!SetClipboardData(CF_BITMAP,bitmap))return 13;CloseClipboard();SendMessageW(input,WM_PASTE,0,0);
    if(!SendMessageW(GetDlgItem(ai,130),STM_GETIMAGE,IMAGE_BITMAP,0))return 14;
    BYTE keys[256]{};if(!GetKeyboardState(keys))return 18;BYTE oldShift=keys[VK_SHIFT];keys[VK_SHIFT]|=0x80;if(!SetKeyboardState(keys))return 19;
    SetWindowTextW(input,L"First");SendMessageW(input,EM_SETSEL,5,5);SendMessageW(input,WM_CHAR,L'\r',1);
    wchar_t multiline[128]{};GetWindowTextW(input,multiline,128);if(!wcschr(multiline,L'\n'))return 20;
    keys[VK_SHIFT]=oldShift;if(!SetKeyboardState(keys))return 21;
    SetWindowTextW(input,L"Keep this draft");SendMessageW(input,WM_CHAR,L'\r',1);
    if(!Wait(ErrorShown,ai,30000))return 15;wchar_t text[128]{};GetWindowTextW(input,text,128);
    if(lstrcmpW(text,L"Keep this draft")||!SendMessageW(GetDlgItem(ai,130),STM_GETIMAGE,IMAGE_BITMAP,0))return 16;
    SendMessageW(input,WM_CHAR,L'\r',1);SendMessageW(input,WM_CHAR,L'\r',1);if(!Wait(Cleared,ai,30000))return 17;
    for(int i=0;i<50;++i)Pump();DestroyWindow(ai);DestroyWindow(owner);CloseDesktop(desktop);CloseWindowStation(station);
    std::puts("PASS: Enter sends, Shift+Enter adds a line, and writer conflict preserves the draft");return 0;
}
