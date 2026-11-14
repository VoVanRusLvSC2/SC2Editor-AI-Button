#define WIN32_LEAN_AND_MEAN
#include "image_attachment.h"
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <vector>
#include <memory>
#include <algorithm>

namespace {
// Convert input images to PNG; limits constrain the dimensions and temporary file size.
constexpr ULONGLONG kMaxPngBytes = 7ull * 1024 * 1024;
// Create a unique temporary PNG copy and a separate preview bitmap. Delete an incomplete file
// if an operation fails.
bool SaveImage(Gdiplus::Bitmap *input, std::wstring &path, HBITMAP &preview) {
    if (!input || input->GetLastStatus() != Gdiplus::Ok)
        return false;
    UINT width = input->GetWidth(), height = input->GetHeight();
    if (!width || !height || width > 8000 || height > 8000 ||
        static_cast<ULONGLONG>(width) * height > 40000000)
        return false;
    UINT count = 0, bytes = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok || !bytes)
        return false;
    std::vector<BYTE> storage(bytes);
    auto codecs = reinterpret_cast<Gdiplus::ImageCodecInfo *>(storage.data());
    if (Gdiplus::GetImageEncoders(count, bytes, codecs) != Gdiplus::Ok)
        return false;
    CLSID png{};
    bool found = false;
    for (UINT i = 0; i < count; ++i)
        if (!wcscmp(codecs[i].MimeType, L"image/png")) {
            png = codecs[i].Clsid;
            found = true;
            break;
        }
    if (!found)
        return false;
    wchar_t temp[32768]{};
    DWORD length = GetTempPathW(32768, temp);
    if (!length || length >= 32768)
        return false;
    std::wstring dir = std::wstring(temp) + L"SC2EditorAI";
    if (!CreateDirectoryW(dir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    wchar_t unique[MAX_PATH]{};
    if (!GetTempFileNameW(dir.c_str(), L"img", 0, unique))
        return false;
    std::wstring target = std::wstring(unique) + L".png";
    if (!MoveFileW(unique, target.c_str())) {
        DeleteFileW(unique);
        return false;
    }
    if (input->Save(target.c_str(), &png, nullptr) != Gdiplus::Ok) {
        DeleteFileW(target.c_str());
        return false;
    }
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(target.c_str(), GetFileExInfoStandard, &data) ||
        ((static_cast<ULONGLONG>(data.nFileSizeHigh) << 32) | data.nFileSizeLow) > kMaxPngBytes) {
        DeleteFileW(target.c_str());
        return false;
    }
    Gdiplus::Bitmap thumbnail(72, 54, PixelFormat32bppARGB);
    Gdiplus::Graphics g(&thumbnail);
    g.Clear(Gdiplus::Color(255, 255, 255, 255));
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    double scale = (std::min)(72.0 / width, 54.0 / height);
    int w = static_cast<int>(width * scale), h = static_cast<int>(height * scale);
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    g.DrawImage(input, (72 - w) / 2, (54 - h) / 2, w, h);
    HBITMAP bmp = nullptr;
    if (thumbnail.GetHBITMAP(Gdiplus::Color(255, 255, 255, 255), &bmp) != Gdiplus::Ok || !bmp) {
        DeleteFileW(target.c_str());
        return false;
    }
    path = target;
    preview = bmp;
    return true;
}
// Start GDI+ for the duration of the image operation and shut it down afterwards.
template <class F> bool WithGdi(F work) {
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput si;
    if (Gdiplus::GdiplusStartup(&token, &si, nullptr) != Gdiplus::Ok)
        return false;
    bool result = work();
    Gdiplus::GdiplusShutdown(token);
    return result;
}
} // namespace
// Load a selected file and pass it to the shared image conversion routine.
bool ImageFromFile(const std::wstring &source, std::wstring &path, HBITMAP &preview) {
    return WithGdi([&]() {
        std::unique_ptr<Gdiplus::Bitmap> image(Gdiplus::Bitmap::FromFile(source.c_str(), FALSE));
        return SaveImage(image.get(), path, preview);
    });
}
// The clipboard can contain a bitmap or a file path. Copy the bitmap before closing the
// clipboard because the original belongs to Windows.
bool ImageFromClipboard(HWND owner, std::wstring &path, HBITMAP &preview) {
    if (!OpenClipboard(owner))
        return false;
    HBITMAP copied = nullptr;
    std::wstring file;
    if (IsClipboardFormatAvailable(CF_BITMAP)) {
        auto bitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
        if (bitmap)
            copied = static_cast<HBITMAP>(CopyImage(bitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    } else if (IsClipboardFormatAvailable(CF_HDROP)) {
        auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
        wchar_t name[32768]{};
        if (drop && DragQueryFileW(drop, 0, name, 32768))
            file = name;
    }
    CloseClipboard();
    if (!file.empty())
        return ImageFromFile(file, path, preview);
    if (!copied)
        return false;
    bool ok = WithGdi([&]() {
        Gdiplus::Bitmap bitmap(copied, nullptr);
        return SaveImage(&bitmap, path, preview);
    });
    DeleteObject(copied);
    return ok;
}
// Read a bounded list of file paths from the clipboard. The UI and bridge apply additional
// attachment limits.
bool ClipboardImageFiles(HWND owner, std::vector<std::wstring> &files) {
    if (!OpenClipboard(owner))
        return false;
    auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
    UINT count = drop ? DragQueryFileW(drop, 0xffffffff, nullptr, 0) : 0;
    for (UINT i = 0; i < count && i < 6; ++i) {
        UINT length = DragQueryFileW(drop, i, nullptr, 0);
        if (!length || length >= 32768)
            break;
        std::wstring name(static_cast<size_t>(length) + 1, L'\0');
        if (!DragQueryFileW(drop, i, name.data(), length + 1))
            break;
        name.resize(length);
        files.push_back(name);
    }
    CloseClipboard();
    return !files.empty();
}
