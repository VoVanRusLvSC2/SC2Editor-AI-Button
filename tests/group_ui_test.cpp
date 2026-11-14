// Check chat grouping, project colors, and sidebar selection.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include "../src/bridge_client.h"
using Attach = BOOL(WINAPI *)(HWND, int, UINT);
static HWND ai;
static BOOL CALLBACK Find(HWND h, LPARAM) {
    wchar_t cls[128]{};
    GetClassNameW(h, cls, 128);
    if (!lstrcmpW(cls, L"SC2AIButton.Window.v39"))
        ai = h;
    return TRUE;
}
int wmain() {
    // Exercise the built-in folder fallback independently of local optional textures.
    SetEnvironmentVariableW(L"SC2_AI_FOLDER_ICON_PATH", L"?:\\not-a-folder-texture.png");
    wchar_t name[80]{};
    swprintf_s(name, L"SC2AIMultiTest-%lu", GetCurrentProcessId());
    HWINSTA station = CreateWindowStationW(name, 0, WINSTA_ALL_ACCESS, nullptr);
    if (!station || !SetProcessWindowStation(station))
        return 1;
    HDESK desktop = CreateDesktopW(L"ImageTest", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desktop || !SetThreadDesktop(desktop))
        return 2;
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&common))
        return 3;
    HWND owner = CreateWindowExW(0, L"STATIC", L"test", WS_OVERLAPPEDWINDOW, 0, 0, 900, 600, nullptr, nullptr,
                                 nullptr, nullptr);
    HWND bar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT, 0, 0, 900,
                               30, owner, nullptr, nullptr, nullptr);
    if (!owner || !bar)
        return 4;
    SendMessageW(bar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    TBBUTTON buttons[2]{};
    for (int i = 0; i < 2; ++i) {
        buttons[i].iBitmap = I_IMAGENONE;
        buttons[i].idCommand = 100 + i;
        buttons[i].fsState = TBSTATE_ENABLED;
    }
    if (!SendMessageW(bar, TB_ADDBUTTONSW, 2, reinterpret_cast<LPARAM>(buttons)))
        return 5;
    HMODULE dll = LoadLibraryW(L"SC2AIButton.dll");
    if (!dll)
        return 6;
    auto attach = reinterpret_cast<Attach>(GetProcAddress(dll, "SC2AI_Attach"));
    if (!attach || !attach(bar, 1, 500))
        return 7;
    SendMessageW(owner, WM_COMMAND, MAKEWPARAM(500, 0), reinterpret_cast<LPARAM>(bar));
    EnumThreadWindows(GetCurrentThreadId(), Find, 0);
    if (!ai)
        return 8;
    auto event = [&](const char *kind, std::vector<std::wstring> fields = {}) {
        auto e = new BridgeEvent{nullptr, kind, fields};
        SendMessageW(ai, WM_AI_BRIDGE, 0, reinterpret_cast<LPARAM>(e));
    };
    event("threads_begin");
    event("thread", {L"own", L"Editor chat", L"", L"sc2-ai:in-editor", L"IN_EDITOR", L"blue"});
    event("thread", {L"one", L"First", L"", L"p1", L"Project One", L"purple"});
    event("thread", {L"two", L"Second", L"", L"p2", L"Project Two"});
    event("thread", {L"three", L"Third", L"", L"p1", L"Project One", L"purple"});
    event("threads_end");
    event("ready", {L"1"});
    HWND list = GetDlgItem(ai, 113);
    if (SendMessageW(list, LB_GETCOUNT, 0, 0) != 3)
        return 9;
    SendMessageW(ai, WM_COMMAND, MAKEWPARAM(120, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(ai, 120)));
    if (SendMessageW(list, LB_GETCOUNT, 0, 0) != 8)
        return 15;
    wchar_t text[128]{};
    SendMessageW(list, LB_GETTEXT, 3, reinterpret_cast<LPARAM>(text));
    if (!wcsstr(text, L"Project One"))
        return 10;
    HDC screen = GetDC(owner);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 220, 22);
    auto old = SelectObject(dc, bitmap);
    DRAWITEMSTRUCT item{};
    item.CtlID = 113;
    item.hwndItem = list;
    item.itemID = 3;
    item.hDC = dc;
    item.rcItem = {0, 0, 220, 22};
    SendMessageW(ai, WM_DRAWITEM, 113, reinterpret_cast<LPARAM>(&item));
    COLORREF folder = GetPixel(dc, 25, 11);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(owner, screen);
    if (folder != RGB(146, 79, 247))
        return 14;
    event("selected", {L"three"});
    if (SendMessageW(list, LB_GETCURSEL, 0, 0) != 5 || !IsWindowEnabled(GetDlgItem(ai, 122)))
        return 11;
    SendMessageW(list, LB_SETCURSEL, 3, 0);
    SendMessageW(ai, WM_COMMAND, MAKEWPARAM(113, LBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    if (SendMessageW(list, LB_GETCOUNT, 0, 0) != 6)
        return 12;
    SendMessageW(list, LB_SETCURSEL, 3, 0);
    SendMessageW(ai, WM_COMMAND, MAKEWPARAM(113, LBN_SELCHANGE), reinterpret_cast<LPARAM>(list));
    if (SendMessageW(list, LB_GETCOUNT, 0, 0) != 8 || SendMessageW(list, LB_GETCURSEL, 0, 0) != 5)
        return 13;
    SendMessageW(ai, WM_COMMAND, MAKEWPARAM(121, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(ai, 121)));
    if (SendMessageW(list, LB_GETCOUNT, 0, 0) != 3 || IsWindowEnabled(GetDlgItem(ai, 122)))
        return 16;
    DestroyWindow(ai);
    DestroyWindow(owner);
    CloseDesktop(desktop);
    CloseWindowStation(station);
    std::puts("PASS: editor-only and all folders, collapse, selection, and delete availability");
    return 0;
}
