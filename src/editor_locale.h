#pragma once
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <cwctype>

// Map known StarCraft II locale codes and Windows language names to catalog IDs.
inline std::wstring NormalizeEditorLocale(std::wstring value) {
    for (auto &ch : value)
        ch = static_cast<wchar_t>(towlower(ch));
    if (value == L"ruru" || value == L"ru" || value.rfind(L"ru-", 0) == 0)
        return L"ruRU";
    if (value == L"enus" || value == L"engb" || value == L"ensg" || value == L"en" ||
        value.rfind(L"en-", 0) == 0)
        return L"enUS";
    if (value == L"dede" || value == L"de" || value.rfind(L"de-", 0) == 0)
        return L"deDE";
    if (value == L"eses" || value == L"es" || value == L"es-es")
        return L"esES";
    if (value == L"esmx" || value.rfind(L"es-", 0) == 0)
        return L"esMX";
    if (value == L"ptbr" || value == L"ptpt" || value == L"pt" || value.rfind(L"pt-", 0) == 0)
        return L"ptBR";
    if (value == L"frfr" || value == L"fr" || value.rfind(L"fr-", 0) == 0)
        return L"frFR";
    if (value == L"itit" || value == L"it" || value.rfind(L"it-", 0) == 0)
        return L"itIT";
    if (value == L"kokr" || value == L"ko" || value.rfind(L"ko-", 0) == 0)
        return L"koKR";
    if (value == L"plpl" || value == L"pl" || value.rfind(L"pl-", 0) == 0)
        return L"plPL";
    if (value == L"zhtw" || value == L"zh-tw" || value == L"zh-hk" || value == L"zh-hant" ||
        value.rfind(L"zh-hant-", 0) == 0)
        return L"zhTW";
    if (value == L"zhcn" || value == L"zh" || value == L"zh-cn" || value == L"zh-sg" || value == L"zh-hans" ||
        value.rfind(L"zh-hans-", 0) == 0)
        return L"zhCN";
    return {};
}

// EditorVariables.txt is editor-specific; Variables.txt may describe only the game.
inline std::wstring EditorLocaleFromVariables(const std::string &bytes) {
    size_t start = 0;
    while (start < bytes.size()) {
        size_t end = bytes.find('\n', start);
        std::string line = bytes.substr(start, end == std::string::npos ? end : end - start);
        if (start == 0 && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
            line.erase(0, 3);
        if (line.rfind("localeiddata=", 0) == 0) {
            std::string code = line.substr(13);
            while (!code.empty() && (code.back() == '\r' || code.back() == ' ' || code.back() == '\t'))
                code.pop_back();
            return NormalizeEditorLocale(std::wstring(code.begin(), code.end()));
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return {};
}
inline std::wstring EditorLocaleFromMenu(std::wstring menu, const std::wstring &configured) {
    std::wstring clean;
    for (wchar_t ch : menu)
        if (ch != L'&')
            clean += ch;
    if (!clean.empty())
        CharLowerBuffW(clean.data(), static_cast<DWORD>(clean.size()));
    if (clean == L"файл")
        return L"ruRU";
    if (clean == L"datei")
        return L"deDE";
    if (clean == L"fichier")
        return L"frFR";
    if (clean == L"arquivo")
        return L"ptBR";
    if (clean == L"plik")
        return L"plPL";
    if (clean == L"파일")
        return L"koKR";
    if (clean == L"archivo")
        return configured == L"esMX" ? L"esMX" : L"esES";
    if (clean == L"文件")
        return configured == L"zhTW" ? L"zhTW" : L"zhCN";
    if (clean == L"file")
        return configured == L"itIT" ? L"itIT" : L"enUS";
    return {};
}
inline std::wstring ReadEditorLocale(HWND owner) {
    wchar_t overrideValue[80]{};
    DWORD n = GetEnvironmentVariableW(L"SC2_AI_LOCALE", overrideValue, 80);
    if (n && n < 80) {
        auto locale = NormalizeEditorLocale(overrideValue);
        if (!locale.empty())
            return locale;
    }
    std::wstring configured;
    wchar_t documents[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, documents))) {
        std::wstring file = std::wstring(documents) + L"\\StarCraft II\\EditorVariables.txt";
        HANDLE h =
            CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            char buffer[65536]{};
            DWORD read = 0;
            BOOL ok = ReadFile(h, buffer, sizeof(buffer), &read, nullptr);
            CloseHandle(h);
            if (ok)
                configured = EditorLocaleFromVariables(std::string(buffer, read));
        }
    }
    wchar_t menuText[128]{};
    HMENU menu = GetMenu(owner);
    if (menu && GetMenuStringW(menu, 0, menuText, 128, MF_BYPOSITION) > 0) {
        auto locale = EditorLocaleFromMenu(menuText, configured);
        if (!locale.empty())
            return locale;
    }
    if (!configured.empty())
        return configured;
    wchar_t windowsLocale[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(windowsLocale, LOCALE_NAME_MAX_LENGTH)) {
        auto locale = NormalizeEditorLocale(windowsLocale);
        if (!locale.empty())
            return locale;
    }
    return L"enUS";
}
