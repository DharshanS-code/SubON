#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include "SubOnDsp.h"
#include <cmath>
#include <sstream>
#pragma comment(lib, "Comctl32.lib")
namespace {
enum { BASS=101, TREBLE, PREAMP, LIMITER, FLAT, BOOST, PREVIEW, STATUS };
HWND bassBar, trebleBar, preampBar, limiterBox, statusText;
HWND label(HWND p,const wchar_t*t,int x,int y,int w,int h){return CreateWindowW(L"STATIC",t,WS_CHILD|WS_VISIBLE,x,y,w,h,p,nullptr,GetModuleHandleW(nullptr),nullptr);}
HWND bar(HWND p,int id,int x,int y){HWND h=CreateWindowW(TRACKBAR_CLASSW,L"",WS_CHILD|WS_VISIBLE|TBS_AUTOTICKS,x,y,290,32,p,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);SendMessageW(h,TBM_SETRANGE,TRUE,MAKELONG(-12,12));SendMessageW(h,TBM_SETPOS,TRUE,0);return h;}
int value(HWND h){return (int)SendMessageW(h,TBM_GETPOS,0,0);}
void update(){
 subon::dsp::ProcessorConfig c;c.bassDb=value(bassBar);c.trebleDb=value(trebleBar);c.preampDb=value(preampBar);c.limiterEnabled=SendMessageW(limiterBox,BM_GETCHECK,0,0)==BST_CHECKED;
 subon::dsp::AudioProcessor p(c);double peak=0;
 for(int i=0;i<4800;++i){double x=.35*sin(2*3.141592653589793*80*i/48000.)+.2*sin(2*3.141592653589793*1000*i/48000.);peak=(std::max)(peak,std::abs(p.process(x)));}
 std::wostringstream s;s<<L"DSP preview peak: "<<peak<<L"\nPrototype only — not processing Windows system audio.";
 SetWindowTextW(statusText,s.str().c_str());
}
LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM){
 if(m==WM_CREATE){InitCommonControls();label(w,L"SubON | Windows DSP prototype",24,18,400,30);label(w,L"Bass shelf (dB)",24,62,140,24);bassBar=bar(w,BASS,170,54);label(w,L"Treble shelf (dB)",24,108,140,24);trebleBar=bar(w,TREBLE,170,100);label(w,L"Preamp (dB)",24,154,140,24);preampBar=bar(w,PREAMP,170,146);limiterBox=CreateWindowW(L"BUTTON",L"Enable limiter",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,24,195,180,28,w,(HMENU)(INT_PTR)LIMITER,GetModuleHandleW(nullptr),nullptr);SendMessageW(limiterBox,BM_SETCHECK,BST_CHECKED,0);CreateWindowW(L"BUTTON",L"Flat",WS_CHILD|WS_VISIBLE,24,235,85,30,w,(HMENU)(INT_PTR)FLAT,GetModuleHandleW(nullptr),nullptr);CreateWindowW(L"BUTTON",L"Bass Boost",WS_CHILD|WS_VISIBLE,118,235,105,30,w,(HMENU)(INT_PTR)BOOST,GetModuleHandleW(nullptr),nullptr);CreateWindowW(L"BUTTON",L"Apply / Preview",WS_CHILD|WS_VISIBLE,232,235,130,30,w,(HMENU)(INT_PTR)PREVIEW,GetModuleHandleW(nullptr),nullptr);statusText=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,24,282,480,65,w,(HMENU)(INT_PTR)STATUS,GetModuleHandleW(nullptr),nullptr);update();return 0;}
 if(m==WM_HSCROLL){update();return 0;} if(m==WM_COMMAND){switch(LOWORD(a)){case FLAT:SendMessageW(bassBar,TBM_SETPOS,TRUE,0);SendMessageW(trebleBar,TBM_SETPOS,TRUE,0);SendMessageW(preampBar,TBM_SETPOS,TRUE,0);break;case BOOST:SendMessageW(bassBar,TBM_SETPOS,TRUE,6);SendMessageW(trebleBar,TBM_SETPOS,TRUE,1);SendMessageW(preampBar,TBM_SETPOS,TRUE,-2);break;default:break;}update();return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(w,m,a,0);
}
}
int WINAPI wWinMain(HINSTANCE i,HINSTANCE,PWSTR,int show){WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=i;c.lpszClassName=L"SubONPrototype";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);if(!RegisterClassW(&c))return 1;HWND w=CreateWindowW(c.lpszClassName,L"SubON Windows Prototype",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,550,400,nullptr,nullptr,i,nullptr);if(!w)return 1;ShowWindow(w,show);MSG m{};while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}return (int)m.wParam;}
