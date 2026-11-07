#define WIN32_LEAN_AND_MEAN
#include "../src/editor_window.h"
#include <cstdio>
#pragma pack(push,2)
struct DialogTemplate { DLGTEMPLATE dialog;WORD menu,windowClass,title; };
#pragma pack(pop)
int wmain(){
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common))return 1;
    DialogTemplate t{};t.dialog.style=WS_POPUP|WS_CAPTION;t.dialog.cx=300;t.dialog.cy=200;
    HWND owner=CreateDialogIndirectParamW(GetModuleHandleW(nullptr),&t.dialog,nullptr,nullptr,0);if(!owner)return 2;
    HMENU menu=CreateMenu();if(!menu||!AppendMenuW(menu,MF_STRING,1,L"File")||!SetMenu(owner,menu))return 3;
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|WS_VISIBLE,0,0,300,30,owner,reinterpret_cast<HMENU>(10),GetModuleHandleW(nullptr),nullptr);if(!bar)return 4;
    ShowWindow(owner,SW_SHOWNOACTIVATE);
    const wchar_t* titles[]={L"Terrain - [D:/Project/Test.SC2Map] - StarCraft II Editor",L"Территория - [D:/Проект/Test.SC2Map] - Редактор StarCraft II",L"Terrain - [D:/Projet/Test.SC2Map] - Éditeur StarCraft II",L"地图 - [D:/项目/Test.SC2Map] - 星际争霸 II 编辑器"};
    for(const auto title:titles){SetWindowTextW(owner,title);if(EditorModuleToolbar(owner)!=bar)return 5;}
    SetMenu(owner,nullptr);if(EditorModuleToolbar(owner))return 6;SetMenu(owner,menu);
    SetWindowLongPtrW(bar,GWLP_ID,11);if(EditorModuleToolbar(owner))return 7;
    SetWindowLongPtrW(bar,GWLP_ID,10);ShowWindow(owner,SW_HIDE);if(EditorModuleToolbar(owner))return 8;
    DestroyWindow(owner);std::puts("PASS: native Editor window discovery does not depend on language or installation path");return 0;
}
