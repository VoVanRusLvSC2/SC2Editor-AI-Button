#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <cstdio>
#include <cwchar>
#include <string>

namespace fs=std::filesystem;
static uintptr_t RemoteModule(DWORD pid, const wchar_t* name, bool fullPath) {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if (snapshot==INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W entry{}; entry.dwSize=sizeof(entry); uintptr_t result=0;
    if (Module32FirstW(snapshot,&entry)) do {
        if (!_wcsicmp(fullPath?entry.szExePath:entry.szModule,name)) { result=reinterpret_cast<uintptr_t>(entry.modBaseAddr);break; }
    } while (Module32NextW(snapshot,&entry));
    CloseHandle(snapshot); return result;
}
static bool Supported(const wchar_t* path) {
    if (_wcsicmp(fs::path(path).filename().c_str(),L"SC2Editor_x64.exe")) return false;
    DWORD ignored=0, size=GetFileVersionInfoSizeW(path,&ignored);
    if (!size || size>1024*1024) return false;
    auto memory=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);
    if (!memory) return false;
    VS_FIXEDFILEINFO* info=nullptr; UINT bytes=0;
    bool ok=GetFileVersionInfoW(path,0,size,memory) &&
        VerQueryValueW(memory,L"\\",reinterpret_cast<void**>(&info),&bytes) && bytes>=sizeof(*info) &&
        info->dwSignature==0xfeef04bd && info->dwFileVersionMS==MAKELONG(0,5) &&
        info->dwFileVersionLS==MAKELONG(98310,17);
    HeapFree(GetProcessHeap(),0,memory); return ok;
}
static bool RunRemote(HANDLE process, uintptr_t address, void* argument, DWORD& result) {
    HANDLE thread=CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(address),argument,0,nullptr);
    if (!thread) return false;
    DWORD wait=WaitForSingleObject(thread,30000);
    bool ok=wait==WAIT_OBJECT_0 && GetExitCodeThread(thread,&result);
    CloseHandle(thread);
    // The caller must not release remote memory if the thread timed out.
    if (wait==WAIT_TIMEOUT) SetLastError(ERROR_TIMEOUT);
    return ok;
}
struct Search { DWORD pid; bool ambiguous; };
static BOOL CALLBACK FindVisible(HWND window, LPARAM raw) {
    auto search=reinterpret_cast<Search*>(raw);
    if (!IsWindowVisible(window)) return TRUE;
    wchar_t title[1024]{},cls[64]{}; GetWindowTextW(window,title,1024); GetClassNameW(window,cls,64);
    if (lstrcmpW(cls,L"#32770") || !wcsstr(title,L" - StarCraft II Editor") || !GetDlgItem(window,10)) return TRUE;
    DWORD pid=0; GetWindowThreadProcessId(window,&pid);
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    wchar_t path[32768]{}; DWORD size=32768;
    bool ok=process && QueryFullProcessImageNameW(process,0,path,&size) && Supported(path);
    if (process) CloseHandle(process);
    if (!ok) return TRUE;
    if (search->pid && search->pid!=pid) search->ambiguous=true;
    search->pid=pid; return TRUE;
}
int wmain(int argc,wchar_t** argv) {
    DWORD pid=0;
    if (argc==3 && !lstrcmpW(argv[1],L"--pid")) {
        wchar_t* end=nullptr; unsigned long value=wcstoul(argv[2],&end,10);
        if (!end || *end || !value) { std::fwprintf(stderr,L"Invalid PID.\n");return 2; }
        pid=value;
    } else if (argc==1) {
        Search found{}; EnumWindows(FindVisible,reinterpret_cast<LPARAM>(&found));
        if (!found.pid || found.ambiguous) { std::fwprintf(stderr,L"Expected exactly one supported visible Editor. Use --pid <id> if necessary.\n");return 2; }
        pid=found.pid;
    } else { std::fwprintf(stderr,L"Usage: SC2AILoader.exe [--pid <editor-pid>]\n");return 2; }
    HANDLE process=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid);
    if (!process) { std::fwprintf(stderr,L"Cannot open Editor: %lu\n",GetLastError());return 3; }
    wchar_t image[32768]{}; DWORD size=32768;
    if (!QueryFullProcessImageNameW(process,0,image,&size) || !Supported(image)) {
        std::fwprintf(stderr,L"Unsupported process or Editor version. Expected x64 5.0.17.98310.\n");CloseHandle(process);return 4;
    }
    wchar_t self[32768]{}; if (!GetModuleFileNameW(nullptr,self,32768)) { CloseHandle(process);return 5; }
    std::wstring dll=(fs::path(self).parent_path()/L"SC2AIButton.dll").wstring();
    HMODULE local=LoadLibraryExW(dll.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if (!local) { std::fwprintf(stderr,L"Cannot load local DLL: %lu\n",GetLastError());CloseHandle(process);return 5; }
    auto start=GetProcAddress(local,"SC2AI_Start");
    auto status=GetProcAddress(local,"SC2AI_Status");
    if (!start || !status) { std::fwprintf(stderr,L"DLL export missing.\n");FreeLibrary(local);CloseHandle(process);return 5; }
    uintptr_t startOffset=reinterpret_cast<uintptr_t>(start)-reinterpret_cast<uintptr_t>(local);
    uintptr_t statusOffset=reinterpret_cast<uintptr_t>(status)-reinterpret_cast<uintptr_t>(local);
    uintptr_t remoteBase=RemoteModule(pid,dll.c_str(),true);
    if (!remoteBase) {
        auto load=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW"); HMODULE owner=nullptr;
        if (!load || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(load),&owner)) { FreeLibrary(local);CloseHandle(process);return 6; }
        wchar_t ownerPath[32768]{}; GetModuleFileNameW(owner,ownerPath,32768);
        uintptr_t remoteOwner=RemoteModule(pid,fs::path(ownerPath).filename().c_str(),false);
        SIZE_T bytes=(dll.size()+1)*sizeof(wchar_t),written=0;
        void* remote=VirtualAllocEx(process,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if (!remoteOwner || !remote || !WriteProcessMemory(process,remote,dll.c_str(),bytes,&written) || written!=bytes) {
            std::fwprintf(stderr,L"Cannot prepare DLL load: %lu\n",GetLastError()); if (remote) VirtualFreeEx(process,remote,0,MEM_RELEASE);FreeLibrary(local);CloseHandle(process);return 6;
        }
        DWORD result=0; uintptr_t address=remoteOwner+(reinterpret_cast<uintptr_t>(load)-reinterpret_cast<uintptr_t>(owner));
        if (!RunRemote(process,address,remote,result)) {
            DWORD error=GetLastError(); if (error!=ERROR_TIMEOUT) VirtualFreeEx(process,remote,0,MEM_RELEASE);
            std::fwprintf(stderr,L"DLL load failed or timed out: %lu\n",error);FreeLibrary(local);CloseHandle(process);return 7;
        }
        VirtualFreeEx(process,remote,0,MEM_RELEASE);
        remoteBase=RemoteModule(pid,dll.c_str(),true);
        if (!remoteBase) { std::fwprintf(stderr,L"DLL was not found in the Editor after loading.\n");FreeLibrary(local);CloseHandle(process);return 7; }
    }
    DWORD result=0;
    bool ok=RunRemote(process,remoteBase+startOffset,nullptr,result);
    if (!ok || (result!=0 && result!=ERROR_ALREADY_EXISTS)) {
        std::fwprintf(stderr,L"DLL initialization failed: %lu\n",ok?result:GetLastError());FreeLibrary(local);CloseHandle(process);return 8;
    }
    bool attached=false;
    for (int i=0;i<60;++i) {
        DWORD state=ERROR_IO_PENDING;
        if (!RunRemote(process,remoteBase+statusOffset,nullptr,state)) break;
        if (state==ERROR_SUCCESS) { attached=true;break; }
        Sleep(250);
    }
    FreeLibrary(local);CloseHandle(process);
    if (!attached) { std::fwprintf(stderr,L"DLL loaded, but no compatible toolbar was found within 15 seconds.\n");return 9; }
    std::wprintf(L"AI button attached to Editor PID %lu.\n",pid);
    return 0;
}
