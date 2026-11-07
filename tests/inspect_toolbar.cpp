#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
#include <cstdlib>

static DWORD targetPid;
static HANDLE process;
static BOOL CALLBACK Child(HWND h, LPARAM) {
    wchar_t name[128]{}, text[256]{};
    GetClassNameW(h, name, 128);
    if (lstrcmpiW(name, TOOLBARCLASSNAMEW) != 0) return TRUE;
    RECT rect{}; GetWindowRect(h, &rect);
    HWND parent = GetParent(h);
    DWORD_PTR count = 0;
    if (!SendMessageTimeoutW(h, TB_BUTTONCOUNT, 0, 0, SMTO_ABORTIFHUNG, 1000, &count)) return TRUE;
    GetClassNameW(parent, text, 256);
    std::wprintf(L"TOOLBAR hwnd=%p id=%d parent=%p parentClass=%ls rect=%ld,%ld,%ld,%ld count=%llu visible=%d\n",
        h, GetDlgCtrlID(h), parent, text, rect.left, rect.top, rect.right, rect.bottom,
        static_cast<unsigned long long>(count), IsWindowVisible(h));
    void* remote = VirtualAllocEx(process, nullptr, sizeof(TBBUTTON) + sizeof(RECT), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) return TRUE;
    for (DWORD_PTR i=0; i<count && i<200; ++i) {
        DWORD_PTR result=0; SIZE_T read=0; TBBUTTON button{}; RECT area{};
        if (!SendMessageTimeoutW(h, TB_GETBUTTON, i, reinterpret_cast<LPARAM>(remote), SMTO_ABORTIFHUNG, 1000, &result) || !result ||
            !ReadProcessMemory(process, remote, &button, sizeof(button), &read) || read!=sizeof(button)) continue;
        if (SendMessageTimeoutW(h, TB_GETITEMRECT, i, reinterpret_cast<LPARAM>(remote), SMTO_ABORTIFHUNG, 1000, &result) && result)
            ReadProcessMemory(process, remote, &area, sizeof(area), &read);
        std::wprintf(L"  index=%llu id=%d style=%u state=%u image=%d rect=%ld,%ld,%ld,%ld\n",
            static_cast<unsigned long long>(i),button.idCommand,button.fsStyle,button.fsState,button.iBitmap,
            area.left,area.top,area.right,area.bottom);
    }
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    return TRUE;
}
static BOOL CALLBACK Top(HWND h, LPARAM) {
    DWORD pid=0; GetWindowThreadProcessId(h,&pid);
    if (pid!=targetPid) return TRUE;
    wchar_t name[256]{},cls[128]{}; GetWindowTextW(h,name,256); GetClassNameW(h,cls,128);
    RECT r{}; GetWindowRect(h,&r);
    std::wprintf(L"WINDOW hwnd=%p class=%ls title=%ls rect=%ld,%ld,%ld,%ld visible=%d\n",h,cls,name,r.left,r.top,r.right,r.bottom,IsWindowVisible(h));
    EnumChildWindows(h,Child,0);
    return TRUE;
}
int wmain(int argc,wchar_t**argv) {
    if (argc!=3) return 1;
    targetPid=wcstoul(argv[2],nullptr,10);
    HDESK desktop=OpenDesktopW(argv[1],0,FALSE,DESKTOP_ENUMERATE|DESKTOP_READOBJECTS);
    if (!desktop || !SetThreadDesktop(desktop)) { std::printf("desktop error %lu\n",GetLastError());return 2; }
    process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_READ,FALSE,targetPid);
    if (!process) { std::printf("process error %lu\n",GetLastError());return 3; }
    EnumDesktopWindows(desktop,Top,0);
    CloseHandle(process); CloseDesktop(desktop);return 0;
}
