#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <new>
#include <cwchar>

namespace {
constexpr wchar_t kProperty[] = L"SC2AIButton.context.v1";
constexpr wchar_t kWindowClass[] = L"SC2AIButton.Window.v1";
constexpr UINT kCommand = 60000;
HMODULE moduleHandle = nullptr;
HHOOK threadHook = nullptr;
UINT scanMessage = 0;
volatile LONG started = 0;
volatile LONG activeButtons = 0;

struct Context { HWND toolbar; HWND owner; HWND window; UINT command; };

LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_CREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(l);
        auto ctx = static_cast<Context*>(cs->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
        ctx->window = window;
        return 0;
    }
    if (msg == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (msg == WM_NCDESTROY) {
        auto ctx = reinterpret_cast<Context*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (ctx) ctx->window = nullptr;
    }
    return DefWindowProcW(window, msg, w, l);
}

bool OpenWindow(Context* ctx) {
    if (IsWindow(ctx->window)) {
        ShowWindow(ctx->window, SW_RESTORE);
        SetForegroundWindow(ctx->window);
        return true;
    }
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=WindowProc;
    wc.hInstance=moduleHandle; wc.lpszClassName=kWindowClass;
    wc.hCursor=LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if (!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, L"AI", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 600, ctx->owner, nullptr, moduleHandle, ctx);
    if (!window) return false;
    ShowWindow(window, SW_SHOWNORMAL); UpdateWindow(window); return true;
}

LRESULT CALLBACK OwnerProc(HWND owner, UINT message, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR ref) {
    auto ctx=reinterpret_cast<Context*>(ref);
    if (message==WM_COMMAND && LOWORD(w)==ctx->command && reinterpret_cast<HWND>(l)==ctx->toolbar) {
        OpenWindow(ctx); return 0;
    }
    if (message==WM_NCDESTROY) {
        RemoveWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
        RemovePropW(ctx->toolbar,kProperty);
        if (IsWindow(ctx->window)) DestroyWindow(ctx->window);
        InterlockedDecrement(&activeButtons);
        delete ctx;
    }
    return DefSubclassProc(owner,message,w,l);
}

bool IsOurThread(HWND window) {
    DWORD pid=0; DWORD thread=GetWindowThreadProcessId(window,&pid);
    return IsWindow(window) && pid==GetCurrentProcessId() && thread==GetCurrentThreadId();
}
}

