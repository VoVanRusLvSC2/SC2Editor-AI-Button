#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <initializer_list>

using Attach = BOOL (WINAPI*)(HWND, int, UINT);
using Detach = BOOL (WINAPI*)(HWND, UINT);
static HWND aiWindow;
static BOOL CALLBACK FindAI(HWND h, LPARAM) {
    wchar_t cls[128]{}; GetClassNameW(h,cls,128);
    if (!lstrcmpW(cls,L"SC2AIButton.Window.v16") && !GetWindow(h,GW_OWNER)) aiWindow=h;
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
    HMENU menu=CreateMenu();if(!menu || !AppendMenuW(menu,MF_STRING,1,L"&File") || !SetMenu(parent,menu))return 25;
    ShowWindow(parent,SW_SHOWNOACTIVATE);
    SendMessageW(toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    TBBUTTON existing[3]{};
    for (int i : {0, 2}) { existing[i].iBitmap = I_IMAGENONE; existing[i].idCommand = 100 + i / 2; existing[i].fsState = TBSTATE_ENABLED; existing[i].fsStyle = BTNS_BUTTON; }
    existing[1].fsStyle = BTNS_SEP; existing[1].iBitmap = 8;
    if (!SendMessageW(toolbar, TB_ADDBUTTONSW, 3, reinterpret_cast<LPARAM>(existing))) return 3;
    HMODULE module = LoadLibraryW(L"SC2AIButton.dll");
    if (!module) return 4;
    auto attach = reinterpret_cast<Attach>(GetProcAddress(module, "SC2AI_Attach"));
    auto detach = reinterpret_cast<Detach>(GetProcAddress(module, "SC2AI_Detach"));
    if (!attach || !detach || attach(toolbar, 1, 100) || attach(toolbar, 4, 500) ||
        !attach(toolbar, 1, 500) || SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0) != 4 ||
        SendMessageW(toolbar, TB_COMMANDTOINDEX, 500, 0) != 1 ||
        SendMessageW(toolbar, TB_COMMANDTOINDEX, 101, 0) != 3 || detach(toolbar, 100)) return 5;
    TBBUTTON ai{};
    if (!SendMessageW(toolbar,TB_GETBUTTON,1,reinterpret_cast<LPARAM>(&ai)) || ai.iBitmap<0) return 11;
    SendMessageW(parent,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(toolbar));
    EnumThreadWindows(GetCurrentThreadId(),FindAI,reinterpret_cast<LPARAM>(parent));
    HWND aiBar=GetDlgItem(aiWindow,100);
    wchar_t localized[80]{};GetWindowTextW(GetDlgItem(aiWindow,106),localized,80);if(lstrcmpW(localized,L"CHATS"))return 26;
    wchar_t editClass[80]{};GetClassNameW(GetDlgItem(aiWindow,105),editClass,80);if(lstrcmpW(editClass,L"RICHEDIT50W"))return 27;
    RECT chatRect{}, transcriptRect{};GetWindowRect(GetDlgItem(aiWindow,113),&chatRect);GetWindowRect(GetDlgItem(aiWindow,105),&transcriptRect);
    if(chatRect.left>=transcriptRect.left || GetDlgItem(aiWindow,114) || GetDlgItem(aiWindow,115) || GetDlgItem(aiWindow,116))return 24;
    SendMessageW(aiWindow,WM_ACTIVATE,WA_ACTIVE,0);
    if(!IsWindow(aiWindow) || GetWindow(aiWindow,GW_OWNER) || !IsWindowVisible(aiWindow) || !aiBar ||
       !(GetWindowLongPtrW(aiWindow,GWL_STYLE)&WS_OVERLAPPEDWINDOW) ||
       !(SendMessageW(aiBar,TB_GETSTATE,500,0)&TBSTATE_CHECKED) ||
       !IsWindow(GetDlgItem(aiWindow,109)) || !IsWindow(GetDlgItem(aiWindow,110)) ||
       IsWindowEnabled(GetDlgItem(aiWindow,108)))return 6;
    if(SendMessageW(aiBar,TB_BUTTONCOUNT,0,0)!=3)return 21;
    for(int i=0;i<3;++i){TBBUTTON item{};if(!SendMessageW(aiBar,TB_GETBUTTON,i,reinterpret_cast<LPARAM>(&item)) || (item.fsStyle&BTNS_SEP))return 22;}
    TBBUTTON originalSeparator{};if(!SendMessageW(toolbar,TB_GETBUTTON,2,reinterpret_cast<LPARAM>(&originalSeparator)) || !(originalSeparator.fsStyle&BTNS_SEP))return 23;
    SendMessageW(aiWindow,WM_ACTIVATE,WA_INACTIVE,0);
    if(SendMessageW(aiBar,TB_GETSTATE,500,0)&TBSTATE_CHECKED)return 18;
    SendMessageW(aiWindow,WM_ACTIVATE,WA_ACTIVE,0);
    if(!(SendMessageW(aiBar,TB_GETSTATE,500,0)&TBSTATE_CHECKED))return 19;
    SendMessageW(aiBar,TB_CHECKBUTTON,500,MAKELPARAM(FALSE,0));
    SendMessageW(aiWindow,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(aiBar));
    if(!(SendMessageW(aiBar,TB_GETSTATE,500,0)&TBSTATE_CHECKED))return 20;

    SendMessageW(aiWindow,WM_COMMAND,MAKEWPARAM(109,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(aiWindow,109)));
    HWND settings=FindWindowExW(nullptr,nullptr,L"SC2AIButton.Settings.v16",nullptr);
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
    if (!detach(toolbar, 500) || IsWindow(aiWindow) || SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0) != 3 ||
        SendMessageW(toolbar, TB_COMMANDTOINDEX, 101, 0) != 2) return 5;
    if(!ModifyMenuW(menu,0,MF_BYPOSITION|MF_STRING,1,L"&Файл") || !attach(toolbar,1,500))return 28;
    aiWindow=nullptr;SendMessageW(parent,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(toolbar));EnumThreadWindows(GetCurrentThreadId(),FindAI,reinterpret_cast<LPARAM>(parent));
    GetWindowTextW(GetDlgItem(aiWindow,106),localized,80);if(lstrcmpW(localized,L"ЧАТЫ"))return 29;
    GetWindowTextW(GetDlgItem(aiWindow,109),localized,80);if(lstrcmpW(localized,L"Настройки") || !detach(toolbar,500))return 30;
    FreeLibrary(module);
    DestroyWindow(parent);
    std::puts("PASS: AI toolbar omits separators; selection and navigation remain correct");
    return 0;
}
