#pragma once
#include "chat_markdown.h"
#include <cwctype>

// Produce a short navigation label from existing text; this does not summarize or call AI.
inline std::wstring OutlineSnippet(const std::wstring& source,bool preferHeading){
    std::wstring first,heading;bool code=false;size_t start=0;
    while(start<source.size()){
        size_t end=source.find(L'\n',start);if(end==std::wstring::npos)end=source.size();
        std::wstring line=source.substr(start,end-start);while(!line.empty() && iswspace(line.back()))line.pop_back();
        size_t i=0;while(i<line.size() && iswspace(line[i]))++i;line.erase(0,i);
        if(line.rfind(L"```",0)==0)code=!code;
        else if(!code && !line.empty()){
            std::wstring candidate;size_t hashes=0;while(hashes<line.size() && line[hashes]==L'#' && hashes<6)++hashes;
            if(hashes && hashes<line.size() && line[hashes]==L' ')candidate=line.substr(hashes+1);
            else if(line.rfind(L"**",0)==0){size_t close=line.find(L"**",2);if(close!=std::wstring::npos){auto tail=line.substr(close+2);if(tail.empty() || tail==L":")candidate=line.substr(2,close-2)+tail;}}
            if(first.empty())first=line;if(!candidate.empty() && heading.empty())heading=candidate;
            if(!preferHeading || !heading.empty())break;
        }
        start=end+1;
    }
    std::wstring chosen=preferHeading && !heading.empty()?heading:first,plain;
    for(const auto& span:ParseMarkdown(chosen))if(span.kind!=MarkdownKind::CopyCode)plain+=span.text;
    std::wstring result;bool space=false;
    for(wchar_t ch:plain){if(iswspace(ch)){space=!result.empty();continue;}if(space)result+=L' ';space=false;result+=ch;}
    if(result.size()>72){result.resize(69);if(!result.empty() && result.back()>=0xD800 && result.back()<=0xDBFF)result.pop_back();result+=L"…";}return result;
}
