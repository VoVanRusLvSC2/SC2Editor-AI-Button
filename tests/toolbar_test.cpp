#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>

using Attach = BOOL (WINAPI*)(HWND, int, UINT);
using Detach = BOOL (WINAPI*)(HWND, UINT);
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
        SendMessageW(toolbar, TB_COMMANDTOINDEX, 101, 0) != 2 || detach(toolbar, 100) ||
        !detach(toolbar, 500) || SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0) != 2 ||
        SendMessageW(toolbar, TB_COMMANDTOINDEX, 101, 0) != 1) return 5;
    FreeLibrary(module);
    DestroyWindow(parent);
    std::puts("PASS: insertion, position, conflict rejection and removal");
    return 0;
}
