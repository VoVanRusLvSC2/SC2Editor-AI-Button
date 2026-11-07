#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <new>
#include <cwchar>
#include <vector>

namespace {
constexpr wchar_t kLegacyProperty1[] = L"SC2AIButton.context.v1";
constexpr wchar_t kLegacyProperty2[] = L"SC2AIButton.context.v2";
constexpr wchar_t kLegacyProperty3[] = L"SC2AIButton.context.v3";
constexpr wchar_t kLegacyProperty4[] = L"SC2AIButton.context.v4";
constexpr wchar_t kLegacyProperty5[] = L"SC2AIButton.context.v5";
constexpr wchar_t kLegacyProperty6[] = L"SC2AIButton.context.v6";
constexpr wchar_t kProperty[] = L"SC2AIButton.context.v7";
constexpr wchar_t kLegacyWindowClass1[] = L"SC2AIButton.Window.v1";
constexpr wchar_t kLegacyWindowClass2[] = L"SC2AIButton.Window.v2";
constexpr wchar_t kLegacyWindowClass3[] = L"SC2AIButton.Window.v3";
constexpr wchar_t kLegacyWindowClass4[] = L"SC2AIButton.Window.v4";
constexpr wchar_t kLegacyWindowClass5[] = L"SC2AIButton.Window.v5";
constexpr wchar_t kLegacyWindowClass6[] = L"SC2AIButton.Window.v6";
constexpr wchar_t kWindowClass[] = L"SC2AIButton.Window.v7";
constexpr UINT kCommand = 60000;
HMODULE moduleHandle = nullptr;
HHOOK threadHook = nullptr;
UINT scanMessage = 0;
volatile LONG started = 0;
volatile LONG activeButtons = 0;
HWND currentAIWindow = nullptr;
bool selectedClaude = false;

struct Context {
    HWND toolbar; HWND owner; HWND window; UINT command;
    int legacy; bool ownsImages; HIMAGELIST images;
    TBBUTTONINFOW original;
    HWND aiToolbar, codex, claude, heading, status, description, history, input, send;
    HWND settings, model, config; bool claudeSelected;
    HIMAGELIST aiImages;
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
    std::vector<unsigned char> original(static_cast<size_t>(width)*height);
    int left=width,top=height,right=-1,bottom=-1;
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        auto value=static_cast<unsigned char>(pixels[y*width+x]&255); original[y*width+x]=value;
        if (value>40) { if(x<left)left=x; if(x>right)right=x; if(y<top)top=y; if(y>bottom)bottom=y; }
    }
    std::vector<unsigned char> mask(static_cast<size_t>(width)*height);
    if (right>=left && bottom>=top) {
        int sourceW=right-left+1,sourceH=bottom-top+1;
        int targetH=height-6,targetW=sourceW*targetH*85/(sourceH*100);
        if(targetW>width-4)targetW=width-4;
        if(targetW<1)targetW=1;
        int x0=(width-targetW)/2,y0=(height-targetH)/2;
        for(int y=0;y<targetH;++y)for(int x=0;x<targetW;++x)
            mask[(y0+y)*width+x0+x]=original[(top+y*sourceH/targetH)*width+left+x*sourceW/targetW];
    }
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

// Build the lettering from the editor's own Text-module icon. Its pixel colors,
// bevel and outline then follow the installed editor's artwork and theme.
HBITMAP CreateNativeAIIcon(HWND toolbar, HIMAGELIST images, int insertion, int width, int height) {
    if (insertion<9 || width<16 || height<16 || width>256 || height>256) return nullptr;
    for (int i=insertion-9;i<insertion;++i) {
        TBBUTTON b{};
        if (!SendMessageW(toolbar,TB_GETBUTTON,i,reinterpret_cast<LPARAM>(&b)) || (b.fsStyle&BTNS_SEP)) return nullptr;
    }
    TBBUTTON text{};
    if (!SendMessageW(toolbar,TB_GETBUTTON,insertion-6,reinterpret_cast<LPARAM>(&text)) ||
        text.iBitmap<0 || text.iBitmap>=ImageList_GetImageCount(images)) return nullptr;
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width; info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr); if(!dc)return nullptr;
    void* bits=nullptr; HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap || !bits){if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return nullptr;}
    auto old=SelectObject(dc,bitmap); auto pixels=static_cast<DWORD*>(bits);
    constexpr DWORD background=0x00f0f0f0;
    for(int i=0;i<width*height;++i)pixels[i]=background;
    bool drawn=ImageList_Draw(images,text.iBitmap,dc,0,0,ILD_TRANSPARENT)!=FALSE;
    GdiFlush();
    std::vector<DWORD> source(pixels,pixels+width*height);
    int left=width,right=-1,top=height,bottom=-1;
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)if((source[y*width+x]&0xffffff)!=background){
        if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;
    }
    if(!drawn || right<left || bottom<top || right-left+1>=width || bottom-top+1>=height){
        SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return nullptr;
    }
    for(int i=0;i<width*height;++i)pixels[i]=0;
    int sourceW=right-left+1,sourceH=bottom-top+1;
    int targetH=sourceH; if(targetH>height-4)targetH=height-4;
    int iWidth=width/4; if(iWidth<5)iWidth=5;
    int aWidth=sourceW*targetH/sourceH;
    if(aWidth>width-iWidth-5)aWidth=width-iWidth-5;
    if(aWidth<3){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return nullptr;}
    int x0=(width-aWidth-iWidth-1)/2,y0=(height-targetH)/2;
    for(int y=0;y<targetH;++y){
        int sy=top+y*sourceH/targetH;
        int runEnd=right;
        while(runEnd>=left && (source[sy*width+runEnd]&0xffffff)==background)--runEnd;
        int runStart=runEnd;
        while(runStart>left && (source[sy*width+runStart-1]&0xffffff)!=background)--runStart;
        for(int x=0;x<aWidth;++x){
            DWORD p=source[sy*width+left+x*sourceW/aWidth]&0xffffff;
            if(p!=background)pixels[(y0+y)*width+x0+x]=0xff000000|p;
        }
        // Copy each horizontal slice of the native A's right-hand stroke into
        // the I. This preserves the actual textured highlight and dark bevel,
        // including their changes from one row to the next.
        if(runEnd>=runStart){
            bool bar=y<2 || y>=targetH-2;
            int begin=bar?0:1, end=bar?iWidth:iWidth-1;
            int runWidth=runEnd-runStart+1, targetWidth=end-begin;
            for(int x=begin;x<end;++x){
                int sample=runStart+(x-begin)*runWidth/targetWidth;
                DWORD p=source[sy*width+sample]&0xffffff;
                pixels[(y0+y)*width+x0+aWidth+1+x]=0xff000000|p;
            }
        }
    }
    SelectObject(dc,old);DeleteDC(dc);return bitmap;
}

