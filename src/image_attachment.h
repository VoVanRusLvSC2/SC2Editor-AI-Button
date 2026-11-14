#pragma once
#include <windows.h>
#include <string>
#include <vector>

// Creates an owned PNG copy and a small preview. The caller owns both outputs.
// Convert supported images to temporary PNGs and create previews owned by the caller.
bool ImageFromFile(const std::wstring &source, std::wstring &path, HBITMAP &preview);
bool ImageFromClipboard(HWND owner, std::wstring &path, HBITMAP &preview);
// Read file paths from the clipboard without treating unrelated clipboard data as images.
bool ClipboardImageFiles(HWND owner, std::vector<std::wstring> &files);
