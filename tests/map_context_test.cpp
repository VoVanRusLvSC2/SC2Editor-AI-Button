#define WIN32_LEAN_AND_MEAN
#include "../src/map_context.h"
#include <cstdio>
int wmain(int argc,wchar_t** argv){
    if(argc==2){auto window=reinterpret_cast<HWND>(wcstoull(argv[1],nullptr,10));auto map=ReadMapContext(window);std::wprintf(L"path=%ls dirty=%d inferred=%d\n",map.path.c_str(),map.dirty,map.inferred);return map.path.empty()?10:0;}
    const std::wstring full=L"Z:\\workspace\\session\\r0\\Map\\output\\Example.SC2Map";
    if(!MapCaptionMatches(L"Z:/.../r0/Map/output/Example.SC2Map",full))return 1;
    if(!MapCaptionMatches(L"Z:/…/r0/Map/output/Example.SC2Map",full))return 2;
    if(MapCaptionMatches(L"Z:/.../Example.SC2Map",full))return 3;
    if(MapCaptionMatches(L"D:/.../r0/Map/output/Example.SC2Map",full))return 4;
    if(MapCaptionMatches(L"Z:/.../r0/Map/output/Other.SC2Map",full))return 5;
    if(!MapCaptionMatches(L"C:/ПРОЕКТ/КАРТА.SC2MAP",L"c:\\проект\\карта.sc2map"))return 10;
    wchar_t temp[MAX_PATH]{};if(!GetTempPathW(MAX_PATH,temp))return 6;
    std::wstring file=std::wstring(temp)+L"SC2AI карта с пробелом "+std::to_wstring(GetCurrentProcessId())+L".SC2Map";
    HANDLE f=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY,nullptr);
    if(f==INVALID_HANDLE_VALUE)return 7;CloseHandle(f);
    std::wstring title=L"Terrain - ["+file+L"*] - StarCraft II Editor";
    HWND w=CreateWindowExW(0,L"STATIC",title.c_str(),WS_OVERLAPPED,0,0,20,20,nullptr,nullptr,nullptr,nullptr);
    auto map=ReadMapContext(w);if(map.path!=file || !map.dirty)return 8;
    std::wstring localized=L"Территория - ["+file+L"] - Редактор StarCraft II";SetWindowTextW(w,localized.c_str());
    auto other=ReadMapContext(w);DestroyWindow(w);DeleteFileW(file.c_str());
    if(other.path!=file || other.dirty)return 9;
    std::puts("PASS: exact map and abbreviated paths; reject weak or mismatched titles; detect unsaved map");return 0;
}