int AddAIIcon(HWND toolbar,Context* ctx,int insertion) {
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
    HBITMAP icon=CreateNativeAIIcon(toolbar,images,insertion,width,height);
    if (!icon) icon=CreateAIIcon(width,height);
    if (!icon) return -1;
    int index=ImageList_Add(images,icon,nullptr); DeleteObject(icon); return index;
}

void ReleaseOwnedImages(Context* ctx) {
    if (!ctx->ownsImages || !ctx->images) return;
    if (IsWindow(ctx->toolbar) && reinterpret_cast<HIMAGELIST>(SendMessageW(ctx->toolbar,TB_GETIMAGELIST,0,0))==ctx->images)
        SendMessageW(ctx->toolbar,TB_SETIMAGELIST,0,0);
    ImageList_Destroy(ctx->images); ctx->images=nullptr;
}

enum : int { kToolbar=100, kCodex=101, kClaude=102, kHeading=103,
             kStatus=104, kDescription=105, kHistory=106, kInput=107, kSend=108,
             kSettings=109, kModel=110 };

HWND Control(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int id) {
    HWND child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,
        parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),moduleHandle,nullptr);
    if(child) SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    return child;
}

void SelectProvider(Context* ctx,bool claude) {
    ctx->claudeSelected=claude;selectedClaude=claude;
    if(IsWindow(ctx->codex))SendMessageW(ctx->codex,BM_SETCHECK,claude?BST_UNCHECKED:BST_CHECKED,0);
    if(IsWindow(ctx->claude))SendMessageW(ctx->claude,BM_SETCHECK,claude?BST_CHECKED:BST_UNCHECKED,0);
    SetWindowTextW(ctx->heading,claude?L"Claude":L"Codex");
    SetWindowTextW(ctx->status,L"Не подключён");
    SetWindowTextW(ctx->description,claude?
        L"Здесь появится переписка с Claude после подключения.":
        L"Здесь появится переписка с Codex после подключения.");
}
LRESULT CALLBACK SettingsProc(HWND window,UINT msg,WPARAM w,LPARAM l){
    auto ctx=reinterpret_cast<Context*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_CREATE){
        auto cs=reinterpret_cast<CREATESTRUCTW*>(l);ctx=static_cast<Context*>(cs->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ctx));ctx->settings=window;
        HWND label=Control(window,L"STATIC",L"Выберите помощника:",SS_LEFT,111);
        ctx->codex=Control(window,L"BUTTON",L"Codex",BS_AUTORADIOBUTTON|WS_GROUP,kCodex);
        ctx->claude=Control(window,L"BUTTON",L"Claude",BS_AUTORADIOBUTTON,kClaude);
        HWND note=Control(window,L"STATIC",L"Подключение помощников пока разрабатывается.",SS_LEFT,112);
        if(!label||!ctx->codex||!ctx->claude||!note)return -1;
        MoveWindow(label,20,20,330,24,TRUE);MoveWindow(ctx->codex,20,54,150,26,TRUE);
        MoveWindow(ctx->claude,20,86,150,26,TRUE);MoveWindow(note,20,130,340,35,TRUE);
        SelectProvider(ctx,ctx->claudeSelected);return 0;
    }
    if(msg==WM_COMMAND && ctx && (LOWORD(w)==kCodex || LOWORD(w)==kClaude)){
        SelectProvider(ctx,LOWORD(w)==kClaude);return 0;
    }
    if(msg==WM_CLOSE){DestroyWindow(window);return 0;}
    if(msg==WM_NCDESTROY && ctx){ctx->settings=nullptr;ctx->codex=nullptr;ctx->claude=nullptr;}
    return DefWindowProcW(window,msg,w,l);
}
void OpenSettings(Context* ctx,HWND owner){
    if(IsWindow(ctx->settings)){SetForegroundWindow(ctx->settings);return;}
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=SettingsProc;wc.hInstance=moduleHandle;
    wc.lpszClassName=L"SC2AIButton.Settings.v7";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;
    RECT r{};GetWindowRect(owner,&r);
    CreateWindowExW(WS_EX_DLGMODALFRAME,wc.lpszClassName,L"Настройки AI",WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        r.left+40,r.top+40,400,220,owner,nullptr,moduleHandle,ctx);
}

