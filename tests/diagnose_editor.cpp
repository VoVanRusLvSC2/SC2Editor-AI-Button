#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
static DWORD target;
struct Stats { HWND owner;HWND panel;int count; };
static BOOL CALLBACK Child(HWND h,LPARAM raw) {
    auto s=reinterpret_cast<Stats*>(raw);wchar_t cls[128]{};GetClassNameW(h,cls,128);
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v3")) {s->panel=h;++s->count;}
    return TRUE;
}
static BOOL CALLBACK Owned(HWND h,LPARAM raw) {
    auto s=reinterpret_cast<Stats*>(raw);wchar_t cls[128]{};GetClassNameW(h,cls,128);
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v4") && GetWindow(h,GW_OWNER)==s->owner) {s->panel=h;++s->count;}
    return TRUE;
}
static BOOL CALLBACK Window(HWND h,LPARAM) {
    DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(pid!=target)return TRUE;
    wchar_t cls[128]{},title[1024]{};GetClassNameW(h,cls,128);GetWindowTextW(h,title,1024);
    if (lstrcmpW(cls,L"#32770") || !wcsstr(title,L" - StarCraft II Editor")) return TRUE;
    HWND bar=GetDlgItem(h,10);wchar_t bclass[128]{};if(!bar || !GetClassNameW(bar,bclass,128) || lstrcmpiW(bclass,TOOLBARCLASSNAMEW))return TRUE;
    DWORD_PTR index=0;bool answered=SendMessageTimeoutW(bar,TB_COMMANDTOINDEX,60000,0,SMTO_ABORTIFHUNG,1000,&index)!=0;
    Stats stats{};stats.owner=h;EnumChildWindows(h,Child,reinterpret_cast<LPARAM>(&stats));EnumWindows(Owned,reinterpret_cast<LPARAM>(&stats));
    RECT br{},pr{};GetWindowRect(bar,&br);if(stats.panel)GetWindowRect(stats.panel,&pr);
    POINT point{pr.left+100,pr.top+100};
    HWND top=stats.panel?WindowFromPoint(point):nullptr;
    std::printf("MODULE visible=%d enabled=%d button=%s index=%lld v1=%d v2=%d v3=%d v4=%d panels=%d panelVisible=%d panelAtFront=%d panelTop=%ld toolbarBottom=%ld\n",
       IsWindowVisible(h),IsWindowEnabled(h),answered?"answered":"timeout",static_cast<long long>(index),
       GetPropW(bar,L"SC2AIButton.context.v1")!=nullptr,GetPropW(bar,L"SC2AIButton.context.v2")!=nullptr,
       GetPropW(bar,L"SC2AIButton.context.v3")!=nullptr,GetPropW(bar,L"SC2AIButton.context.v4")!=nullptr,
       stats.count,stats.panel?IsWindowVisible(stats.panel):0,stats.panel && top && GetAncestor(top,GA_ROOT)==stats.panel,pr.top,br.bottom);
    return TRUE;
}
int wmain(int argc,wchar_t**argv) {if(argc!=2)return 1;target=wcstoul(argv[1],nullptr,10);if(!target)return 2;EnumWindows(Window,0);return 0;}
