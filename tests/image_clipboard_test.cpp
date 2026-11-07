#define WIN32_LEAN_AND_MEAN
#include "../src/image_attachment.h"
#include <cstdio>
int wmain(){
    wchar_t name[80]{};swprintf_s(name,L"SC2AIClipboardTest-%lu",GetCurrentProcessId());
    HWINSTA station=CreateWindowStationW(name,0,WINSTA_ALL_ACCESS,nullptr);if(!station)return 1;
    if(!SetProcessWindowStation(station))return 2;
    HDESK desktop=CreateDesktopW(L"ClipboardTest",nullptr,nullptr,0,GENERIC_ALL,nullptr);if(!desktop||!SetThreadDesktop(desktop))return 3;
    HWND window=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPED,0,0,10,10,nullptr,nullptr,nullptr,nullptr);if(!window)return 4;
    HDC dc=GetDC(window);HBITMAP bitmap=CreateCompatibleBitmap(dc,4,4);ReleaseDC(window,dc);if(!bitmap)return 5;
    if(!OpenClipboard(window))return 6;if(!EmptyClipboard() || !SetClipboardData(CF_BITMAP,bitmap)){CloseClipboard();DeleteObject(bitmap);return 7;}CloseClipboard();
    std::wstring path;HBITMAP preview=nullptr;bool ok=ImageFromClipboard(window,path,preview);
    if(preview)DeleteObject(preview);if(!path.empty())DeleteFileW(path.c_str());
    HGLOBAL dib=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,sizeof(BITMAPINFOHEADER)+64);if(!dib)return 9;
    auto header=static_cast<BITMAPINFOHEADER*>(GlobalLock(dib));if(!header)return 10;
    header->biSize=sizeof(*header);header->biWidth=4;header->biHeight=4;header->biPlanes=1;header->biBitCount=32;header->biCompression=BI_RGB;GlobalUnlock(dib);
    if(!OpenClipboard(window))return 11;if(!EmptyClipboard() || !SetClipboardData(CF_DIB,dib)){CloseClipboard();GlobalFree(dib);return 12;}CloseClipboard();
    path.clear();preview=nullptr;ok=ok && ImageFromClipboard(window,path,preview);if(preview)DeleteObject(preview);if(!path.empty())DeleteFileW(path.c_str());
    DestroyWindow(window);CloseDesktop(desktop);CloseWindowStation(station);
    if(!ok)return 8;std::puts("PASS: image pasted from isolated clipboard and preview created");return 0;
}
