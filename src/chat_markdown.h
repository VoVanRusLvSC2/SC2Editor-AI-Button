#pragma once
#include <string>
#include <vector>

// The renderer deliberately supports a small, safe Markdown subset. Unclosed
// markers remain visible so streaming and incomplete replies never lose text.
enum class MarkdownKind { Text, Bold, Code, Link, CopyCode };
struct MarkdownSpan { MarkdownKind kind; std::wstring text, target; };
inline bool IsWebLink(const std::wstring& url) {
    return url.rfind(L"https://",0)==0 || url.rfind(L"http://",0)==0;
}
inline void AddMarkdownSpan(std::vector<MarkdownSpan>& out,MarkdownKind kind,const std::wstring& text,const std::wstring& target={}) {
    if(!text.empty() || kind==MarkdownKind::CopyCode)out.push_back({kind,text,target});
}
inline std::vector<MarkdownSpan> ParseMarkdown(const std::wstring& source) {
    std::vector<MarkdownSpan> result;std::wstring plain;
    auto flush=[&](){AddMarkdownSpan(result,MarkdownKind::Text,plain);plain.clear();};
    size_t i=0;
    while(i<source.size()) {
        bool lineStart=i==0 || source[i-1]==L'\n';
        if(lineStart && source.compare(i,3,L"```")==0) {
            size_t start=source.find(L'\n',i+3);
            if(start!=std::wstring::npos){
                size_t end=start+1;
                while(end<source.size() && !(source.compare(end,3,L"```")==0 && (end==0 || source[end-1]==L'\n')))++end;
                if(end<source.size()){
                    flush();std::wstring code=source.substr(start+1,end-start-1);
                    AddMarkdownSpan(result,MarkdownKind::Code,code);AddMarkdownSpan(result,MarkdownKind::CopyCode,L"",code);
                    i=end+3;while(i<source.size() && source[i]!=L'\n')++i;
                    if(i<source.size()){plain+=L'\n';++i;}continue;
                }
            }
        }
        if(lineStart && source[i]==L'#') {
            size_t n=i;while(n<source.size() && source[n]==L'#' && n-i<6)++n;
            if(n<source.size() && source[n]==L' '){size_t end=source.find(L'\n',n+1);if(end==std::wstring::npos)end=source.size();flush();AddMarkdownSpan(result,MarkdownKind::Bold,source.substr(n+1,end-n-1));i=end;continue;}
        }
        if(lineStart && i+2<=source.size() && (source.compare(i,2,L"- ")==0 || source.compare(i,2,L"* ")==0)){
            plain+=L"• ";i+=2;continue;
        }
        if(source.compare(i,2,L"**")==0){size_t end=source.find(L"**",i+2);if(end!=std::wstring::npos && end>i+2){flush();AddMarkdownSpan(result,MarkdownKind::Bold,source.substr(i+2,end-i-2));i=end+2;continue;}}
        if(source[i]==L'`'){size_t end=source.find(L'`',i+1);if(end!=std::wstring::npos && end>i+1){flush();AddMarkdownSpan(result,MarkdownKind::Code,source.substr(i+1,end-i-1));i=end+1;continue;}}
        if(source[i]==L'['){size_t close=source.find(L']',i+1);if(close!=std::wstring::npos && close+1<source.size() && source[close+1]==L'('){size_t end=source.find(L')',close+2);if(end!=std::wstring::npos){auto target=source.substr(close+2,end-close-2);if(IsWebLink(target)){flush();AddMarkdownSpan(result,MarkdownKind::Link,source.substr(i+1,close-i-1),target);i=end+1;continue;}}}}
        if(source.compare(i,7,L"http://")==0 || source.compare(i,8,L"https://")==0){
            size_t end=i;while(end<source.size() && source[end]!=L' ' && source[end]!=L'\r' && source[end]!=L'\n' && source[end]!=L'\t' && source[end]!=L'<')++end;
            while(end>i && (source[end-1]==L'.' || source[end-1]==L',' || source[end-1]==L'!' || source[end-1]==L'?'))--end;
            if(end>i){auto target=source.substr(i,end-i);flush();AddMarkdownSpan(result,MarkdownKind::Link,target,target);i=end;continue;}
        }
        plain+=source[i++];
    }
    flush();return result;
}
