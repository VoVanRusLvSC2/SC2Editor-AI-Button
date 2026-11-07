#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>

using Attach = BOOL (WINAPI*)(HWND, int, UINT);
using Detach = BOOL (WINAPI*)(HWND, UINT);
static HWND aiWindow;
static BOOL CALLBACK FindAI(HWND h, LPARAM) {
    wchar_t cls[128]{}; GetClassNameW(h,cls,128);
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v3")) aiWindow=h;
    return TRUE;
}
int wmain() {
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&common)) return 1;
    HWND parent = CreateWindowExW(0, L"STATIC", L"test", WS_OVERLAPPEDWINDOW,
                                  0, 0, 500, 100, nullptr, nullptr, nullptr, nullptr);
    HWND toolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr, WS_CHILD | TBSTYLE_FLAT,
                                   0, 0, 500, 30, parent, nullptr, nullptr, nullptr);
    if (!parent || !toolbar) return 2;
    SendMessageW(toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    TBBUTTON existing[2]{};
    for (int i = 0; i != 2; ++i) { existing[i].iBitmap = I_IMAGENONE; existing[i].idCommand = 100 + i; existing[i].fsState = TBSTATE_ENABLED; existing[i].fsStyle = BTNS_BUTTON; }
    if (!SendMessageW(toolbar, TB_ADDBUTTONSW, 2, reinterpret_cast<LPARAM>(existing))) return 3;
    HMODULE module = LoadLibraryW(L"SC2AIButton.dll");
    if (!module) return 4;
    auto attach = reinterpret_cast<Attach>(GetProcAddress(module, "SC2AI_Attach"));
    auto detach = reinterpret_cast<Detach>(GetProcAddress(module, "SC2AI_Detach"));
    if (!attach || !detach || attach(toolbar, 1, 100) || attach(toolbar, 3, 500) ||
        !attach(toolbar, 1, 500) || SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0) != 3 ||
        SendMessageW(toolbar, TB_COMMANDTOINDEX, 500, 0) != 1 ||
        SendMessageW(toolbar, TB_COMMANDTOINDEX, 101, 0) != 2 || detach(toolbar, 100)) return 5;
    TBBUTTON ai{};
    if (!SendMessageW(toolbar,TB_GETBUTTON,1,reinterpret_cast<LPARAM>(&ai)) || ai.iBitmap<0) return 11;
    SendMessageW(parent,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(toolbar));
    EnumChildWindows(parent,FindAI,0);
    RECT parentArea{},barRect{},panelRect{};
    GetClientRect(parent,&parentArea);GetWindowRect(toolbar,&barRect);GetWindowRect(aiWindow,&panelRect);
    MapWindowPoints(nullptr,parent,reinterpret_cast<POINT*>(&barRect),2);
    MapWindowPoints(nullptr,parent,reinterpret_cast<POINT*>(&panelRect),2);
    if (!IsWindow(aiWindow) || GetParent(aiWindow)!=parent || !(GetWindowLongPtrW(aiWindow,GWL_STYLE)&WS_VISIBLE) ||
        panelRect.top!=barRect.bottom || panelRect.right!=parentArea.right || panelRect.bottom!=parentArea.bottom) return 6;
    HWND first=aiWindow;
    SendMessageW(parent,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(toolbar));
    if (first!=aiWindow || !IsWindow(first)) return 7;
    SetWindowPos(parent,nullptr,0,0,650,180,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    GetClientRect(parent,&parentArea);GetWindowRect(aiWindow,&panelRect);
    MapWindowPoints(nullptr,parent,reinterpret_cast<POINT*>(&panelRect),2);
    if (panelRect.right!=parentArea.right || panelRect.bottom!=parentArea.bottom) return 15;
    SendMessageW(first,WM_CLOSE,0,0);
    if (IsWindow(first)) return 8;
    aiWindow=nullptr;
    SendMessageW(parent,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(toolbar));
    EnumChildWindows(parent,FindAI,0);
    if (!IsWindow(aiWindow)) return 9;
    if (!detach(toolbar, 500) || IsWindow(aiWindow) || SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0) != 2 ||
        SendMessageW(toolbar, TB_COMMANDTOINDEX, 101, 0) != 1) return 5;
    FreeLibrary(module);
    DestroyWindow(parent);
    std::puts("PASS: image button and child panel below toolbar, resize, repeat/close/reopen, removal");
    return 0;
}
