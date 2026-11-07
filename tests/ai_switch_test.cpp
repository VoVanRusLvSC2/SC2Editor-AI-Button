#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
static DWORD pid; static HWND source,ai,aiBar,data;
static BOOL CALLBACK Find(HWND h,LPARAM){
 DWORD p=0;GetWindowThreadProcessId(h,&p);if(p!=pid)return TRUE;
 wchar_t cls[128]{},title[1024]{};GetClassNameW(h,cls,128);GetWindowTextW(h,title,1024);
 if(!lstrcmpW(cls,L"SC2AIButton.Window.v28")){ai=h;aiBar=GetDlgItem(h,100);}
 if(!lstrcmpW(cls,L"#32770")){
  if(!wcsncmp(title,L"Terrain - ",10))source=GetDlgItem(h,10);
  if(!wcsncmp(title,L"Data - ",7) && IsWindowVisible(h))data=h;
 }return TRUE;
}
static bool Click(HWND bar,int command){
 int index=static_cast<int>(SendMessageW(bar,TB_COMMANDTOINDEX,command,0));
 HANDLE p=OpenProcess(PROCESS_VM_OPERATION|PROCESS_VM_READ,FALSE,pid);if(!p || index<0)return false;
 void* mem=VirtualAllocEx(p,nullptr,sizeof(RECT),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 RECT r{};SIZE_T n=0;bool ok=mem && SendMessageW(bar,TB_GETITEMRECT,index,reinterpret_cast<LPARAM>(mem)) && ReadProcessMemory(p,mem,&r,sizeof(r),&n) && n==sizeof(r);
 if(mem)VirtualFreeEx(p,mem,0,MEM_RELEASE);CloseHandle(p);if(!ok)return false;
 LPARAM pt=MAKELPARAM((r.left+r.right)/2,(r.top+r.bottom)/2);
 PostMessageW(bar,WM_LBUTTONDOWN,MK_LBUTTON,pt);PostMessageW(bar,WM_LBUTTONUP,0,pt);return true;
}
int wmain(int argc,wchar_t**argv){
 if(argc!=3)return 1;pid=wcstoul(argv[2],nullptr,10);
 HDESK desk=OpenDesktopW(argv[1],0,FALSE,DESKTOP_ENUMERATE|DESKTOP_READOBJECTS);
 if(!desk||!SetThreadDesktop(desk))return 2;
 EnumDesktopWindows(desk,Find,0);if(!source||!Click(source,60000))return 3;
 for(int i=0;i<60&&!aiBar;++i){Sleep(100);EnumDesktopWindows(desk,Find,0);}
 if(!aiBar||!(SendMessageW(aiBar,TB_GETSTATE,60000,0)&TBSTATE_CHECKED))return 4;
 if(!Click(aiBar,60000))return 8;
 Sleep(200);
 if(!(SendMessageW(aiBar,TB_GETSTATE,60000,0)&TBSTATE_CHECKED)){std::puts("Click deselected active AI");return 9;}
 if(!Click(aiBar,30))return 5;
 for(int i=0;i<150&&!data;++i){Sleep(100);EnumDesktopWindows(desk,Find,0);}
 if(!data||!IsWindowVisible(data)){std::puts("Native Data window did not open");return 6;}
 if(GetWindow(ai,GW_OWNER)){std::puts("AI remains owned by another module");return 7;}
 for(int i=0;i<30 && (SendMessageW(aiBar,TB_GETSTATE,60000,0)&TBSTATE_CHECKED);++i)Sleep(100);
 if(SendMessageW(aiBar,TB_GETSTATE,60000,0)&TBSTATE_CHECKED){std::puts("Inactive AI remained selected");return 10;}
 SendMessageW(ai,WM_CLOSE,0,0);std::puts("PASS: click keeps AI selected; Data opens and inactive AI is deselected");return 0;
}

