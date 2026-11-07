#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
using Detach=BOOL (WINAPI*)(HWND,UINT);
static HWND oldWindow,newWindow;
static BOOL CALLBACK Find(HWND h,LPARAM owner) {
    if (GetWindow(h,GW_OWNER)!=reinterpret_cast<HWND>(owner)) return TRUE;
    wchar_t cls[128]{}; GetClassNameW(h,cls,128);
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v1")) oldWindow=h;
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v2")) newWindow=h;
    return TRUE;
}
int wmain(int argc,wchar_t**argv) {
    if(argc!=2) return 1;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES}; if(!InitCommonControlsEx(&common)) return 2;
    HWND owner=CreateWindowExW(0,L"STATIC",L"test",WS_OVERLAPPEDWINDOW,0,0,500,100,nullptr,nullptr,nullptr,nullptr);
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|TBSTYLE_FLAT,0,0,500,30,owner,nullptr,nullptr,nullptr);
    if(!owner||!bar)return 3;
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
    SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));
    EnumThreadWindows(GetCurrentThreadId(),Find,reinterpret_cast<LPARAM>(owner));
    if(!IsWindow(oldWindow))return 8;
    HWND previous=oldWindow;
    if(!attach(bar,1,500))return 9;
    oldWindow=nullptr;newWindow=nullptr;EnumThreadWindows(GetCurrentThreadId(),Find,reinterpret_cast<LPARAM>(owner));
    if(IsWindow(previous)||!IsWindow(newWindow)||!IsZoomed(newWindow)||SendMessageW(bar,TB_BUTTONCOUNT,0,0)!=3)return 10;
    TBBUTTON button{}; if(!SendMessageW(bar,TB_GETBUTTON,1,reinterpret_cast<LPARAM>(&button)) || button.iBitmap<0)return 11;
    if(!detach(bar,500)||IsWindow(newWindow)||SendMessageW(bar,TB_BUTTONCOUNT,0,0)!=3)return 12;
    if(!SendMessageW(bar,TB_GETBUTTON,1,reinterpret_cast<LPARAM>(&button)) || button.iBitmap!=I_IMAGENONE)return 13;
    if(!oldDetach(bar,500)||SendMessageW(bar,TB_BUTTONCOUNT,0,0)!=2)return 14;
    FreeLibrary(current);FreeLibrary(old);DestroyWindow(owner);ImageList_Destroy(images);
    std::puts("PASS: legacy button upgraded in place; old window replaced by maximized AI window; legacy restored on detach");return 0;
}
