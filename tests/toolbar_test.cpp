#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>

using Attach = BOOL (WINAPI*)(HWND, int, UINT);
using Detach = BOOL (WINAPI*)(HWND, UINT);
static HWND aiWindow;
static BOOL CALLBACK FindAI(HWND h, LPARAM) {
    wchar_t cls[128]{}; GetClassNameW(h,cls,128);
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v8") && !GetWindow(h,GW_OWNER)) aiWindow=h;
    return TRUE;
}
int wmain() {
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&common)) return 1;
    HWND parent = CreateWindowExW(0, L"STATIC", L"test", WS_OVERLAPPEDWINDOW,
                                  -30000, -30000, 500, 180, nullptr, nullptr, nullptr, nullptr);
    HWND toolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr, WS_CHILD | TBSTYLE_FLAT,
                                   0, 0, 500, 30, parent, nullptr, nullptr, nullptr);
    if (!parent || !toolbar) return 2;
    ShowWindow(parent,SW_SHOWNOACTIVATE);
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
    EnumThreadWindows(GetCurrentThreadId(),FindAI,reinterpret_cast<LPARAM>(parent));
    HWND aiBar=GetDlgItem(aiWindow,100);
    if(!IsWindow(aiWindow) || GetWindow(aiWindow,GW_OWNER) || !IsWindowVisible(aiWindow) || !aiBar ||
       !(GetWindowLongPtrW(aiWindow,GWL_STYLE)&WS_OVERLAPPEDWINDOW) ||
       !(SendMessageW(aiBar,TB_GETSTATE,500,0)&TBSTATE_CHECKED) ||
       !IsWindow(GetDlgItem(aiWindow,109)) || !IsWindow(GetDlgItem(aiWindow,110)) ||
       IsWindowEnabled(GetDlgItem(aiWindow,108)))return 6;
    SendMessageW(aiWindow,WM_COMMAND,MAKEWPARAM(109,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(aiWindow,109)));
    HWND settings=FindWindowExW(nullptr,nullptr,L"SC2AIButton.Settings.v8",nullptr);
    if(!settings || GetWindow(settings,GW_OWNER)!=aiWindow || !GetDlgItem(settings,102) || !GetDlgItem(settings,118))return 16;
    SendMessageW(settings,WM_COMMAND,MAKEWPARAM(102,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(settings,102)));
    wchar_t label[80]{}; GetWindowTextW(GetDlgItem(aiWindow,103),label,80);
    if(lstrcmpW(label,L"Claude"))return 17;
    SendMessageW(settings,WM_CLOSE,0,0);
    HWND first=aiWindow;
    SendMessageW(parent,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(toolbar));
    if (first!=aiWindow || !IsWindow(first)) return 7;
    SendMessageW(first,WM_CLOSE,0,0);
    if (IsWindow(first)) return 8;
    aiWindow=nullptr;
    SendMessageW(parent,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(toolbar));
    EnumThreadWindows(GetCurrentThreadId(),FindAI,reinterpret_cast<LPARAM>(parent));
    if (!IsWindow(aiWindow)) return 9;
    if (!detach(toolbar, 500) || IsWindow(aiWindow) || SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0) != 2 ||
        SendMessageW(toolbar, TB_COMMANDTOINDEX, 101, 0) != 1) return 5;
    FreeLibrary(module);
    DestroyWindow(parent);
    std::puts("PASS: separate AI window, selected AI, provider selection, close/reopen, removal");
    return 0;
}
