#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <cstdio>
#include <cwchar>
#include <cwctype>
static DWORD pid;static HWND editor,bar,ai;
static BOOL CALLBACK Find(HWND h,LPARAM){DWORD p=0;GetWindowThreadProcessId(h,&p);if(p!=pid)return TRUE;
 wchar_t cls[128]{},title[1024]{};GetClassNameW(h,cls,128);GetWindowTextW(h,title,1024);
 if(!lstrcmpW(cls,L"SC2AIButton.Window.v28"))ai=h;
 if(!lstrcmpW(cls,L"#32770")&&!wcsncmp(title,L"Terrain - ",10)){HWND candidate=GetDlgItem(h,10);if(candidate&&SendMessageW(candidate,TB_COMMANDTOINDEX,60000,0)>=0){editor=h;bar=candidate;}}
 return TRUE;}
int wmain(int argc,wchar_t**argv){if(argc!=3 && argc!=4)return 1;pid=wcstoul(argv[2],nullptr,10);
 HDESK desk=OpenDesktopW(argv[1],0,FALSE,DESKTOP_ENUMERATE|DESKTOP_READOBJECTS);if(!desk||!SetThreadDesktop(desk))return 2;
 EnumDesktopWindows(desk,Find,0);if(!editor||!bar)return 3;
 PostMessageW(editor,WM_COMMAND,MAKEWPARAM(60000,0),reinterpret_cast<LPARAM>(bar));
 for(int i=0;i<150&&!ai;++i){Sleep(100);EnumDesktopWindows(desk,Find,0);}if(!ai)return 4;
 HWND chats=GetDlgItem(ai,113),models=GetDlgItem(ai,110),send=GetDlgItem(ai,108),transcript=GetDlgItem(ai,105);
 if(!chats||!models||!send||!transcript)return 5;
 for(int i=0;i<300 && (!IsWindowEnabled(send)||SendMessageW(chats,LB_GETCOUNT,0,0)<3||SendMessageW(models,CB_GETCOUNT,0,0)<1);++i)Sleep(100);
 auto chatCount=SendMessageW(chats,LB_GETCOUNT,0,0),modelCount=SendMessageW(models,CB_GETCOUNT,0,0);
 if(!IsWindowEnabled(send)||chatCount<3||modelCount<1){std::printf("Bridge did not become ready (chats=%lld models=%lld enabled=%d)\n",static_cast<long long>(chatCount),static_cast<long long>(modelCount),IsWindowEnabled(send));return 6;}
 SendMessageW(transcript,WM_SETTEXT,0,reinterpret_cast<LPARAM>(L""));
 SendMessageW(chats,LB_SETCURSEL,2,0);PostMessageW(ai,WM_COMMAND,MAKEWPARAM(113,LBN_SELCHANGE),reinterpret_cast<LPARAM>(chats));
 int chars=0;for(int i=0;i<200;++i){Sleep(100);chars=static_cast<int>(SendMessageW(transcript,WM_GETTEXTLENGTH,0,0));if(chars>0)break;}
 if(chars==0){std::puts("Selected chat did not load any history");return 7;}
 std::printf("PASS: live editor UI loaded %lld chats, %lld models and selected history (%d characters)\n",static_cast<long long>(chatCount),static_cast<long long>(modelCount),chars);
 if(argc==4 && !lstrcmpW(argv[3],L"send")){
  HWND input=GetDlgItem(ai,107);if(!input)return 8;
  SendMessageW(chats,LB_SETCURSEL,0,0);SendMessageW(ai,WM_COMMAND,MAKEWPARAM(113,LBN_SELCHANGE),reinterpret_cast<LPARAM>(chats));
  SendMessageW(input,WM_SETTEXT,0,reinterpret_cast<LPARAM>(L"Answer only the word ready. Do not use tools."));
  SendMessageW(ai,WM_COMMAND,MAKEWPARAM(108,BN_CLICKED),reinterpret_cast<LPARAM>(send));
  bool complete=false;wchar_t text[4096]{};
  for(int i=0;i<900;++i){Sleep(100);SendMessageW(transcript,WM_GETTEXT,4096,reinterpret_cast<LPARAM>(text));
   wchar_t* reply=wcsstr(text,L"AI:");if(reply){reply+=3;while(*reply && iswspace(*reply))++reply;}
   if(reply && *reply && IsWindowEnabled(send)){complete=true;break;}
   if(wcsstr(text,L"[\x041e\x0448\x0438\x0431\x043a\x0430]"))break;
  }
  if(!complete || !wcsstr(text,L"\x0412\x044b:") || wcsstr(text,L"[\x041e\x0448\x0438\x0431\x043a\x0430]")){std::puts("Ephemeral chat did not render an answer");return 10;}
  std::puts("PASS: editor UI sent an ephemeral Codex message and displayed the answer");
 }
 SendMessageW(ai,WM_CLOSE,0,0);return 0;}