bool CopyToolbar(Context* ctx,HWND window) {
    HWND bar=CreateWindowExW(0,TOOLBARCLASSNAMEW,nullptr,WS_CHILD|WS_VISIBLE|TBSTYLE_FLAT|CCS_TOP|CCS_NODIVIDER,
        0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolbar)),moduleHandle,nullptr);
    if(!bar)return false;
    ctx->aiToolbar=bar;
    SendMessageW(bar,TB_BUTTONSTRUCTSIZE,sizeof(TBBUTTON),0);
    HIMAGELIST source=reinterpret_cast<HIMAGELIST>(SendMessageW(ctx->toolbar,TB_GETIMAGELIST,0,0));
    if(source){ctx->aiImages=ImageList_Duplicate(source);if(!ctx->aiImages)return false;
        SendMessageW(bar,TB_SETIMAGELIST,0,reinterpret_cast<LPARAM>(ctx->aiImages));}
    int count=static_cast<int>(SendMessageW(ctx->toolbar,TB_BUTTONCOUNT,0,0));
    int ai=static_cast<int>(SendMessageW(ctx->toolbar,TB_COMMANDTOINDEX,ctx->command,0));
    if(count<1 || count>150 || ai<0)return false;
    RECT nativeButton{};
    int buttonWidth=40,buttonHeight=40;
    if(SendMessageW(ctx->toolbar,TB_GETITEMRECT,ai,reinterpret_cast<LPARAM>(&nativeButton))){
        int w=nativeButton.right-nativeButton.left,h=nativeButton.bottom-nativeButton.top;
        if(w>=20 && w<=80)buttonWidth=w;if(h>=20 && h<=80)buttonHeight=h;
    }
    SendMessageW(bar,TB_SETBUTTONSIZE,0,MAKELPARAM(buttonWidth,buttonHeight));
    for(int i=0;i<count;++i){
        TBBUTTON b{};if(!SendMessageW(ctx->toolbar,TB_GETBUTTON,i,reinterpret_cast<LPARAM>(&b)))return false;
        // Only module navigation is meaningful in the AI workspace.
        if(!(b.fsStyle&BTNS_SEP)){
            b.iString=-1;b.dwData=0;b.fsStyle&=static_cast<BYTE>(~(BTNS_AUTOSIZE|BTNS_SHOWTEXT));b.fsState&=static_cast<BYTE>(~(TBSTATE_CHECKED|TBSTATE_PRESSED));
            if(i<ai-9 || i>ai) b.fsState&=static_cast<BYTE>(~TBSTATE_ENABLED);
            if(i==ai){b.fsStyle=BTNS_CHECK;b.fsState|=TBSTATE_ENABLED|TBSTATE_CHECKED;}
        }
        if(!SendMessageW(bar,TB_ADDBUTTONSW,1,reinterpret_cast<LPARAM>(&b)))return false;
    }
    SendMessageW(bar,TB_AUTOSIZE,0,0);return true;
}
void LayoutControls(Context* ctx,HWND window) {
    RECT r{};GetClientRect(window,&r);int width=r.right,height=r.bottom;
    SendMessageW(ctx->aiToolbar,TB_AUTOSIZE,0,0);
    RECT bar{};GetWindowRect(ctx->aiToolbar,&bar);int top=bar.bottom-bar.top+16;
    int side=width<700?170:260, mainX=22, mainW=width-side-48;
    if(mainW<160)mainW=160;
    int sidebarX=width-side+12;
    MoveWindow(ctx->heading,mainX,top,150,24,TRUE);
    MoveWindow(ctx->status,mainX+155,top,120,24,TRUE);
    int modelX=mainX+290;
    int modelWidth=mainW-430;if(modelWidth>200)modelWidth=200;if(modelWidth<90)modelWidth=90;
    MoveWindow(ctx->model,modelX,top-4,modelWidth,200,TRUE);
    MoveWindow(ctx->config,mainX+mainW-115,top-4,115,30,TRUE);
    int centerY=top+(height-top)/2-35;if(centerY<top+60)centerY=top+60;
    MoveWindow(ctx->description,mainX,centerY,mainW,60,TRUE);
    MoveWindow(ctx->history,sidebarX,top,side-24,80,TRUE);
    int inputY=height-80;if(inputY<top+140)inputY=top+140;
    MoveWindow(ctx->input,mainX,inputY,mainW>102?mainW-98:mainW,52,TRUE);
    MoveWindow(ctx->send,mainX+(mainW>102?mainW-90:0),inputY,90,30,TRUE);
}
LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM w, LPARAM l) {
    auto ctx=reinterpret_cast<Context*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_CREATE){
        auto cs=reinterpret_cast<CREATESTRUCTW*>(l);ctx=static_cast<Context*>(cs->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(ctx));ctx->window=window;
        if(!CopyToolbar(ctx,window))return -1;
        ctx->heading=Control(window,L"STATIC",L"Codex",SS_LEFT,kHeading);
        ctx->status=Control(window,L"STATIC",L"",SS_LEFT,kStatus);
        ctx->description=Control(window,L"STATIC",L"",SS_CENTER,kDescription);
        ctx->history=Control(window,L"STATIC",L"ЧАТЫ\r\n\r\nСписок появится после подключения",SS_LEFT,kHistory);
        ctx->model=Control(window,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_DISABLED,kModel);
        ctx->config=Control(window,L"BUTTON",L"Настройки",BS_PUSHBUTTON,kSettings);
        if(ctx->model){SendMessageW(ctx->model,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Выбор модели"));SendMessageW(ctx->model,CB_SETCURSEL,0,0);}
        ctx->input=Control(window,L"EDIT",L"Сообщение будет доступно после подключения",WS_BORDER|ES_MULTILINE|ES_READONLY,kInput);
        ctx->send=Control(window,L"BUTTON",L"Отправить",BS_PUSHBUTTON|WS_DISABLED,kSend);
        if(!ctx->heading||!ctx->status||!ctx->description||!ctx->history||!ctx->input||!ctx->send||!ctx->model||!ctx->config)return -1;
        SelectProvider(ctx,selectedClaude);return 0;
    }
    if(msg==WM_SIZE && ctx){LayoutControls(ctx,window);return 0;}
    if(msg==WM_GETMINMAXINFO){auto mm=reinterpret_cast<MINMAXINFO*>(l);mm->ptMinTrackSize.x=900;mm->ptMinTrackSize.y=420;return 0;}
    if(msg==WM_PAINT){
        PAINTSTRUCT ps{};HDC dc=BeginPaint(window,&ps);RECT r{};GetClientRect(window,&r);
        int side=r.right<700?170:260;int divider=r.right-side-3;
        HPEN pen=CreatePen(PS_SOLID,1,GetSysColor(COLOR_3DLIGHT));
        if(pen){auto old=SelectObject(dc,pen);MoveToEx(dc,divider,45,nullptr);LineTo(dc,divider,r.bottom);SelectObject(dc,old);DeleteObject(pen);}
        EndPaint(window,&ps);return 0;
    }
    if(msg==WM_CTLCOLORSTATIC){SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT);return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));}
    if(msg==WM_COMMAND && ctx){
        UINT id=LOWORD(w);HWND source=reinterpret_cast<HWND>(l);
        if(id==kSettings && source==ctx->config){OpenSettings(ctx,window);return 0;}
        if(source==ctx->aiToolbar){
            if(id==ctx->command)return 0;
            // Bring the source editor forward before invoking its own module command.
            if(IsIconic(ctx->owner))ShowWindow(ctx->owner,SW_RESTORE);
            SetForegroundWindow(ctx->owner);
            SendMessageW(ctx->owner,WM_COMMAND,MAKEWPARAM(id,0),reinterpret_cast<LPARAM>(ctx->toolbar));
            return 0;
        }
        if(!source){
            if(IsIconic(ctx->owner))ShowWindow(ctx->owner,SW_RESTORE);
            SetForegroundWindow(ctx->owner);
            SendMessageW(ctx->owner,WM_COMMAND,w,0);return 0;
        }
    }
    if(msg==WM_CLOSE){DestroyWindow(window);return 0;}
    if(msg==WM_NCDESTROY && ctx){
        if(currentAIWindow==window)currentAIWindow=nullptr;
        ctx->window=nullptr;ctx->aiToolbar=nullptr;
        if(ctx->aiImages){ImageList_Destroy(ctx->aiImages);ctx->aiImages=nullptr;}
    }
    return DefWindowProcW(window,msg,w,l);
}

