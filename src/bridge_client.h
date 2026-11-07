#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
struct BridgeState;
struct BridgeEvent { BridgeState* source; std::string kind; std::vector<std::wstring> fields; };
constexpr UINT WM_AI_BRIDGE = WM_APP + 501;
BridgeState* StartBridge(HWND window, HMODULE module, const std::wstring& workspace, bool claude, bool russian);
bool SendBridge(BridgeState* state, const char* command, const std::vector<std::wstring>& fields = {});
void StopBridge(BridgeState* state);
