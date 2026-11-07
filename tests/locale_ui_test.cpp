#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
#include "../src/editor_locale.h"
#include "../src/locales_generated.h"
using Attach=BOOL (WINAPI*)(HWND,int,UINT);
using Detach=BOOL (WINAPI*)(HWND,UINT);
static HWND ai;
static BOOL CALLBACK Find(HWND h,LPARAM){wchar_t cls[128]{};GetClassNameW(h,cls,128);if(!lstrcmpW(cls,L"SC2AIButton.Window.v33"))ai=h;return TRUE;}
int wmain(){
    if(EditorLocaleFromVariables("\xEF\xBB\xBFlocaleiddata=zhTW\r\n")!=L"zhTW" ||
       EditorLocaleFromVariables("localeiddata=esMX\n")!=L"esMX" ||
       NormalizeEditorLocale(L"enGB")!=L"enUS" || NormalizeEditorLocale(L"ptPT")!=L"ptBR" ||
       NormalizeEditorLocale(L"zh-Hant-TW")!=L"zhTW" || !NormalizeEditorLocale(L"xyz").empty() ||
       EditorLocaleFromMenu(L"&Файл",L"enUS")!=L"ruRU" || EditorLocaleFromMenu(L"&File",L"itIT")!=L"itIT" ||
       EditorLocaleFromMenu(L"&文件",L"zhTW")!=L"zhTW") return 1;
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES};if(!InitCommonControlsEx(&common))return 2;
    HWND owner=CreateWindowExW(0,L"STATIC",L"test",WS_OVERLAPPEDWINDOW,0,0,1000,700,nullptr,nullptr,nullptr,nullptr);
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT,0,0,1000,30,owner,nullptr,nullptr,nullptr);if(!owner||!bar)return 3;
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);TBBUTTON buttons[2]{};
    for(int i=0;i<2;++i){buttons[i].iBitmap=I_IMAGENONE;buttons[i].idCommand=100+i;buttons[i].fsState=TBSTATE_ENABLED;}
    if(!SendMessageW(bar,TB_ADDBUTTONSW,2,reinterpret_cast<LPARAM>(buttons)))return 4;
    HMODULE dll=LoadLibraryW(L"SC2AIButton.dll");if(!dll)return 5;
    auto attach=reinterpret_cast<Attach>(GetProcAddress(dll,"SC2AI_Attach"));auto detach=reinterpret_cast<Detach>(GetProcAddress(dll,"SC2AI_Detach"));
    if(!attach||!detach||!attach(bar,1,500))return 6;
    const wchar_t* locales[]={L"enUS",L"ruRU",L"deDE",L"esES",L"esMX",L"frFR",L"itIT",L"koKR",L"plPL",L"ptBR",L"zhCN",L"zhTW"};
    for(auto locale:locales){
        if(!SetEnvironmentVariableW(L"SC2_AI_LOCALE",locale))return 7;
        ai=nullptr;SendMessageW(owner,WM_COMMAND,MAKEWPARAM(500,0),reinterpret_cast<LPARAM>(bar));EnumThreadWindows(GetCurrentThreadId(),Find,0);if(!ai)return 8;
        wchar_t actual[256]{};GetWindowTextW(GetDlgItem(ai,108),actual,256);
        if(wcscmp(actual,TranslateText(locale,L"Отправить",L"Send"))){std::fwprintf(stderr,L"Incorrect Send label for %ls\n",locale);return 9;}
        SetWindowPos(ai,nullptr,0,0,1000,700,SWP_NOZORDER|SWP_NOACTIVATE);
        RECT heading{},all{},editor{},remove{},list{};
        GetWindowRect(GetDlgItem(ai,106),&heading);GetWindowRect(GetDlgItem(ai,120),&all);
        GetWindowRect(GetDlgItem(ai,121),&editor);GetWindowRect(GetDlgItem(ai,122),&remove);GetWindowRect(GetDlgItem(ai,113),&list);
        if(heading.left!=list.left || heading.right>all.left || all.right>editor.left || editor.right>remove.left || remove.right>list.right ||
           heading.top!=all.top || all.top!=editor.top || editor.top!=remove.top){std::fwprintf(stderr,L"Header layout overlaps for %ls\n",locale);return 10;}
        SendMessageW(ai,WM_CLOSE,0,0);if(IsWindow(ai))return 11;
    }
    SetEnvironmentVariableW(L"SC2_AI_LOCALE",nullptr);if(!detach(bar,500))return 12;
    FreeLibrary(dll);DestroyWindow(owner);std::puts("PASS: locale detection, 12 translations and sidebar layout");return 0;
}