// Called on the toolbar's GUI thread. The module must stay loaded until detach.
extern "C" __declspec(dllexport) BOOL WINAPI SC2AI_Attach(HWND toolbar, int index, UINT command) {
    wchar_t cls[64]{};
    HWND owner=GetParent(toolbar);
    if (!IsOurThread(toolbar) || !IsOurThread(owner) ||
        !GetClassNameW(toolbar,cls,64) || lstrcmpiW(cls,TOOLBARCLASSNAMEW) ||
        command==0 || command>0xffff || GetPropW(toolbar,kProperty)) return FALSE;
    LRESULT count=SendMessageW(toolbar,TB_BUTTONCOUNT,0,0);
    if (count<0 || index<0 || index>count || SendMessageW(toolbar,TB_COMMANDTOINDEX,command,0)!=-1) return FALSE;
    auto ctx=new(std::nothrow) Context{toolbar,owner,nullptr,command};
    if (!ctx) return FALSE;
    if (!SetPropW(toolbar,kProperty,ctx)) { delete ctx; return FALSE; }
    if (!SetWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx),reinterpret_cast<DWORD_PTR>(ctx))) {
        RemovePropW(toolbar,kProperty); delete ctx; return FALSE;
    }
    TBBUTTON button{}; button.iBitmap=I_IMAGENONE; button.idCommand=static_cast<int>(command);
    button.fsState=TBSTATE_ENABLED; button.fsStyle=BTNS_BUTTON|BTNS_AUTOSIZE|BTNS_SHOWTEXT;
    button.iString=reinterpret_cast<INT_PTR>(L"AI");
    if (!SendMessageW(toolbar,TB_INSERTBUTTONW,index,reinterpret_cast<LPARAM>(&button))) {
        RemoveWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
        RemovePropW(toolbar,kProperty); delete ctx; return FALSE;
    }
    SendMessageW(toolbar,TB_AUTOSIZE,0,0);
    InterlockedIncrement(&activeButtons);
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL WINAPI SC2AI_Detach(HWND toolbar, UINT command) {
    if (!IsOurThread(toolbar)) return FALSE;
    auto ctx=static_cast<Context*>(GetPropW(toolbar,kProperty));
    if (!ctx || ctx->command!=command) return FALSE;
    LRESULT index=SendMessageW(toolbar,TB_COMMANDTOINDEX,command,0);
    if (index<0 || !SendMessageW(toolbar,TB_DELETEBUTTON,index,0)) return FALSE;
    RemoveWindowSubclass(ctx->owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
    RemovePropW(toolbar,kProperty);
    if (IsWindow(ctx->window)) DestroyWindow(ctx->window);
    delete ctx;
    InterlockedDecrement(&activeButtons);
    SendMessageW(toolbar,TB_AUTOSIZE,0,0);
    return TRUE;
}

namespace {
// Only the observed module toolbar shape is accepted; ambiguous layouts are ignored.
bool FindInsertion(HWND toolbar, int& insertion) {
    int count=static_cast<int>(SendMessageW(toolbar,TB_BUTTONCOUNT,0,0));
    if (count<11 || count>150 || SendMessageW(toolbar,TB_COMMANDTOINDEX,kCommand,0)!=-1) return false;
    TBBUTTON sep{},last{},previous{};
    if (!SendMessageW(toolbar,TB_GETBUTTON,count-2,reinterpret_cast<LPARAM>(&sep)) ||
        !SendMessageW(toolbar,TB_GETBUTTON,count-1,reinterpret_cast<LPARAM>(&last)) ||
        !SendMessageW(toolbar,TB_GETBUTTON,count-3,reinterpret_cast<LPARAM>(&previous))) return false;
    if (!(sep.fsStyle&BTNS_SEP) || (last.fsStyle&BTNS_SEP) || (previous.fsStyle&BTNS_SEP)) return false;
    for (int i=0;i<count;++i) { TBBUTTON b{}; if (!SendMessageW(toolbar,TB_GETBUTTON,i,reinterpret_cast<LPARAM>(&b)) || b.idCommand!=i) return false; }
    insertion=count-2;
    return true;
}
BOOL CALLBACK InstallOnWindow(HWND owner, LPARAM) {
    if (!IsOurThread(owner) || !IsWindowVisible(owner)) return TRUE;
    wchar_t cls[64]{},title[1024]{}; GetClassNameW(owner,cls,64); GetWindowTextW(owner,title,1024);
    if (lstrcmpW(cls,L"#32770") || !wcsstr(title,L" - StarCraft II Editor")) return TRUE;
    HWND toolbar=GetDlgItem(owner,10);
    if (!toolbar || GetPropW(toolbar,kProperty)) return TRUE;
    wchar_t barClass[64]{}; GetClassNameW(toolbar,barClass,64);
    int index=0;
    if (!lstrcmpiW(barClass,TOOLBARCLASSNAMEW) && FindInsertion(toolbar,index)) SC2AI_Attach(toolbar,index,kCommand);
    return TRUE;
}
LRESULT CALLBACK GetMessageHook(int code, WPARAM w, LPARAM l) {
    auto msg=reinterpret_cast<MSG*>(l);
    if (code>=0 && msg && !msg->hwnd && msg->message==scanMessage)
        EnumThreadWindows(GetCurrentThreadId(),InstallOnWindow,0);
    return CallNextHookEx(threadHook,code,w,l);
}
BOOL CALLBACK FindEditorThread(HWND window, LPARAM data) {
    DWORD pid=0; DWORD tid=GetWindowThreadProcessId(window,&pid);
    if (pid!=GetCurrentProcessId() || !IsWindowVisible(window)) return TRUE;
    wchar_t cls[64]{},title[1024]{}; GetClassNameW(window,cls,64); GetWindowTextW(window,title,1024);
    if (!lstrcmpW(cls,L"#32770") && wcsstr(title,L" - StarCraft II Editor") && GetDlgItem(window,10)) {
        *reinterpret_cast<DWORD*>(data)=tid; return FALSE;
    }
    return TRUE;
}
DWORD WINAPI Worker(void*) {
    DWORD installedThread=0;
    for (;;) {
        DWORD tid=0; EnumWindows(FindEditorThread,reinterpret_cast<LPARAM>(&tid));
        if (tid && tid!=installedThread) {
            if (threadHook) UnhookWindowsHookEx(threadHook);
            threadHook=SetWindowsHookExW(WH_GETMESSAGE,GetMessageHook,moduleHandle,tid);
            installedThread=threadHook?tid:0;
        }
        if (installedThread) PostThreadMessageW(installedThread,scanMessage,0,0);
        Sleep(750);
    }
}
}

// The loader calls this once after loading the DLL into SC2Editor_x64.exe.
extern "C" __declspec(dllexport) DWORD WINAPI SC2AI_Start(void*) {
    wchar_t path[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr,path,MAX_PATH)) return ERROR_BAD_EXE_FORMAT;
    const wchar_t* name=wcsrchr(path,L'\\'); name=name?name+1:path;
    if (lstrcmpiW(name,L"SC2Editor_x64.exe")) return ERROR_BAD_EXE_FORMAT;
    if (InterlockedCompareExchange(&started,1,0)!=0) return ERROR_ALREADY_EXISTS;
    scanMessage=RegisterWindowMessageW(L"SC2AIButton.Scan.v1");
    if (!scanMessage) { InterlockedExchange(&started,0); return GetLastError(); }
    HANDLE thread=CreateThread(nullptr,0,Worker,nullptr,0,nullptr);
    if (!thread) { InterlockedExchange(&started,0); return GetLastError(); }
    CloseHandle(thread); return 0;
}
extern "C" __declspec(dllexport) DWORD WINAPI SC2AI_Status(void*) {
    return InterlockedCompareExchange(&activeButtons,0,0)>0 ? ERROR_SUCCESS : ERROR_IO_PENDING;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) { moduleHandle=instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
