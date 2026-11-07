#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>

// Контекст получается из заголовка и истории редактора. Он помогает найти карту, но не
// подтверждает привязку MCP к документу в памяти.
struct MapContext { std::wstring path, caption; bool dirty=false, inferred=false; };
// Приводим регистр и слеши к единому виду только для сравнения Windows-путей.
inline std::wstring NormalizeMapPath(std::wstring value) {
    for(auto& c:value)if(c==L'/')c=L'\\';
    if(!value.empty())CharLowerBuffW(value.data(),static_cast<DWORD>(value.size()));
    return value;
}
// Сокращённый путь сопоставляется по достаточно конкретным началу и концу. Совпадения только имени
// файла недостаточно.
inline bool MapCaptionMatches(const std::wstring& caption,const std::wstring& file){
    auto c=NormalizeMapPath(caption),p=NormalizeMapPath(file);auto dots=c.find(L"...");size_t dotsLen=3;
    if(dots==std::wstring::npos){dots=c.find(L'…');dotsLen=1;}
    if(dots==std::wstring::npos)return c==p;
    auto prefix=c.substr(0,dots),suffix=c.substr(dots+dotsLen);
    if(prefix.size()!=3 || prefix[1]!=L':' || prefix[2]!=L'\\' || suffix.empty() || suffix[0]!=L'\\' || suffix.size()<20 || std::count(suffix.begin(),suffix.end(),L'\\')<3)return false;
    return p.size()>prefix.size()+suffix.size() && p.compare(0,prefix.size(),prefix)==0 && p.compare(p.size()-suffix.size(),suffix.size(),suffix)==0;
}
// Принимаем только существующие файлы карт и модов с ожидаемым расширением.
inline bool ExistingMap(const std::wstring& path){
    auto normalized=NormalizeMapPath(path);
    if(normalized.size()<7 || (normalized.rfind(L".sc2map")!=normalized.size()-7 && normalized.rfind(L".sc2mod")!=normalized.size()-7))return false;
    DWORD attributes=GetFileAttributesW(path.c_str());return attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY);
}
// Читаем путь и звёздочку несохранённых изменений из заголовка. Если путь сокращён, принимаем
// только единственное подходящее совпадение из истории. Флаг inferred сообщает, что такой путь ещё
// нужно проверить через MCP.
inline MapContext ReadMapContext(HWND owner){
    wchar_t title[32768]{};GetWindowTextW(owner,title,32768);std::wstring text(title);MapContext result;
    auto left=text.find(L" - ["),right=text.rfind(L"] - ");
    if(left==std::wstring::npos || right==std::wstring::npos || right<=left+4)return result;
    result.caption=text.substr(left+4,right-left-4);
    if(!result.caption.empty() && result.caption.back()==L'*'){result.dirty=true;result.caption.pop_back();}
    if(result.caption.find(L"...")==std::wstring::npos && result.caption.find(L'…')==std::wstring::npos){if(ExistingMap(result.caption))result.path=result.caption;return result;}
    HKEY key=nullptr;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Blizzard Entertainment\\StarCraft II Editor\\Recent Files",0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return result;
    std::vector<std::wstring> matches;
    for(int i=0;;++i){wchar_t name[512]{},data[32768]{};DWORD nameSize=512,dataSize=sizeof(data),type=0;
        LONG rc=RegEnumValueW(key,i,name,&nameSize,nullptr,&type,reinterpret_cast<BYTE*>(data),&dataSize);
        if(rc==ERROR_NO_MORE_ITEMS)break;
        if(rc!=ERROR_SUCCESS || type!=REG_SZ)continue;
        data[32767]=0;std::wstring candidate(data);
        if(!MapCaptionMatches(result.caption,candidate) || !ExistingMap(candidate))continue;
        auto normalized=NormalizeMapPath(candidate);bool duplicate=false;
        for(const auto& old:matches)if(NormalizeMapPath(old)==normalized)duplicate=true;
        if(!duplicate)matches.push_back(candidate);
    }
    RegCloseKey(key);if(matches.size()==1){result.path=matches[0];result.inferred=true;}return result;
}
