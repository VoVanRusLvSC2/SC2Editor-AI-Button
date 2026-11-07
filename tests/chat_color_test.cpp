#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <commdlg.h>
#include <cstdio>
#include "../src/bridge_client.h"
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
static HWND ai;
static BOOL CALLBACK Find(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v20"))ai=h;return TRUE;}
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
    auto check=[&](const wchar_t* text,COLORREF expected){
        FINDTEXTEXW find{};find.chrg.cpMin=0;find.chrg.cpMax=-1;find.lpstrText=const_cast<wchar_t*>(text);
        if(SendMessageW(edit,EM_FINDTEXTEXW,FR_DOWN,reinterpret_cast<LPARAM>(&find))<0)return false;
        CHARRANGE selection{find.chrgText.cpMin,find.chrgText.cpMin+1};SendMessageW(edit,EM_EXSETSEL,0,reinterpret_cast<LPARAM>(&selection));
        CHARFORMAT2W format{};format.cbSize=sizeof(format);SendMessageW(edit,EM_GETCHARFORMAT,SCF_SELECTION,reinterpret_cast<LPARAM>(&format));return (format.dwMask&CFM_COLOR) && format.crTextColor==expected;
    };
    event("history",{L"You: usertext\r\n\r\nAI: firsttext\r\n\r\nsecondtext\r\n\r\n[bracketed reply]\r\n\r\n[Command]\r\n\r\nAI: thirdtext"});
    if(!check(L"usertext",RGB(35,40,46)) || !check(L"firsttext",RGB(0,105,170)) || !check(L"secondtext",RGB(0,105,170)) || !check(L"[bracketed reply]",RGB(0,105,170)) || !check(L"[Command]",RGB(104,113,123)) || !check(L"thirdtext",RGB(0,105,170)))return 9;
    event("line",{L"AI: "});event("delta",{L"streamed text"});
    if(!check(L"streamed text",RGB(0,105,170)))return 10;
    DestroyWindow(ai);DestroyWindow(owner);CloseDesktop(desktop);CloseWindowStation(station);
    std::puts("PASS: assistant text is blue in history and streamed replies; user and command colors remain distinct");return 0;
}
