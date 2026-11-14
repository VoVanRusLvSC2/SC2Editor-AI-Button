#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
// BridgeState owns the pipes and child process; its implementation is private to the bridge client.
struct BridgeState;
// Events are transferred to the UI thread with WM_AI_BRIDGE and carry decoded text fields.
struct BridgeEvent {
    BridgeState *source;
    std::string kind;
    std::vector<std::wstring> fields;
};
constexpr UINT WM_AI_BRIDGE = WM_APP + 501;
BridgeState *StartBridge(HWND window, HMODULE module, const std::wstring &workspace, bool claude,
                         const std::wstring &locale);
bool SendBridge(BridgeState *state, const char *command, const std::vector<std::wstring> &fields = {});
void StopBridge(BridgeState *state);