// Copy the source module's normal menu; commands are handled by that module.
HMENU CopyMenuTree(HMENU source,bool root){
    HMENU copy=root?CreateMenu():CreatePopupMenu();if(!copy)return nullptr;
    int count=GetMenuItemCount(source);
    for(int i=0;i<count;++i){
        wchar_t label[1024]{};MENUITEMINFOW item{};item.cbSize=sizeof(item);
        item.fMask=MIIM_FTYPE|MIIM_STATE|MIIM_ID|MIIM_SUBMENU|MIIM_STRING;
        item.dwTypeData=label;item.cch=1023;
        if(!GetMenuItemInfoW(source,static_cast<UINT>(i),TRUE,&item)){DestroyMenu(copy);return nullptr;}
        if(item.fType&MFT_OWNERDRAW){item.fType&=~MFT_OWNERDRAW;item.fState|=MFS_DISABLED;}
        if(item.hSubMenu){item.hSubMenu=CopyMenuTree(item.hSubMenu,false);if(!item.hSubMenu){DestroyMenu(copy);return nullptr;}}
        if(!InsertMenuItemW(copy,static_cast<UINT>(i),TRUE,&item)){
            if(item.hSubMenu)DestroyMenu(item.hSubMenu);DestroyMenu(copy);return nullptr;
        }
    }
    return copy;
}

