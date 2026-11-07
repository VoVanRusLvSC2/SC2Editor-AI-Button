#pragma once
#include <windows.h>
#include <string>

// Creates an owned PNG copy and a small preview. The caller owns both outputs.
bool ImageFromFile(const std::wstring& source,std::wstring& path,HBITMAP& preview);
bool ImageFromClipboard(HWND owner,std::wstring& path,HBITMAP& preview);
