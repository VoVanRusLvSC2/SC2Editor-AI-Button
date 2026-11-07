#pragma once
#include <windows.h>
#include <string>
#include <vector>

// Creates an owned PNG copy and a small preview. The caller owns both outputs.
bool ImageFromFile(const std::wstring& source,std::wstring& path,HBITMAP& preview);
bool ImageFromClipboard(HWND owner,std::wstring& path,HBITMAP& preview);
bool ClipboardImageFiles(HWND owner,std::vector<std::wstring>& files);
