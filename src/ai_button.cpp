#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <new>
#include <cwchar>
#include <vector>

namespace {
constexpr wchar_t kLegacyProperty1[] = L"SC2AIButton.context.v1";
constexpr wchar_t kLegacyProperty2[] = L"SC2AIButton.context.v2";
constexpr wchar_t kProperty[] = L"SC2AIButton.context.v3";
constexpr wchar_t kLegacyWindowClass1[] = L"SC2AIButton.Window.v1";
constexpr wchar_t kLegacyWindowClass2[] = L"SC2AIButton.Window.v2";
constexpr wchar_t kWindowClass[] = L"SC2AIButton.Window.v3";
constexpr UINT kCommand = 60000;
HMODULE moduleHandle = nullptr;
HHOOK threadHook = nullptr;
UINT scanMessage = 0;
volatile LONG started = 0;
volatile LONG activeButtons = 0;

struct Context {
    HWND toolbar; HWND owner; HWND window; UINT command;
    int legacy; bool ownsImages; HIMAGELIST images;
    TBBUTTONINFOW original;
};

// Draw beveled steel-blue lettering at the toolbar's native image dimensions.
HBITMAP CreateAIIcon(int width,int height) {
    if (width<12 || height<12 || width>256 || height>256) return nullptr;
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr; HDC dc=CreateCompatibleDC(nullptr);
    if (!dc) return nullptr;
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if (!bitmap || !bits) { if (bitmap) DeleteObject(bitmap); DeleteDC(dc); return nullptr; }
    ZeroMemory(bits,static_cast<SIZE_T>(width)*height*4);
    auto oldBitmap=SelectObject(dc,bitmap);
    int fontHeight=height*72/100;
    HFONT font=CreateFontW(-fontHeight,0,0,0,FW_HEAVY,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Georgia");
    if (!font) { SelectObject(dc,oldBitmap); DeleteObject(bitmap); DeleteDC(dc); return nullptr; }
    auto oldFont=SelectObject(dc,font); SetBkMode(dc,TRANSPARENT);
    RECT base{0,0,width,height};
    SetTextColor(dc,RGB(255,255,255));
    DrawTextW(dc,L"AI",2,&base,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    GdiFlush();
    auto pixels=static_cast<DWORD*>(bits);
    std::vector<unsigned char> mask(static_cast<size_t>(width)*height);
    for (int i=0;i<width*height;++i) mask[i]=static_cast<unsigned char>(pixels[i]&255);
    auto ink=[&](int x,int y){return x>=0 && y>=0 && x<width && y<height && mask[y*width+x]>40;};
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        bool inside=ink(x,y); bool edge=false;
        if (!inside) for (int dy=-1;dy<=1 && !edge;++dy) for (int dx=-1;dx<=1;++dx) if (ink(x+dx,y+dy)) { edge=true;break; }
        if (!inside && !edge) { pixels[y*width+x]=0;continue; }
        int r=5,g=17,b=26;
        if (inside) {
            int shade=(y*70)/(height-1);
            r=28+shade/3;g=91+shade;b=131+shade;
            if (!ink(x-1,y-1)) { r=164;g=210;b=225; }
            else if (!ink(x+1,y+1)) { r=12;g=47;b=69; }
            else if ((x*3+y*5)%13==0) { r+=30;g+=25;b+=20; }
        }
        pixels[y*width+x]=0xff000000u|(static_cast<DWORD>(r)<<16)|(static_cast<DWORD>(g)<<8)|static_cast<DWORD>(b);
    }
    SelectObject(dc,oldFont); DeleteObject(font); SelectObject(dc,oldBitmap); DeleteDC(dc);
    return bitmap;
}

int AddAIIcon(HWND toolbar,Context* ctx) {
    auto images=reinterpret_cast<HIMAGELIST>(SendMessageW(toolbar,TB_GETIMAGELIST,0,0));
    int width=0,height=0;
    if (!images) {
        images=ImageList_Create(24,24,ILC_COLOR32|ILC_MASK,0,1);
        if (!images) return -1;
        SendMessageW(toolbar,TB_SETIMAGELIST,0,reinterpret_cast<LPARAM>(images));
        ctx->ownsImages=true;
    }
    ctx->images=images;
    if (!ImageList_GetIconSize(images,&width,&height)) return -1;
    HBITMAP icon=CreateAIIcon(width,height);
    if (!icon) return -1;
    int index=ImageList_Add(images,icon,nullptr); DeleteObject(icon); return index;
}

void ReleaseOwnedImages(Context* ctx) {
    if (!ctx->ownsImages || !ctx->images) return;
    if (IsWindow(ctx->toolbar) && reinterpret_cast<HIMAGELIST>(SendMessageW(ctx->toolbar,TB_GETIMAGELIST,0,0))==ctx->images)
        SendMessageW(ctx->toolbar,TB_SETIMAGELIST,0,0);
    ImageList_Destroy(ctx->images); ctx->images=nullptr;
}

LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_CREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(l);
        auto ctx = static_cast<Context*>(cs->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
        ctx->window = window;
        CreateWindowExW(0,L"BUTTON",L"\x00d7",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            0,0,32,26,window,reinterpret_cast<HMENU>(1),moduleHandle,nullptr);
        return 0;
    }
    if (msg == WM_SIZE) {
        HWND close=GetDlgItem(window,1);
        if (close) SetWindowPos(close,nullptr,LOWORD(l)>40?LOWORD(l)-38:0,4,32,26,SWP_NOZORDER|SWP_NOACTIVATE);
        return 0;
    }
    if (msg == WM_COMMAND && LOWORD(w)==1) { DestroyWindow(window); return 0; }
    if (msg == WM_PAINT) {
        PAINTSTRUCT paint{}; HDC dc=BeginPaint(window,&paint);
        RECT area{}; GetClientRect(window,&area);
        RECT heading{12,0,area.right-44,34}; SetBkMode(dc,TRANSPARENT);
        DrawTextW(dc,L"AI",2,&heading,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
        HPEN pen=CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DLIGHT));
        if (pen) { auto old=SelectObject(dc,pen); MoveToEx(dc,0,34,nullptr); LineTo(dc,area.right,34); SelectObject(dc,old); DeleteObject(pen); }
        EndPaint(window,&paint); return 0;
    }
    if (msg == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (msg == WM_NCDESTROY) {
        auto ctx = reinterpret_cast<Context*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (ctx) ctx->window = nullptr;
    }
    return DefWindowProcW(window, msg, w, l);
}

void LayoutWindow(Context* ctx) {
    if (!IsWindow(ctx->window) || !IsWindow(ctx->owner) || !IsWindow(ctx->toolbar)) return;
    RECT area{},bar{};
    if (!GetClientRect(ctx->owner,&area) || !GetWindowRect(ctx->toolbar,&bar)) return;
    MapWindowPoints(nullptr,ctx->owner,reinterpret_cast<POINT*>(&bar),2);
    int top=bar.bottom<0?0:bar.bottom;
    if (top>area.bottom) top=area.bottom;
    SetWindowPos(ctx->window,HWND_TOP,0,top,area.right,area.bottom-top,SWP_NOACTIVATE|SWP_SHOWWINDOW);
}

bool OpenWindow(Context* ctx) {
    if (IsWindow(ctx->window)) {
        LayoutWindow(ctx);
        return true;
    }
    WNDCLASSEXW wc{}; wc.cbSize=sizeof(wc); wc.lpfnWndProc=WindowProc;
    wc.hInstance=moduleHandle; wc.lpszClassName=kWindowClass;
    wc.hCursor=LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if (!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;
    HWND window=CreateWindowExW(0, kWindowClass, L"AI", WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
        0,0,0,0,ctx->owner,nullptr,moduleHandle,ctx);
    if (!window) return false;
    LayoutWindow(ctx); UpdateWindow(window); return true;
}

LRESULT CALLBACK OwnerProc(HWND owner, UINT message, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR ref) {
    auto ctx=reinterpret_cast<Context*>(ref);
    if (message==WM_COMMAND && LOWORD(w)==ctx->command && reinterpret_cast<HWND>(l)==ctx->toolbar) {
        OpenWindow(ctx); return 0;
    }
    if (message==WM_SIZE || message==WM_DPICHANGED) {
        HWND toolbar=ctx->toolbar;
        auto result=DefSubclassProc(owner,message,w,l);
        if (IsWindow(toolbar) && GetPropW(toolbar,kProperty)==ctx) LayoutWindow(ctx);
        return result;
    }
    if (message==WM_NCDESTROY) {
        RemoveWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
        RemovePropW(ctx->toolbar,kProperty);
        if (IsWindow(ctx->window)) DestroyWindow(ctx->window);
        ReleaseOwnedImages(ctx);
        InterlockedDecrement(&activeButtons);
        delete ctx;
    }
    return DefSubclassProc(owner,message,w,l);
}

bool IsOurThread(HWND window) {
    DWORD pid=0; DWORD thread=GetWindowThreadProcessId(window,&pid);
    return IsWindow(window) && pid==GetCurrentProcessId() && thread==GetCurrentThreadId();
}
struct LegacyClose { Context* ctx; bool wasOpen; };
BOOL CALLBACK CloseLegacyWindow(HWND window, LPARAM raw) {
    auto state=reinterpret_cast<LegacyClose*>(raw);
    wchar_t cls[128]{}; GetClassNameW(window,cls,128);
    if ((!lstrcmpW(cls,kLegacyWindowClass1) || !lstrcmpW(cls,kLegacyWindowClass2)) && GetWindow(window,GW_OWNER)==state->ctx->owner) {
        state->wasOpen=true;
        DestroyWindow(window);
    }
    return TRUE;
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
    LRESULT current=SendMessageW(toolbar,TB_COMMANDTOINDEX,command,0);
    int legacy=GetPropW(toolbar,kLegacyProperty2)?2:(GetPropW(toolbar,kLegacyProperty1)?1:0);
    if (count<0 || index<0 || index>count || (legacy ? current!=index : current!=-1)) return FALSE;
    auto ctx=new(std::nothrow) Context{toolbar,owner,nullptr,command,legacy,false,nullptr,{}};
    if (!ctx) return FALSE;
    if (legacy) {
        ctx->original.cbSize=sizeof(ctx->original);
        ctx->original.dwMask=TBIF_IMAGE|TBIF_STYLE|TBIF_SIZE;
        if (SendMessageW(toolbar,TB_GETBUTTONINFOW,command,reinterpret_cast<LPARAM>(&ctx->original))<0) { delete ctx; return FALSE; }
    }
    if (!SetPropW(toolbar,kProperty,ctx)) { delete ctx; return FALSE; }
    if (!SetWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx),reinterpret_cast<DWORD_PTR>(ctx))) {
        RemovePropW(toolbar,kProperty); delete ctx; return FALSE;
    }
    int imageIndex=AddAIIcon(toolbar,ctx);
    bool inserted=false;
    if (imageIndex>=0 && legacy) {
        RECT previous{};
        int width=LOWORD(SendMessageW(toolbar,TB_GETBUTTONSIZE,0,0));
        if (index>0 && SendMessageW(toolbar,TB_GETITEMRECT,index-1,reinterpret_cast<LPARAM>(&previous))) width=previous.right-previous.left;
        if (width<12 || width>256) width=32;
        TBBUTTONINFOW info{}; info.cbSize=sizeof(info);
        info.dwMask=TBIF_IMAGE|TBIF_STYLE|TBIF_SIZE|TBIF_TEXT;
        info.iImage=imageIndex; info.fsStyle=BTNS_BUTTON; info.cx=static_cast<WORD>(width);
        wchar_t empty[]=L""; info.pszText=empty;
        inserted=SendMessageW(toolbar,TB_SETBUTTONINFOW,command,reinterpret_cast<LPARAM>(&info))!=FALSE;
    } else if (imageIndex>=0) {
        TBBUTTON button{}; button.iBitmap=imageIndex; button.idCommand=static_cast<int>(command);
        button.fsState=TBSTATE_ENABLED; button.fsStyle=BTNS_BUTTON; button.iString=-1;
        inserted=SendMessageW(toolbar,TB_INSERTBUTTONW,index,reinterpret_cast<LPARAM>(&button))!=FALSE;
    }
    if (!inserted) {
        RemoveWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
        RemovePropW(toolbar,kProperty); ReleaseOwnedImages(ctx); delete ctx; return FALSE;
    }
    SendMessageW(toolbar,TB_AUTOSIZE,0,0);
    InterlockedIncrement(&activeButtons);
    if (legacy) {
        LegacyClose previous{ctx,false};
        EnumThreadWindows(GetCurrentThreadId(),CloseLegacyWindow,reinterpret_cast<LPARAM>(&previous));
        if (previous.wasOpen) OpenWindow(ctx);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL WINAPI SC2AI_Detach(HWND toolbar, UINT command) {
    if (!IsOurThread(toolbar)) return FALSE;
    auto ctx=static_cast<Context*>(GetPropW(toolbar,kProperty));
    if (!ctx || ctx->command!=command) return FALSE;
    LRESULT index=SendMessageW(toolbar,TB_COMMANDTOINDEX,command,0);
    if (index<0) return FALSE;
    if (ctx->legacy) {
        auto old=ctx->original;
        wchar_t text[]=L"AI"; wchar_t empty[]=L"";
        old.dwMask|=TBIF_TEXT; old.pszText=ctx->legacy==1?text:empty;
        if (!SendMessageW(toolbar,TB_SETBUTTONINFOW,command,reinterpret_cast<LPARAM>(&old))) return FALSE;
    } else if (!SendMessageW(toolbar,TB_DELETEBUTTON,index,0)) return FALSE;
    RemoveWindowSubclass(ctx->owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx));
    RemovePropW(toolbar,kProperty);
    if (IsWindow(ctx->window)) DestroyWindow(ctx->window);
    ReleaseOwnedImages(ctx);
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
    if (lstrcmpiW(barClass,TOOLBARCLASSNAMEW)) return TRUE;
    if (GetPropW(toolbar,kLegacyProperty1) || GetPropW(toolbar,kLegacyProperty2)) {
        auto oldIndex=SendMessageW(toolbar,TB_COMMANDTOINDEX,kCommand,0);
        if (oldIndex>=0) SC2AI_Attach(toolbar,static_cast<int>(oldIndex),kCommand);
    } else if (FindInsertion(toolbar,index)) SC2AI_Attach(toolbar,index,kCommand);
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
    scanMessage=RegisterWindowMessageW(L"SC2AIButton.Scan.v3");
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
