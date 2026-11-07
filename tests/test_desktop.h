#pragma once
#include <windows.h>
#include <cwchar>

// GUI tests create top-level windows, sometimes with different locales. A separate,
// noninteractive desktop prevents those windows from appearing on the user's screen.
// The process owns this handle until it exits, including on an early test failure.
inline bool UsePrivateTestDesktop(){
    wchar_t name[80]{};swprintf_s(name,L"SC2AI-Test-%lu",GetCurrentProcessId());
    HDESK desktop=CreateDesktopW(name,nullptr,nullptr,0,GENERIC_ALL,nullptr);
    return desktop && SetThreadDesktop(desktop);
}
