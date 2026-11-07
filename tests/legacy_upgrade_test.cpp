#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
using Detach=BOOL (WINAPI*)(HWND,UINT);
static HWND oldWindow,newWindow;
static BOOL CALLBACK Find(HWND h,LPARAM owner) {
    wchar_t cls[128]{}; GetClassNameW(h,cls,128);
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v1") || !lstrcmpW(cls,L"SC2AIButton.Window.v2") || !lstrcmpW(cls,L"SC2AIButton.Window.v4") || !lstrcmpW(cls,L"SC2AIButton.Window.v5") || !lstrcmpW(cls,L"SC2AIButton.Window.v6")) { if(GetWindow(h,GW_OWNER)==reinterpret_cast<HWND>(owner))oldWindow=h; }
    if ((!lstrcmpW(cls,L"SC2AIButton.Window.v7") || !lstrcmpW(cls,L"SC2AIButton.Window.v8") || !lstrcmpW(cls,L"SC2AIButton.Window.v9") || !lstrcmpW(cls,L"SC2AIButton.Window.v10") || !lstrcmpW(cls,L"SC2AIButton.Window.v11") || !lstrcmpW(cls,L"SC2AIButton.Window.v12") || !lstrcmpW(cls,L"SC2AIButton.Window.v13") || !lstrcmpW(cls,L"SC2AIButton.Window.v14") || !lstrcmpW(cls,L"SC2AIButton.Window.v15") || !lstrcmpW(cls,L"SC2AIButton.Window.v16") || !lstrcmpW(cls,L"SC2AIButton.Window.v17") || !lstrcmpW(cls,L"SC2AIButton.Window.v18") || !lstrcmpW(cls,L"SC2AIButton.Window.v19") || !lstrcmpW(cls,L"SC2AIButton.Window.v20") || !lstrcmpW(cls,L"SC2AIButton.Window.v21")) && !GetWindow(h,GW_OWNER))oldWindow=h;
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v22")) newWindow=h;
    return TRUE;
}
static BOOL CALLBACK FindChild(HWND h,LPARAM) {
    wchar_t cls[128]{}; GetClassNameW(h,cls,128);
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v3")) oldWindow=h;
    return TRUE;
}
int wmain(int argc,wchar_t**argv) {
    if(argc!=2) return 1;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES}; if(!InitCommonControlsEx(&common)) return 2;
    HWND owner=CreateWindowExW(0,L"STATIC",L"test",WS_OVERLAPPEDWINDOW,-30000,-30000,500,180,nullptr,nullptr,nullptr,nullptr);
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|TBSTYLE_FLAT,0,0,500,30,owner,nullptr,nullptr,nullptr);
    if(!owner||!bar)return 3;
    ShowWindow(owner,SW_SHOWNOACTIVATE);
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
    HIMAGELIST images=ImageList_Create(24,24,ILC_COLOR32|ILC_MASK,0,1); if(!images)return 4;
    SendMessageW(bar,TB_SETIMAGELIST,0,reinterpret_cast<LPARAM>(images));
    TBBUTTON buttons[2]{};for(int i=0;i<2;++i){buttons[i].iBitmap=I_IMAGENONE;buttons[i].idCommand=100+i;buttons[i].fsState=TBSTATE_ENABLED;}
    if(!SendMessageW(bar,TB_ADDBUTTONSW,2,reinterpret_cast<LPARAM>(buttons)))return 5;
    wchar_t currentPath[32768]{};
    if (!GetFullPathNameW(L"SC2AIButton.dll",32768,currentPath,nullptr)) return 6;
    HMODULE old=LoadLibraryW(argv[1]),current=LoadLibraryW(currentPath);if(!old||!current||old==current)return 6;
    auto oldAttach=reinterpret_cast<Attach>(GetProcAddress(old,"SC2AI_Attach"));
    auto oldDetach=reinterpret_cast<Detach>(GetProcAddress(old,"SC2AI_Detach"));
    auto attach=reinterpret_cast<Attach>(GetProcAddress(current,"SC2AI_Attach"));
    auto detach=reinterpret_cast<Detach>(GetProcAddress(current,"SC2AI_Detach"));
    if(!oldAttach||!oldDetach||!attach||!detach||!oldAttach(bar,1,500))return 7;
    TBBUTTON before{}; if(!SendMessageW(bar,TB_GETBUTTON,1,reinterpret_cast<LPARAM>(&before)))return 15;
    SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));
    EnumThreadWindows(GetCurrentThreadId(),Find,reinterpret_cast<LPARAM>(owner));
    EnumChildWindows(owner,FindChild,0);
    if(!IsWindow(oldWindow))return 8;
    HWND previous=oldWindow;
    wchar_t oldClass[128]{};GetClassNameW(oldWindow,oldClass,128);
    if(!lstrcmpW(oldClass,L"SC2AIButton.Window.v21")){
        for(int i=0;i<300 && !IsWindowEnabled(GetDlgItem(oldWindow,107));++i){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(50);}
        if(!IsWindowEnabled(GetDlgItem(oldWindow,107)))return 16;
        SetWindowTextW(GetDlgItem(oldWindow,107),L"unsent draft");
        if(attach(bar,1,500) || !IsWindow(oldWindow) || GetWindowTextLengthW(GetDlgItem(oldWindow,107))==0)return 17;
        SetWindowTextW(GetDlgItem(oldWindow,107),L"");
    }
    if(!attach(bar,1,500))return 9;
    oldWindow=nullptr;newWindow=nullptr;EnumThreadWindows(GetCurrentThreadId(),Find,reinterpret_cast<LPARAM>(owner));EnumChildWindows(owner,FindChild,0);
    if(IsWindow(previous)||!IsWindow(newWindow)||GetWindow(newWindow,GW_OWNER)!=nullptr||SendMessageW(bar,TB_BUTTONCOUNT,0,0)!=3)return 10;
    TBBUTTON button{}; if(!SendMessageW(bar,TB_GETBUTTON,1,reinterpret_cast<LPARAM>(&button)) || button.iBitmap<0)return 11;
    if(!detach(bar,500)||IsWindow(newWindow)||SendMessageW(bar,TB_BUTTONCOUNT,0,0)!=3)return 12;
    if(!SendMessageW(bar,TB_GETBUTTON,1,reinterpret_cast<LPARAM>(&button)) || button.iBitmap!=before.iBitmap)return 13;
    if(!oldDetach(bar,500)||SendMessageW(bar,TB_BUTTONCOUNT,0,0)!=2)return 14;
    FreeLibrary(current);FreeLibrary(old);DestroyWindow(owner);ImageList_Destroy(images);
    std::puts("PASS: legacy button upgraded in place; window replaced with separate AI window; legacy restored on detach");return 0;
}
