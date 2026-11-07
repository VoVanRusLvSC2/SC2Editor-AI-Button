#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

static constexpr wchar_t kProperty[] = L"SC2AIButton.prototype.command";

// Attach and detach must run on the toolbar's own UI thread. The caller owns
// the DLL lifetime and must detach before unloading it.
extern "C" __declspec(dllexport) BOOL WINAPI SC2AI_Attach(HWND toolbar, int index, UINT command) {
    wchar_t cls[64]{};
    DWORD pid = 0;
    DWORD thread = GetWindowThreadProcessId(toolbar, &pid);
    if (!IsWindow(toolbar) || pid != GetCurrentProcessId() || thread != GetCurrentThreadId() ||
        !GetClassNameW(toolbar, cls, 64) || lstrcmpiW(cls, TOOLBARCLASSNAMEW) ||
        command == 0 || command > 0xffff || GetPropW(toolbar, kProperty)) return FALSE;
    const auto count = SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0);
    if (count < 0 || index < 0 || index > count ||
        SendMessageW(toolbar, TB_COMMANDTOINDEX, command, 0) != -1) return FALSE;
    TBBUTTON button{};
    button.iBitmap = I_IMAGENONE;
    button.idCommand = static_cast<int>(command);
    button.fsState = TBSTATE_ENABLED;
    button.fsStyle = BTNS_BUTTON | BTNS_AUTOSIZE | BTNS_SHOWTEXT;
    button.iString = reinterpret_cast<INT_PTR>(L"AI");
    if (!SetPropW(toolbar, kProperty, reinterpret_cast<HANDLE>(static_cast<UINT_PTR>(command)))) return FALSE;
    if (!SendMessageW(toolbar, TB_INSERTBUTTONW, index, reinterpret_cast<LPARAM>(&button))) {
        RemovePropW(toolbar, kProperty);
        return FALSE;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL WINAPI SC2AI_Detach(HWND toolbar, UINT command) {
    DWORD pid = 0;
    DWORD thread = GetWindowThreadProcessId(toolbar, &pid);
    if (!IsWindow(toolbar) || pid != GetCurrentProcessId() || thread != GetCurrentThreadId() ||
        reinterpret_cast<UINT_PTR>(GetPropW(toolbar, kProperty)) != command || !command) return FALSE;
    LRESULT index = SendMessageW(toolbar, TB_COMMANDTOINDEX, command, 0);
    if (index < 0) return FALSE;
    if (!SendMessageW(toolbar, TB_DELETEBUTTON, index, 0)) return FALSE;
    RemovePropW(toolbar, kProperty);
    return TRUE;
}
