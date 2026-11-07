#pragma once
#include <windows.h>
#include <commctrl.h>

// The process and Editor build are validated by the loader. Match module
// windows by their native controls rather than their translated captions.
// Находим панель по Win32-классам окна и элемента с ID 10, не завися от перевода заголовка. Это
// предположение проверено для поддерживаемой версии редактора.
inline HWND EditorModuleToolbar(HWND window){
    if(!IsWindowVisible(window) || !GetMenu(window))return nullptr;
    wchar_t cls[64]{};if(!GetClassNameW(window,cls,64) || lstrcmpW(cls,L"#32770"))return nullptr;
    HWND toolbar=GetDlgItem(window,10);if(!toolbar || GetParent(toolbar)!=window)return nullptr;
    if(!GetClassNameW(toolbar,cls,64) || lstrcmpiW(cls,TOOLBARCLASSNAMEW))return nullptr;
    return toolbar;
}
