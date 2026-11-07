#define WIN32_LEAN_AND_MEAN
#include "bridge_client.h"
#include <wincrypt.h>
#include <atomic>
#include <new>
#include <vector>
#include <string>

struct BridgeState {
    std::atomic<HWND> window{nullptr};
    HANDLE input=nullptr, output=nullptr, process=nullptr;
    bool russian=false;
    volatile LONG references=1;
};
static void Release(BridgeState* s){if(InterlockedDecrement(&s->references)==0)delete s;}
static std::string Utf8(const std::wstring& s){
    if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
    if(n<=0)return {};std::string out(static_cast<size_t>(n),'\0');WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr);return out;
}
static std::wstring Wide(const std::string& s,bool russian){
    if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(n<=0)return russian?L"[Некорректный текст]":L"[Invalid text]";std::wstring out(static_cast<size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n);return out;
}
static std::string Encode(const std::wstring& text){
    auto in=Utf8(text);if(in.empty())return {};DWORD size=0;
    if(!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(in.data()),static_cast<DWORD>(in.size()),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&size))return {};
    std::string out(size,'\0');if(!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(in.data()),static_cast<DWORD>(in.size()),CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,out.data(),&size))return {};
    out.resize(size);return out;
}
static std::wstring Decode(const std::string& in,bool russian){
    if(in.empty())return {};DWORD n=0;if(!CryptStringToBinaryA(in.c_str(),static_cast<DWORD>(in.size()),CRYPT_STRING_BASE64,nullptr,&n,nullptr,nullptr))return russian?L"[Некорректные данные]":L"[Invalid data]";
    std::string bytes(n,'\0');if(!CryptStringToBinaryA(in.c_str(),static_cast<DWORD>(in.size()),CRYPT_STRING_BASE64,reinterpret_cast<BYTE*>(bytes.data()),&n,nullptr,nullptr))return russian?L"[Некорректные данные]":L"[Invalid data]";
    bytes.resize(n);return Wide(bytes,russian);
}
static void Deliver(BridgeState* s,const std::string& line){
    HWND window=s->window.load();if(!window)return;
    auto e=new(std::nothrow) BridgeEvent{};if(!e)return;e->source=s;
    size_t start=0,end=line.find('\t');e->kind=line.substr(0,end);
    while(end!=std::string::npos){start=end+1;end=line.find('\t',start);e->fields.push_back(Decode(line.substr(start,end-start),s->russian));}
    if(!PostMessageW(window,WM_AI_BRIDGE,0,reinterpret_cast<LPARAM>(e)))delete e;
}
static DWORD WINAPI ReadOutput(void* raw){
    auto s=static_cast<BridgeState*>(raw);char buf[4096];DWORD count=0;std::string pending;
    while(ReadFile(s->output,buf,sizeof(buf),&count,nullptr) && count){
        pending.append(buf,count);size_t end;
        while((end=pending.find('\n'))!=std::string::npos){auto line=pending.substr(0,end);if(!line.empty()&&line.back()=='\r')line.pop_back();pending.erase(0,end+1);Deliver(s,line);}
        if(pending.size()>4*1024*1024){Deliver(s,s->russian?"error\t0J7RgtCy0LXRgiDRgdC70LjRiNC60L7QvCDQtNC70LjQvdC90YvQuQ==":"error\tUmVzcG9uc2UgaXMgdG9vIGxvbmc=");break;}
    }
    Deliver(s,"disconnected");CloseHandle(s->output);WaitForSingleObject(s->process,5000);
    if(WaitForSingleObject(s->process,0)==WAIT_TIMEOUT)TerminateProcess(s->process,1);
    CloseHandle(s->process);Release(s);return 0;
}
BridgeState* StartBridge(HWND window, HMODULE module, const std::wstring& workspace,bool claude,bool russian){
    wchar_t node[32768]{};DWORD got=SearchPathW(nullptr,L"node.exe",nullptr,32768,node,nullptr);
    if(!got||got>=32768){wchar_t programFiles[32768]{};DWORD n=GetEnvironmentVariableW(L"ProgramFiles",programFiles,32768);
        if(!n||n>=32768)return nullptr;std::wstring fallback=std::wstring(programFiles)+L"\\nodejs\\node.exe";
        if(GetFileAttributesW(fallback.c_str())==INVALID_FILE_ATTRIBUTES || fallback.size()>=32768)return nullptr;
        wcscpy_s(node,fallback.c_str());}

    wchar_t dll[32768]{};got=GetModuleFileNameW(module,dll,32768);if(!got||got>=32768)return nullptr;
    std::wstring script(dll);auto slash=script.find_last_of(L"\\/");if(slash==std::wstring::npos)return nullptr;script.resize(slash);script+=L"\\bridge\\main.mjs";
    if(GetFileAttributesW(script.c_str())==INVALID_FILE_ATTRIBUTES)return nullptr;
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE childIn=nullptr,parentIn=nullptr,parentOut=nullptr,childOut=nullptr;
    if(!CreatePipe(&childIn,&parentIn,&sa,0))return nullptr;
    if(!CreatePipe(&parentOut,&childOut,&sa,0)){CloseHandle(childIn);CloseHandle(parentIn);return nullptr;}
    SetHandleInformation(parentIn,HANDLE_FLAG_INHERIT,0);SetHandleInformation(parentOut,HANDLE_FLAG_INHERIT,0);
    HANDLE nullErr=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(nullErr==INVALID_HANDLE_VALUE){CloseHandle(childIn);CloseHandle(parentIn);CloseHandle(parentOut);CloseHandle(childOut);return nullptr;}
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);auto mem=HeapAlloc(GetProcessHeap(),0,bytes);
    auto attrs=static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(mem);HANDLE inherited[]={childIn,childOut,nullErr};
    bool initialized=attrs && InitializeProcThreadAttributeList(attrs,1,0,&bytes);
    bool attributes=initialized && UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr);
    STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    si.StartupInfo.wShowWindow=SW_HIDE;si.StartupInfo.hStdInput=childIn;si.StartupInfo.hStdOutput=childOut;si.StartupInfo.hStdError=nullErr;si.lpAttributeList=attrs;
    // Paths are locally derived and cannot contain a quote on Windows.
    auto quote=[](const std::wstring& arg){std::wstring out=L"\"";size_t slashes=0;for(wchar_t ch:arg){if(ch==L'\\'){++slashes;continue;}if(ch==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');slashes=0;out+=ch;}out.append(slashes*2,L'\\');return out+L"\"";};
    std::wstring command=quote(node)+L" "+quote(script)+L" "+quote(workspace)+L" "+quote(claude?L"claude":L"codex")+L" "+quote(russian?L"ru":L"en");
    PROCESS_INFORMATION pi{};bool launched=attributes && CreateProcessW(node,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&si.StartupInfo,&pi);
    if(initialized)DeleteProcThreadAttributeList(attrs);if(mem)HeapFree(GetProcessHeap(),0,mem);
    CloseHandle(childIn);CloseHandle(childOut);CloseHandle(nullErr);
    if(!launched){CloseHandle(parentIn);CloseHandle(parentOut);return nullptr;}
    CloseHandle(pi.hThread);auto s=new(std::nothrow) BridgeState{};if(!s){TerminateProcess(pi.hProcess,1);CloseHandle(pi.hProcess);CloseHandle(parentIn);CloseHandle(parentOut);return nullptr;}
    s->window.store(window);s->input=parentIn;s->output=parentOut;s->process=pi.hProcess;s->russian=russian;InterlockedIncrement(&s->references);
    HANDLE thread=CreateThread(nullptr,0,ReadOutput,s,0,nullptr);if(!thread){TerminateProcess(s->process,1);CloseHandle(s->process);CloseHandle(s->input);CloseHandle(s->output);Release(s);Release(s);return nullptr;}
    CloseHandle(thread);return s;
}
bool SendBridge(BridgeState* s,const char* command,const std::vector<std::wstring>& fields){
    if(!s||!s->input)return false;std::string line(command);for(const auto& field:fields){line+='\t';line+=Encode(field);}line+='\n';
    DWORD written=0;return WriteFile(s->input,line.data(),static_cast<DWORD>(line.size()),&written,nullptr) && written==line.size();
}
void StopBridge(BridgeState* s){if(!s)return;s->window.store(nullptr);if(s->input){CloseHandle(s->input);s->input=nullptr;}Release(s);}
