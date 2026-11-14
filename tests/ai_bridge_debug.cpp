// Inspect the AI window and bridge status in a running editor process.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
static DWORD pid;
static HWND ai;
static BOOL CALLBACK Find(HWND h, LPARAM) {
    DWORD p = 0;
    GetWindowThreadProcessId(h, &p);
    if (p != pid)
        return TRUE;
    wchar_t c[128]{};
    GetClassNameW(h, c, 128);
    if (!lstrcmpW(c, L"SC2AIButton.Window.v39") || !lstrcmpW(c, L"SC2AIButton.Window.v23") ||
        !lstrcmpW(c, L"SC2AIButton.Window.v22") || !lstrcmpW(c, L"SC2AIButton.Window.v21") ||
        !lstrcmpW(c, L"SC2AIButton.Window.v20") || !lstrcmpW(c, L"SC2AIButton.Window.v19") ||
        !lstrcmpW(c, L"SC2AIButton.Window.v18") || !lstrcmpW(c, L"SC2AIButton.Window.v17") ||
        !lstrcmpW(c, L"SC2AIButton.Window.v16") || !lstrcmpW(c, L"SC2AIButton.Window.v15") ||
        !lstrcmpW(c, L"SC2AIButton.Window.v14") || !lstrcmpW(c, L"SC2AIButton.Window.v13") ||
        !lstrcmpW(c, L"SC2AIButton.Window.v12") || !lstrcmpW(c, L"SC2AIButton.Window.v11"))
        ai = h;
    return TRUE;
}
int wmain(int argc, wchar_t **argv) {
    if (argc != 3)
        return 1;
    pid = wcstoul(argv[2], nullptr, 10);
    HDESK d = OpenDesktopW(argv[1], 0, FALSE, DESKTOP_ENUMERATE | DESKTOP_READOBJECTS);
    if (!d || !SetThreadDesktop(d))
        return 2;
    EnumDesktopWindows(d, Find, 0);
    if (!ai)
        return 3;
    HWND text = GetDlgItem(ai, 105), input = GetDlgItem(ai, 107), send = GetDlgItem(ai, 108),
         stop = GetDlgItem(ai, 116), status = GetDlgItem(ai, 104);
    wchar_t s[300]{};
    SendMessageW(status, WM_GETTEXT, 300, reinterpret_cast<LPARAM>(s));
    wchar_t body[4096]{};
    SendMessageW(text, WM_GETTEXT, 4096, reinterpret_cast<LPARAM>(body));
    std::wprintf(L"sendEnabled=%d stopEnabled=%d inputChars=%lld transcriptChars=%lld status=%ls hasError=%d "
                 L"hasUser=%d hasAI=%d\n",
                 IsWindowEnabled(send), IsWindowEnabled(stop),
                 static_cast<long long>(SendMessageW(input, WM_GETTEXTLENGTH, 0, 0)),
                 static_cast<long long>(SendMessageW(text, WM_GETTEXTLENGTH, 0, 0)), s,
                 wcsstr(body, L"[\x041e\x0448\x0438\x0431\x043a\x0430]") != nullptr,
                 wcsstr(body, L"\x0412\x044b:") != nullptr, wcsstr(body, L"AI:") != nullptr);
    return 0;
}
