#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <windowsx.h>
#include "SubOnDsp.h"
#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Winmm.lib")

namespace {
enum : int { OPEN_FILE=100, PLAY_PAUSE, STOP_PLAY, PRESET_FLAT, PRESET_BASS, PRESET_VOCAL, PREAMP=200, BASS, TREBLE, LIMITER, EQ0=220, STATUS=260 };
struct WavData { WAVEFORMATEX format{}; std::vector<std::uint8_t> pcm; };
HWND g_window{}, g_status{}, g_preamp{}, g_bass{}, g_treble{}, g_limiter{};
std::array<HWND, subon::dsp::kEqualizerBandCount> g_eq{};
std::mutex g_mutex;
subon::dsp::ProcessorConfig g_config;
WavData g_wav;
std::thread g_worker;
std::atomic<bool> g_stop{false}, g_playing{false};
HWAVEOUT g_wave{};
std::wstring g_filename;
std::size_t g_position=0;
HFONT g_font{};

HWND control(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id=0){
 HWND hnd=CreateWindowW(cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,g_window,id?(HMENU)(INT_PTR)id:nullptr,GetModuleHandleW(nullptr),nullptr);
 if(g_font) SendMessageW(hnd,WM_SETFONT,(WPARAM)g_font,TRUE); return hnd;
}
HWND label(const wchar_t* text,int x,int y,int w=145){return control(L"STATIC",text,0,x,y,w,22);}
HWND slider(int id,int x,int y,int initial,int low=-12,int high=12,int width=230){
 HWND h=control(TRACKBAR_CLASSW,L"",TBS_AUTOTICKS,x,y,width,28,id);
 SendMessageW(h,TBM_SETRANGE,TRUE,MAKELONG(low,high));SendMessageW(h,TBM_SETPOS,TRUE,initial);SendMessageW(h,TBM_SETTICFREQ,4,0);return h;
}
int pos(HWND h){return (int)SendMessageW(h,TBM_GETPOS,0,0);}
void setStatus(const std::wstring& s){if(g_status)SetWindowTextW(g_status,s.c_str());}
void snapshotConfig(subon::dsp::ProcessorConfig& c){
 std::lock_guard<std::mutex> lock(g_mutex);
 c.sampleRate=g_config.sampleRate;c.preampDb=pos(g_preamp);c.bassDb=pos(g_bass);c.trebleDb=pos(g_treble);
 c.limiterEnabled=SendMessageW(g_limiter,BM_GETCHECK,0,0)==BST_CHECKED;
 for(std::size_t i=0;i<g_eq.size();++i)c.equalizerDb[i]=pos(g_eq[i]);
 g_config=c;
}
bool readWav(const std::wstring& path,WavData& out,std::wstring& error){
 std::ifstream f(path,std::ios::binary);
 if(!f){error=L"Could not open that file.";return false;}
 char riff[4],wave[4];std::uint32_t riffSize=0;
 f.read(riff,4);f.read(reinterpret_cast<char*>(&riffSize),4);f.read(wave,4);
 if(!f||std::memcmp(riff,"RIFF",4)||std::memcmp(wave,"WAVE",4)||riffSize<4){
  error=L"Choose a valid RIFF/WAVE file.";return false;
 }
 f.seekg(0,std::ios::end);const auto fileEnd=f.tellg();
 const std::uint64_t riffEnd=8ull+riffSize;
 if(fileEnd==std::streampos(-1)||riffEnd>static_cast<std::uint64_t>(static_cast<std::streamoff>(fileEnd))){
  error=L"WAV file is truncated or has an invalid RIFF size.";return false;
 }
 f.seekg(12,std::ios::beg);
 bool gotFmt=false,gotData=false;WAVEFORMATEX fmt{};std::vector<std::uint8_t> data;
 while(f&&(!gotFmt||!gotData)){
  const auto chunkHeaderPos=f.tellg();
  if(chunkHeaderPos==std::streampos(-1)||static_cast<std::uint64_t>(static_cast<std::streamoff>(chunkHeaderPos))+8>riffEnd)break;
  char id[4];std::uint32_t size=0;
  f.read(id,4);f.read(reinterpret_cast<char*>(&size),4);if(!f)break;
  const auto payloadPos=f.tellg();
  if(payloadPos==std::streampos(-1)||static_cast<std::uint64_t>(static_cast<std::streamoff>(payloadPos))+size+(size&1u)>riffEnd){
   error=L"WAV file contains a truncated or out-of-bounds chunk.";return false;
  }
  if(!std::memcmp(id,"fmt ",4)){
   if(size<16||size>4096){error=L"Unsupported WAV format chunk.";return false;}
   std::vector<char> b(size);f.read(b.data(),static_cast<std::streamsize>(size));
   if(!f){error=L"Could not read the WAV format chunk.";return false;}
   std::uint16_t tag=0;std::memcpy(&tag,b.data(),2);
   std::memcpy(&fmt.nChannels,b.data()+2,2);std::memcpy(&fmt.nSamplesPerSec,b.data()+4,4);
   std::memcpy(&fmt.nAvgBytesPerSec,b.data()+8,4);std::memcpy(&fmt.nBlockAlign,b.data()+12,2);
   std::memcpy(&fmt.wBitsPerSample,b.data()+14,2);fmt.wFormatTag=tag;gotFmt=true;
   if(size&1)f.seekg(1,std::ios::cur);
  }else if(!std::memcmp(id,"data",4)){
   if(size>512u*1024u*1024u){error=L"File is too large (maximum 512 MB).";return false;}
   data.resize(size);f.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(size));
   if(!f){error=L"WAV audio data is truncated.";return false;}
   gotData=true;if(size&1)f.seekg(1,std::ios::cur);
  }else{f.seekg(static_cast<std::streamoff>(size+(size&1u)),std::ios::cur);}
 }
 if(!gotFmt||!gotData){error=L"WAV file is missing format or audio data.";return false;}
 const bool pcm16=fmt.wFormatTag==WAVE_FORMAT_PCM&&fmt.wBitsPerSample==16;
 const bool pcm24=fmt.wFormatTag==WAVE_FORMAT_PCM&&fmt.wBitsPerSample==24;
 const bool pcm32=fmt.wFormatTag==WAVE_FORMAT_PCM&&fmt.wBitsPerSample==32;
 const bool float32=fmt.wFormatTag==WAVE_FORMAT_IEEE_FLOAT&&fmt.wBitsPerSample==32;
 const unsigned bytesPerSample=fmt.wBitsPerSample/8;
 if(fmt.nChannels<1||fmt.nChannels>2||fmt.nSamplesPerSec<32000||fmt.nSamplesPerSec>192000||
    !(pcm16||pcm24||pcm32||float32)||!fmt.nBlockAlign||
    fmt.nBlockAlign!=fmt.nChannels*bytesPerSample||fmt.nAvgBytesPerSec!=fmt.nSamplesPerSec*fmt.nBlockAlign||
    data.empty()||data.size()%fmt.nBlockAlign!=0){
  error=L"Unsupported or inconsistent WAV format. Use mono/stereo PCM 16/24/32-bit or float32, 32–192 kHz.";return false;
 }
 out.format=fmt;out.pcm=std::move(data);return true;
}
double decodeSample(const std::uint8_t* p,const WAVEFORMATEX& f){
 if(f.wFormatTag==WAVE_FORMAT_IEEE_FLOAT){float v;std::memcpy(&v,p,4);return std::isfinite(v)?v:0.0;}
 if(f.wBitsPerSample==16){std::int16_t v;std::memcpy(&v,p,2);return v/32768.0;}
 if(f.wBitsPerSample==24){std::int32_t v=(std::int32_t(p[0])|(std::int32_t(p[1])<<8)|(std::int32_t(p[2])<<16));if(v&0x800000)v|=~0xffffff;return v/8388608.0;}
 std::int32_t v;std::memcpy(&v,p,4);return v/2147483648.0;
}
void encodeSample(std::uint8_t* p,const WAVEFORMATEX& f,double v){
 v=std::clamp(v,-1.0,1.0);
 if(f.wFormatTag==WAVE_FORMAT_IEEE_FLOAT){float x=(float)v;std::memcpy(p,&x,4);return;}
 if(f.wBitsPerSample==16){auto x=(std::int16_t)std::lrint(v*32767.0);std::memcpy(p,&x,2);return;}
 if(f.wBitsPerSample==24){auto x=(std::int32_t)std::lrint(v*8388607.0);p[0]=x&255;p[1]=(x>>8)&255;p[2]=(x>>16)&255;return;}
 auto x=(std::int32_t)std::llround(v*2147483647.0);std::memcpy(p,&x,4);
}
void playback(){
 WAVEFORMATEX fmt=g_wav.format;
 MMRESULT result=waveOutOpen(&g_wave,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL);
 if(result!=MMSYSERR_NOERROR){g_playing=false;PostMessageW(g_window,WM_APP+1,result,0);return;}
 const std::size_t frameBytes=fmt.nBlockAlign;
 const std::size_t totalFrames=g_wav.pcm.size()/frameBytes;
 const std::size_t chunkFrames=4096;
 subon::dsp::AudioProcessor left, right;
 subon::dsp::ProcessorConfig appliedCfg{};
 bool hasAppliedCfg=false;
 std::size_t frame=g_position;
 while(!g_stop&&frame<totalFrames){
  const std::size_t count=(std::min)(chunkFrames,totalFrames-frame);
  std::vector<std::uint8_t> output(count*frameBytes);
  subon::dsp::ProcessorConfig cfg;snapshotConfig(cfg);cfg.sampleRate=fmt.nSamplesPerSec;
  // Reconfigure only when controls actually change. configure() resets filter
  // histories, so doing it for every block causes audible discontinuities.
  const auto sameConfig=[](const subon::dsp::ProcessorConfig& a,const subon::dsp::ProcessorConfig& b){
   if(a.sampleRate!=b.sampleRate||a.preampDb!=b.preampDb||a.bassDb!=b.bassDb||
      a.trebleDb!=b.trebleDb||a.limiterEnabled!=b.limiterEnabled||
      a.limiterCeiling!=b.limiterCeiling)return false;
   for(std::size_t j=0;j<a.equalizerDb.size();++j)if(a.equalizerDb[j]!=b.equalizerDb[j])return false;
   return true;
  };
  try{
   if(!hasAppliedCfg||!sameConfig(cfg,appliedCfg)){
    left.configure(cfg);right.configure(cfg);appliedCfg=cfg;hasAppliedCfg=true;
   }
  }catch(...){std::memcpy(output.data(),g_wav.pcm.data()+frame*frameBytes,output.size());}
  const int bytesPerSample=fmt.wBitsPerSample/8;
  for(std::size_t i=0;i<count&&!g_stop;++i){
   for(unsigned ch=0;ch<fmt.nChannels;++ch){
    auto* src=g_wav.pcm.data()+(frame+i)*frameBytes+ch*bytesPerSample;
    auto* dst=output.data()+i*frameBytes+ch*bytesPerSample;
    double v=decodeSample(src,fmt);
    try{v=(ch==0?left:right).process(v);}catch(...){}
    encodeSample(dst,fmt,v);
   }
  }
  if(g_stop)break;
  WAVEHDR hdr{};hdr.lpData=(LPSTR)output.data();hdr.dwBufferLength=(DWORD)output.size();
  if(waveOutPrepareHeader(g_wave,&hdr,sizeof(hdr))!=MMSYSERR_NOERROR)break;
  if(waveOutWrite(g_wave,&hdr,sizeof(hdr))!=MMSYSERR_NOERROR){waveOutUnprepareHeader(g_wave,&hdr,sizeof(hdr));break;}
  while(!(hdr.dwFlags&WHDR_DONE)&&!g_stop)Sleep(5);
  waveOutUnprepareHeader(g_wave,&hdr,sizeof(hdr));frame+=count;g_position=frame;
 }
 waveOutReset(g_wave);waveOutClose(g_wave);g_wave=nullptr;g_playing=false;
 if(!g_stop){g_position=0;PostMessageW(g_window,WM_APP+2,0,0);}
}
void stopPlayback(){
 g_stop=true;
 if(g_wave)waveOutReset(g_wave);
 if(g_worker.joinable()&&g_worker.get_id()!=std::this_thread::get_id())g_worker.join();
 g_playing=false;g_stop=false;g_position=0;
}
void openFile(){
 OPENFILENAMEW ofn{};wchar_t path[MAX_PATH]{};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=g_window;ofn.lpstrFile=path;ofn.nMaxFile=MAX_PATH;ofn.lpstrFilter=L"WAV audio (*.wav)\0*.wav\0All files (*.*)\0*.*\0";ofn.nFilterIndex=1;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
 if(!GetOpenFileNameW(&ofn))return;
 stopPlayback();WavData loaded;std::wstring err;
 if(!readWav(path,loaded,err)){setStatus(err);return;}
 g_wav=std::move(loaded);g_filename=path;g_position=0;
 setStatus(L"Loaded: "+g_filename+L" — press Play. WAV playback only; system-wide audio is not intercepted.");
}
void startPlayback(){
 if(g_playing||g_wav.pcm.empty()){if(g_wav.pcm.empty())setStatus(L"Open a WAV file first.");return;}
 g_stop=false;g_playing=true;
 if(g_worker.joinable())g_worker.join();
 g_worker=std::thread(playback);
 setStatus(L"Playing through SubON DSP. Changes apply to subsequent audio blocks.");
}
void preset(int kind){
 SendMessageW(g_preamp,TBM_SETPOS,TRUE,kind==2?2:kind==1?-2:0);
 SendMessageW(g_bass,TBM_SETPOS,TRUE,kind==1?6:kind==2?1:0);
 SendMessageW(g_treble,TBM_SETPOS,TRUE,kind==2?4:kind==1?1:0);
 for(auto h:g_eq)SendMessageW(h,TBM_SETPOS,TRUE,0);
 if(kind==2){SendMessageW(g_eq[2],TBM_SETPOS,TRUE,-2);SendMessageW(g_eq[3],TBM_SETPOS,TRUE,-1);SendMessageW(g_eq[5],TBM_SETPOS,TRUE,2);}
}
LRESULT CALLBACK wndProc(HWND w,UINT m,WPARAM a,LPARAM l){
 if(m==WM_CREATE){
  g_window=w;
  g_font=CreateFontW(17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  label(L"SubON",24,12,200);label(L"Audio player + 10-band equalizer",24,38,350);
  control(L"BUTTON",L"Open WAV",BS_PUSHBUTTON,24,70,100,30,OPEN_FILE);
  control(L"BUTTON",L"Play",BS_PUSHBUTTON,132,70,75,30,PLAY_PAUSE);
  control(L"BUTTON",L"Stop",BS_PUSHBUTTON,215,70,75,30,STOP_PLAY);
  label(L"Preamp (dB)",24,115);g_preamp=slider(PREAMP,170,108,0);
  label(L"Bass shelf (dB)",24,151);g_bass=slider(BASS,170,144,0);
  label(L"Treble shelf (dB)",24,187);g_treble=slider(TREBLE,170,180,0);
  g_limiter=control(L"BUTTON",L"Enable limiter",BS_AUTOCHECKBOX,24,218,150,26,LIMITER);SendMessageW(g_limiter,BM_SETCHECK,BST_CHECKED,0);
  label(L"10-band EQ (dB)",24,250,200);
  for(int i=0;i<10;++i){int y=278+i*34;wchar_t text[32];swprintf_s(text,L"%.0f Hz",subon::dsp::kEqualizerFrequencies[i]);label(text,24,y,100);g_eq[i]=slider(EQ0+i,125,y-5,0,-12,12,275);}
  control(L"BUTTON",L"Flat",BS_PUSHBUTTON,24,630,70,30,PRESET_FLAT);
  control(L"BUTTON",L"Bass Boost",BS_PUSHBUTTON,102,630,105,30,PRESET_BASS);
  control(L"BUTTON",L"Vocal",BS_PUSHBUTTON,215,630,75,30,PRESET_VOCAL);
  g_status=control(L"STATIC",L"Open a WAV file to begin. Supported: PCM 16/24/32-bit or float32 WAV, mono/stereo.",0,24,675,590,58,STATUS);
  return 0;
 }
 if(m==WM_HSCROLL){subon::dsp::ProcessorConfig c;snapshotConfig(c);return 0;}
 if(m==WM_COMMAND){switch(LOWORD(a)){
 case OPEN_FILE:openFile();return 0;
 case PLAY_PAUSE:if(g_playing){stopPlayback();setStatus(L"Stopped.");}else startPlayback();return 0;
 case STOP_PLAY:stopPlayback();setStatus(L"Stopped.");return 0;
 case PRESET_FLAT:preset(0);break;case PRESET_BASS:preset(1);break;case PRESET_VOCAL:preset(2);break;
 default:break;}subon::dsp::ProcessorConfig c;snapshotConfig(c);return 0;}
 if(m==WM_APP+1){setStatus(L"Could not open audio output. Check the Windows output device and format.");return 0;}
 if(m==WM_APP+2){setStatus(L"Playback finished.");return 0;}
 if(m==WM_DESTROY){stopPlayback();if(g_font)DeleteObject(g_font);PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,m,a,l);
}
}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){
 INITCOMMONCONTROLSEX cc{sizeof(cc),ICC_BAR_CLASSES};InitCommonControlsEx(&cc);
 WNDCLASSW wc{};wc.lpfnWndProc=wndProc;wc.hInstance=inst;wc.lpszClassName=L"SubONFullDesktop";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
 if(!RegisterClassW(&wc))return 1;
 HWND w=CreateWindowExW(0,wc.lpszClassName,L"SubON — Windows Audio Player",WS_OVERLAPPEDWINDOW|WS_VSCROLL,CW_USEDEFAULT,CW_USEDEFAULT,650,790,nullptr,nullptr,inst,nullptr);
 if(!w)return 1;g_window=w;ShowWindow(w,show);UpdateWindow(w);
 MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return (int)msg.wParam;
}