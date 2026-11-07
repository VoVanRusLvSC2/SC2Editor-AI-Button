#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>

static DWORD targetPid;
static HWND editor, toolbar, aiWindow;
static const wchar_t* requestedTitle=nullptr;
static int legacyOpen=0;
static BOOL CALLBACK FindPanel(HWND h,LPARAM) {
    wchar_t cls[128]{}; GetClassNameW(h,cls,128);
    if (legacyOpen==3 && !lstrcmpW(cls,L"SC2AIButton.Window.v3")) aiWindow=h;
    return TRUE;
}
static BOOL CALLBACK Find(HWND h, LPARAM) {
    DWORD pid=0; GetWindowThreadProcessId(h,&pid);
    if (pid!=targetPid) return TRUE;
    wchar_t cls[128]{},title[1024]{}; GetClassNameW(h,cls,128); GetWindowTextW(h,title,1024);
    if (!legacyOpen && !lstrcmpW(cls,L"SC2AIButton.Window.v5")) aiWindow=h;
    if (legacyOpen==4 && !lstrcmpW(cls,L"SC2AIButton.Window.v4")) aiWindow=h;
    if (legacyOpen && legacyOpen<3 && !lstrcmpW(cls,legacyOpen==1?L"SC2AIButton.Window.v1":L"SC2AIButton.Window.v2")) aiWindow=h;
    if (!lstrcmpW(cls,L"#32770") && wcsstr(title,L" - StarCraft II Editor") &&
        (!requestedTitle || wcsstr(title,requestedTitle)==title)) {
        HWND child=GetDlgItem(h,10); wchar_t childClass[128]{};
        if (child && GetClassNameW(child,childClass,128) && !lstrcmpiW(childClass,TOOLBARCLASSNAMEW) &&
            SendMessageW(child,TB_COMMANDTOINDEX,60000,0)>=0) {
                editor=h; toolbar=child;
                if (legacyOpen==3) EnumChildWindows(h,FindPanel,0);
            }
    }
    return TRUE;
}
static bool Capture(HWND window,const wchar_t* file) {
    RECT r{}; if (!GetClientRect(window,&r)) return false;
    int width=r.right-r.left,height=r.bottom-r.top;
    if (width<=0 || height<=0 || width>8192 || height>8192) return false;
    HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,width,height); auto old=SelectObject(dc,bitmap);
    bool ok=PrintWindow(window,dc,PW_CLIENTONLY)!=FALSE;
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=height; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    DWORD imageSize=static_cast<DWORD>(width*height*4);
    void* pixels=HeapAlloc(GetProcessHeap(),0,imageSize);
    SelectObject(dc,old);
    if (!pixels || !GetDIBits(dc,bitmap,0,height,pixels,&info,DIB_RGB_COLORS)) ok=false;
    HANDLE output=CreateFileW(file,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (output==INVALID_HANDLE_VALUE) ok=false;
    if (ok) {
        BITMAPFILEHEADER head{}; head.bfType=0x4d42; head.bfOffBits=sizeof(head)+sizeof(BITMAPINFOHEADER); head.bfSize=head.bfOffBits+imageSize;
        DWORD written=0;
        ok=WriteFile(output,&head,sizeof(head),&written,nullptr) && written==sizeof(head) &&
           WriteFile(output,&info.bmiHeader,sizeof(info.bmiHeader),&written,nullptr) && written==sizeof(info.bmiHeader) &&
           WriteFile(output,pixels,imageSize,&written,nullptr) && written==imageSize;
    }
    if (output!=INVALID_HANDLE_VALUE) CloseHandle(output);
    if (pixels) HeapFree(GetProcessHeap(),0,pixels);
    DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);return ok;
}
int wmain(int argc,wchar_t**argv) {
    if (argc<4 || argc>6) return 1;
    targetPid=wcstoul(argv[2],nullptr,10);
    if (argc>=5) requestedTitle=argv[4];
    if (argc==6) legacyOpen=!lstrcmpW(argv[5],L"legacy-open")?1:(!lstrcmpW(argv[5],L"legacy-v2-open")?2:(!lstrcmpW(argv[5],L"legacy-v3-open")?3:(!lstrcmpW(argv[5],L"legacy-v4-open")?4:0)));
    HDESK desk=OpenDesktopW(argv[1],0,FALSE,DESKTOP_ENUMERATE|DESKTOP_READOBJECTS);
    if (!desk || !SetThreadDesktop(desk)) { std::printf("desktop error %lu\n",GetLastError());return 2; }
    EnumDesktopWindows(desk,Find,0);
    if (!toolbar) { std::puts("AI toolbar not found");return 3; }
    HANDLE process=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_READ,FALSE,targetPid);
    if (!process) return 4;
    void* remote=VirtualAllocEx(process,nullptr,sizeof(RECT),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    int index=static_cast<int>(SendMessageW(toolbar,TB_COMMANDTOINDEX,60000,0));
    RECT button{}; SIZE_T read=0;
    bool ok=remote && SendMessageW(toolbar,TB_GETITEMRECT,index,reinterpret_cast<LPARAM>(remote)) &&
        ReadProcessMemory(process,remote,&button,sizeof(button),&read) && read==sizeof(button);
    if (remote) VirtualFreeEx(process,remote,0,MEM_RELEASE);
    CloseHandle(process);
    if (!ok) return 5;
    wchar_t image[1024]{}; swprintf_s(image,L"%ls\\toolbar.bmp",argv[3]); Capture(toolbar,image);
    int x=(button.left+button.right)/2,y=(button.top+button.bottom)/2;
    DWORD_PTR result=0;
    if (!SendMessageTimeoutW(toolbar,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y),SMTO_ABORTIFHUNG,5000,&result) ||
        !SendMessageTimeoutW(toolbar,WM_LBUTTONUP,0,MAKELPARAM(x,y),SMTO_ABORTIFHUNG,5000,&result)) return 6;
    for (int i=0;i<40 && !aiWindow;++i) { Sleep(100);EnumDesktopWindows(desk,Find,0); }
    if (!IsWindow(aiWindow) || !IsWindowVisible(aiWindow) ||
        (legacyOpen==3 ? GetParent(aiWindow)!=editor : GetWindow(aiWindow,GW_OWNER)!=editor)) { std::puts("AI window not opened with the expected state");return 7; }
    if (legacyOpen) { std::puts("PASS: legacy AI window open for upgrade test");return 0; }
    RECT barRect{},panelRect{}; GetWindowRect(toolbar,&barRect);GetWindowRect(aiWindow,&panelRect);
    if (panelRect.top<barRect.bottom || panelRect.right<=panelRect.left || panelRect.bottom<=panelRect.top) { std::puts("AI panel overlaps toolbar or is empty");return 11; }
    POINT workspace{panelRect.left+100,panelRect.top+100};
    HWND under=WindowFromPoint(workspace);
    if (under && GetAncestor(under,GA_ROOT)!=aiWindow) { std::puts("AI panel is not visually above the workspace");return 12; }
    swprintf_s(image,L"%ls\\editor-with-ai.bmp",argv[3]); Capture(editor,image);
    swprintf_s(image,L"%ls\\ai-window.bmp",argv[3]); Capture(aiWindow,image);
    HWND first=aiWindow;
    SendMessageW(toolbar,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(toolbar,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    if (!IsWindow(first)) return 8;
    SendMessageW(first,WM_CLOSE,0,0);
    if (IsWindow(first)) return 9;
    aiWindow=nullptr;
    SendMessageW(toolbar,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(toolbar,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    for (int i=0;i<40 && !aiWindow;++i) { Sleep(100);EnumDesktopWindows(desk,Find,0); }
    if (!IsWindow(aiWindow)) return 10;
    SendMessageW(aiWindow,WM_CLOSE,0,0);
    std::printf("PASS: Editor toolbar opens and reopens AI panel below the visible toolbar\n");
    return 0;
}
