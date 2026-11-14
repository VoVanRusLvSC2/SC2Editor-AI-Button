// Check file conversion, temporary PNG output, and image validation.
#define WIN32_LEAN_AND_MEAN
#include "../src/image_attachment.h"
#include <cstdio>
int wmain() {
    wchar_t temp[MAX_PATH]{}, source[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, temp) || !GetTempFileNameW(temp, L"sc2", 0, source))
        return 1;
    HANDLE f = CreateFileW(source, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return 2;
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    BYTE pixels[16] = {0, 0, 255, 0, 255, 0, 0, 0, 255, 0, 0, 255, 255, 255, 0, 0};
    fh.bfType = 0x4d42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + sizeof(pixels);
    ih.biSize = sizeof(ih);
    ih.biWidth = 2;
    ih.biHeight = 2;
    ih.biPlanes = 1;
    ih.biBitCount = 24;
    ih.biSizeImage = sizeof(pixels);
    DWORD n = 0;
    bool wrote = WriteFile(f, &fh, sizeof(fh), &n, nullptr) && n == sizeof(fh) &&
                 WriteFile(f, &ih, sizeof(ih), &n, nullptr) && n == sizeof(ih) &&
                 WriteFile(f, pixels, sizeof(pixels), &n, nullptr) && n == sizeof(pixels);
    CloseHandle(f);
    std::wstring staged;
    HBITMAP preview = nullptr;
    bool ok = wrote && ImageFromFile(source, staged, preview);
    DeleteFileW(source);
    if (!ok || staged.empty() || !preview)
        return 3;
    BITMAP bm{};
    if (!GetObjectW(preview, sizeof(bm), &bm) || bm.bmWidth != 72 || bm.bmHeight != 54)
        return 4;
    f = CreateFileW(staged.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    BYTE signature[8]{};
    bool valid = f != INVALID_HANDLE_VALUE && ReadFile(f, signature, sizeof(signature), &n, nullptr) &&
                 n == sizeof(signature);
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    DeleteObject(preview);
    DeleteFileW(staged.c_str());
    const BYTE png[] = {137, 80, 78, 71, 13, 10, 26, 10};
    for (int i = 0; i < 8; ++i)
        if (signature[i] != png[i])
            valid = false;
    std::wstring missing;
    preview = nullptr;
    if (ImageFromFile(L"Z:\\not-present\\missing.png", missing, preview))
        return 5;
    if (!valid)
        return 6;
    std::puts("PASS: image decoded, copied as PNG and previewed; invalid file rejected");
    return 0;
}