bool OpenWindow(Context* ctx) {
    if(IsWindow(currentAIWindow)){
        if(IsIconic(currentAIWindow))ShowWindow(currentAIWindow,SW_RESTORE);
        SetForegroundWindow(currentAIWindow);return true;
    }
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.lpfnWndProc=WindowProc;
    wc.hInstance=moduleHandle;wc.lpszClassName=kWindowClass;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    wc.hIcon=reinterpret_cast<HICON>(SendMessageW(ctx->owner,WM_GETICON,ICON_BIG,0));
    if(!RegisterClassExW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    wchar_t ownerTitle[1024]{},title[1100]{};GetWindowTextW(ctx->owner,ownerTitle,1024);
    const wchar_t* suffix=wcsstr(ownerTitle,L" - ");
    swprintf_s(title,L"AI%ls",suffix?suffix:L" - StarCraft II Editor");
    RECT r{};GetWindowRect(ctx->owner,&r);
    HWND window=CreateWindowExW(WS_EX_APPWINDOW,kWindowClass,title,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,moduleHandle,ctx);
    if(!window)return false;
    HMENU sourceMenu=GetMenu(ctx->owner);
    if(sourceMenu){HMENU menu=CopyMenuTree(sourceMenu,true);if(menu && !SetMenu(window,menu))DestroyMenu(menu);}
    currentAIWindow=window;ShowWindow(window,IsZoomed(ctx->owner)?SW_SHOWMAXIMIZED:SW_SHOW);UpdateWindow(window);return true;
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
    if ((!lstrcmpW(cls,kLegacyWindowClass1) || !lstrcmpW(cls,kLegacyWindowClass2) || !lstrcmpW(cls,kLegacyWindowClass4) || !lstrcmpW(cls,kLegacyWindowClass5) || !lstrcmpW(cls,kLegacyWindowClass6)) && GetWindow(window,GW_OWNER)==state->ctx->owner) {
        state->wasOpen=true;
        DestroyWindow(window);
    }
    return TRUE;
}
BOOL CALLBACK CloseLegacyPanel(HWND window, LPARAM raw) {
    auto state=reinterpret_cast<LegacyClose*>(raw);
    wchar_t cls[128]{}; GetClassNameW(window,cls,128);
    if (!lstrcmpW(cls,kLegacyWindowClass3)) { state->wasOpen=true; DestroyWindow(window); }
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
    int legacy=GetPropW(toolbar,kLegacyProperty6)?6:(GetPropW(toolbar,kLegacyProperty5)?5:(GetPropW(toolbar,kLegacyProperty4)?4:(GetPropW(toolbar,kLegacyProperty3)?3:(GetPropW(toolbar,kLegacyProperty2)?2:(GetPropW(toolbar,kLegacyProperty1)?1:0)))));
    if (count<0 || index<0 || index>count || (legacy ? current!=index : current!=-1)) return FALSE;
    auto ctx=new(std::nothrow) Context{};
    if (!ctx) return FALSE;
    ctx->toolbar=toolbar;ctx->owner=owner;ctx->command=command;ctx->legacy=legacy;
    if (legacy) {
        ctx->original.cbSize=sizeof(ctx->original);
        ctx->original.dwMask=TBIF_IMAGE|TBIF_STYLE|TBIF_SIZE;
        if (SendMessageW(toolbar,TB_GETBUTTONINFOW,command,reinterpret_cast<LPARAM>(&ctx->original))<0) { delete ctx; return FALSE; }
    }
    if (!SetPropW(toolbar,kProperty,ctx)) { delete ctx; return FALSE; }
    if (!SetWindowSubclass(owner,OwnerProc,reinterpret_cast<UINT_PTR>(ctx),reinterpret_cast<DWORD_PTR>(ctx))) {
        RemovePropW(toolbar,kProperty); delete ctx; return FALSE;
    }
    int imageIndex=AddAIIcon(toolbar,ctx,index);
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
        EnumChildWindows(owner,CloseLegacyPanel,reinterpret_cast<LPARAM>(&previous));
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
    if (GetPropW(toolbar,kLegacyProperty1) || GetPropW(toolbar,kLegacyProperty2) || GetPropW(toolbar,kLegacyProperty3) || GetPropW(toolbar,kLegacyProperty4) || GetPropW(toolbar,kLegacyProperty5) || GetPropW(toolbar,kLegacyProperty6)) {
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
    scanMessage=RegisterWindowMessageW(L"SC2AIButton.Scan.v7");
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
