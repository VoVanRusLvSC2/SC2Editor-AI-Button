#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
static DWORD pid;
static HWND source, destination;
static BOOL CALLBACK Find(HWND window,LPARAM) {
    DWORD found=0; GetWindowThreadProcessId(window,&found); if(found!=pid) return TRUE;
    wchar_t title[1024]{}; GetWindowTextW(window,title,1024);
    HWND bar=GetDlgItem(window,10); if(!bar || !IsWindowVisible(window)) return TRUE;
    if (!wcsncmp(title,L"Terrain - ",10)) source=bar;
    if (!wcsncmp(title,L"Data - ",7) && SendMessageW(bar,TB_COMMANDTOINDEX,60000,0)>=0) destination=bar;
    return TRUE;
}
int wmain(int argc,wchar_t**argv) {
    if(argc!=3) return 1;
    pid=wcstoul(argv[2],nullptr,10);
    HDESK desk=OpenDesktopW(argv[1],0,FALSE,DESKTOP_ENUMERATE|DESKTOP_READOBJECTS);
    if(!desk || !SetThreadDesktop(desk)) return 2;
    EnumDesktopWindows(desk,Find,0); if(!source) return 3;
    HANDLE process=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_READ,FALSE,pid);if(!process)return 4;
    void* mem=VirtualAllocEx(process,nullptr,sizeof(RECT),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    RECT rect{};SIZE_T read=0; int index=static_cast<int>(SendMessageW(source,TB_COMMANDTOINDEX,30,0));
    bool ok=mem && index>=0 && SendMessageW(source,TB_GETITEMRECT,index,reinterpret_cast<LPARAM>(mem)) && ReadProcessMemory(process,mem,&rect,sizeof(rect),&read) && read==sizeof(rect);
    if(mem)VirtualFreeEx(process,mem,0,MEM_RELEASE);CloseHandle(process);if(!ok)return 5;
    LPARAM pt=MAKELPARAM((rect.left+rect.right)/2,(rect.top+rect.bottom)/2);
    PostMessageW(source,WM_LBUTTONDOWN,MK_LBUTTON,pt);PostMessageW(source,WM_LBUTTONUP,0,pt);
    for(int i=0;i<300 && !destination;++i){Sleep(100);EnumDesktopWindows(desk,Find,0);}
    if(!destination){std::puts("Data toolbar with AI did not appear");return 6;}
    std::puts("PASS: switching from Terrain opens Data and installs AI on its toolbar");return 0;
}
