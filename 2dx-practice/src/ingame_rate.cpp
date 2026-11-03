#include "core.hpp"
#include "addresses.hpp"
#include "request_policy.hpp"
#include "native_calls.hpp"
#include "native_hooks.hpp"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <cwchar>
#include <shellapi.h>
#include <stdexcept>
namespace ingame_rate {
uintptr_t base{};std::atomic<int> percent{100}, selected{100};std::atomic<bool> tainted{false}, scratch_selected{false}, scratch_played{false};bool course_locked=false;std::atomic<bool> published{false};
std::mutex audio_mutex;
template<class T>T& field(void* p,size_t n){return *reinterpret_cast<T*>(static_cast<char*>(p)+n);}
template<class F>F native(size_t r){return reinterpret_cast<F>(base+addresses::rva(r));}
void log(const char* msg){FILE* f=std::fopen("practice-mod/runtime.log","a");if(f){std::fprintf(f,"pid=%lu rate=%d tick=%llu %s\n",GetCurrentProcessId(),percent.load(),GetTickCount64(),msg);std::fclose(f);}}
bool free_dan_no_save(){
 auto mod=GetModuleHandleW(L"2dx-free-dan.dll");if(!mod)return false;
 const auto proc=GetProcAddress(mod,"FreeDanBlocksGradeSaveV1");BOOL(*query)(){};
 static_assert(sizeof(query)==sizeof(proc));std::memcpy(&query,&proc,sizeof(query));
 return query && query();
}
bool preparing=true;
[[noreturn]] void fatal(const char* msg){log(msg);if(preparing)throw std::runtime_error(msg);TerminateProcess(GetCurrentProcess(),91);std::abort();}
using Feed=int(*)(void*,float*,int,float*,float*);
using Destroy=void*(*)(void*,unsigned);
using Clock=double(*)(void*);
using Seek=void(*)(void*,double);
using Getter=void*(*)(int,int);
using Chart=bool(*)(void*,const char*,int);
using Request=int(*)(void*,int*,int,void*,void*);
using Update=void(*)(void*,bool,void*);
Feed original_feed{};Destroy original_voice_destroy{},original_feeder_destroy{};Clock original_clock{};Seek original_seek{};
enum class AudioRole { system, song, preview };
struct VoiceTag {void* source;AudioRole role;};
struct SourceTag {size_t references;AudioRole role;};
struct State {rate_mod::Stream stream;int expected{};bool started=false;void* source{};AudioRole role{};};
std::atomic<bool> song_armed{false};
std::unordered_map<void*,VoiceTag> voices;
std::unordered_map<void*,SourceTag> sources;
std::unordered_map<void*,State> streams;
void clear_source_streams(void* source){
 for(auto it=streams.begin();it!=streams.end();)if(it->second.source==source)it=streams.erase(it);else ++it;
}
void untag(void* voice){
 auto it=voices.find(voice);
 if(it!=voices.end()){
  auto source=sources.find(it->second.source);
  if(source!=sources.end() && --source->second.references==0){clear_source_streams(source->first);sources.erase(source);}
  voices.erase(it);
 }
}
AudioRole role_for(int bank,int index){
 const int scene=*native<int*>(0x107ecf0);
 if(bank==4 && index==0 && (scene==23 || scene==28 || scene==25))return AudioRole::preview;
 if(bank==2 && song_armed && addresses::is_stage(scene))return AudioRole::song;
 return AudioRole::system;
}
void tag_voice(void* voice,AudioRole role){
 void* source=field<void*>(voice,0x50);
 auto it=voices.find(voice);
 if(it!=voices.end() && it->second.source==source && it->second.role==role)return;
 untag(voice);
 // Banks share pooled source/voice objects. A reused source must lose every
 // old classification, including when it is now a folder/system voice.
 auto old=sources.find(source);
 if(old!=sources.end() && old->second.role!=role){
  for(auto v=voices.begin();v!=voices.end();)if(v->second.source==source)v=voices.erase(v);else ++v;
  clear_source_streams(source);
  sources.erase(old);
 }
 if(role!=AudioRole::system && source){voices[voice]={source,role};auto& s=sources[source];++s.references;s.role=role;}
}
void* get_voice(int bank,int index){
 auto result=native<Getter>(0xa907d0)(bank,index);
 if(result && field<void*>(result,0)){
  std::lock_guard<std::mutex> guard(audio_mutex);
  tag_voice(field<void*>(result,0),role_for(bank,index));
 }
 return result;
}
void* destroy_voice(void* voice,unsigned flags){ {std::lock_guard<std::mutex> guard(audio_mutex);untag(voice);} return original_voice_destroy(voice,flags); }
void* destroy_feeder(void* feeder,unsigned flags){ {std::lock_guard<std::mutex> guard(audio_mutex);streams.erase(feeder);} return original_feeder_destroy(feeder,flags); }
int feed(void* feeder,float* out,int bytes,float* left,float* right) noexcept {
 try {
  std::unique_lock<std::mutex> guard(audio_mutex);
  void* source=field<void*>(feeder,8);auto tag=sources.find(source);
  if(tag==sources.end()){streams.erase(feeder);guard.unlock();return original_feed(feeder,out,bytes,left,right);}
  const auto role=tag->second.role;
  const int rate=role==AudioRole::preview?selected.load():percent.load();
  if(rate==100 && !streams.count(feeder)){guard.unlock();return original_feed(feeder,out,bytes,left,right);}
  if(bytes<0 || bytes%8 || bytes>8*1024*1024)fatal("Unexpected PCM buffer size.");
  auto& state=streams[feeder];
  if(!state.started || state.expected!=field<int>(feeder,0x34) || state.source!=source || state.role!=role){state.stream.reset();state.started=true;state.source=source;state.role=role;}
  // Gains are refreshed even when a very small block uses buffered samples only.
  *left=field<float>(feeder,0x20);*right=field<float>(feeder,0x24);
  int got=state.stream.render(out,bytes/8,rate,[&](float* dst,int frames){return original_feed(feeder,dst,frames*8,left,right)/8;});
  state.expected=field<int>(feeder,0x34);
  return got*8;
 } catch(...) {fatal("Audio resampling failed.");}
}
double voice_clock(void* voice){
 double t=original_clock(voice);
 std::lock_guard<std::mutex> guard(audio_mutex);
 auto tag=voices.find(voice);if(tag==voices.end())return t;
 if(field<int>(voice,0x88)==-1){
  auto begin=field<void**>(voice,0x38),end=field<void**>(voice,0x40);
  if(begin && begin<end){void* feeder=field<void*>(*begin,8);auto it=streams.find(feeder);
   if(it!=streams.end() && it->second.expected==field<int>(feeder,0x34)){
    auto source=field<void*>(voice,0x50);
    if(field<uintptr_t>(source,0)==base+addresses::rva(0xe6d080)){auto format=field<void*>(source,0x28);auto hz=field<uint32_t>(format,4);if(hz)t-=it->second.stream.pending()*1000/hz;}
   }
  }
 }
 // Preview fades/cutoff are expressed in source milliseconds. Only the
 // gameplay clock follows the transformed chart's time scale.
 return std::max(0.0,t)*(tag->second.role==AudioRole::song?100.0/percent.load():1.0);
}
void voice_seek(void* voice,double ms){
 bool song=false;
 {std::lock_guard<std::mutex> guard(audio_mutex);auto tag=voices.find(voice);song=tag!=voices.end() && tag->second.role==AudioRole::song;
  if(tag!=voices.end()){auto begin=field<void**>(voice,0x38),end=field<void**>(voice,0x40);for(auto p=begin;p && p<end;++p)streams.erase(field<void*>(*p,8));}}
 original_seek(voice,song?ms*percent/100:ms);
}
bool external_uploader();
void stop_preview(){
 if(!base || !published)return;
 native<void(*)()>(0x5c4db0)();
 native<void(*)(void*,int)>(0xa90400)(native<void*(*)()>(0x5c5300)(),4);
}
void enter_selection(){
 if(!base || !published)return;
 // Stop before clearing tags: otherwise a slow song's buffered tail would
 // resume at native speed underneath the new selector's BGM.
 native<void(*)(void*,int)>(0xa90400)(native<void*(*)()>(0x5c5300)(),2);
 stop_preview();
 std::lock_guard<std::mutex> guard(audio_mutex);
 song_armed=false;voices.clear();sources.clear();streams.clear();
}
void freeze_rate(){
 std::lock_guard<std::mutex> guard(audio_mutex);
 const int next=selected.load();
 const bool scratch=scratch_selected.load();
 if((next!=100 || scratch) && external_uploader())fatal("Remove external score uploader before rate practice; use boot.bat.");
 scratch_played=scratch;tainted.store(next!=100 || scratch);
 if(percent.exchange(next)!=next)streams.clear();
 song_armed=true;
}
bool chart(void* dest,const char* path,int index){
 stop_preview();
 freeze_rate();
 if(!native<Chart>(0x8269a0)(dest,path,index))return false;
 if(scratch_played && !rate_mod::all_scratch(static_cast<rate_mod::Event*>(dest),0x18000/8))fatal("ALL SCRATCH chart conversion failed.");
 if(!rate_mod::transform(static_cast<rate_mod::Event*>(dest),0x18000/8,percent))fatal("Chart is malformed or cannot represent this rate.");
 log("chart scaled");return true;
}
std::vector<practice_network::Entry> request_entries;
void prepare_requests(){
 const auto& layout=addresses::layout();
 if(!layout.request_count || layout.request_count>128)fatal("Invalid request table size.");
 request_entries.resize(layout.request_count);
 for(size_t i=0;i<request_entries.size();++i)
  if(!addresses::read(base+layout.request_table+i*sizeof(practice_network::Entry),request_entries[i]))
   fatal("Cannot read native request table.");
 if(!practice_network::valid(request_entries))fatal("Unsupported native request methods; practice aborted.");
}
int request(void* manager,int* handle,int id,void* input,void* output){
 if(tainted.load() || free_dan_no_save()){
  const auto* entry=practice_network::find(request_entries,id);
  if(entry && practice_network::is_write(*entry)){
   char message[160];std::snprintf(message,sizeof(message),"practice upload blocked id=%d method=%s",id,entry->method);
   log(message);return 3;
  }
 }
 return native<Request>(0xab7b80)(manager,handle,id,input,output);
}
// Only the result call is wrapped; profile option values are never changed.
void* result_options(void* manager,void* output,int side,int style,bool compact){
 auto result=native<void*(*)(void*,void*,int,int,bool)>(0x895b00)(manager,output,side,style,compact);
 if(percent.load()==100 && !scratch_played.load())return result;
 const size_t length=field<size_t>(output,0x10),capacity=field<size_t>(output,0x18);
 const char* data=capacity<16?static_cast<char*>(output):field<char*>(output,0);
 std::string label(data,length);
 if(label=="OFF" || label=="OFF/OFF")label.clear();
 char suffix[16];const int rate=percent.load();std::snprintf(suffix,sizeof(suffix),"%d.%02dx",rate/100,rate%100);
 if(scratch_played){if(!label.empty())label+=" / ";label+="ALL-SCR";}
 if(rate!=100){if(!label.empty())label+=" / ";label+=suffix;}
 // Allocate/free with the game's own MSVC string implementation.
 native<void*(*)(void*,const char*,size_t)>(0x365840)(output,label.data(),label.size());
 return result;
}
#include "practice_guards.inc"
#include "video_rate.inc"
self_dan::CallHook result_call;
self_dan::CallHook calls[20];self_dan::SlotHook slots[5];
void publish_menu();
void prepare_menu();
void publish(){
 if(published.load())return;
 preparing=false;
 if(!self_dan::install_calls(calls,20))fatal("Conflicting chart/audio patch; practice aborted.");
 if(!self_dan::install_calls(&result_call,1))fatal("Result option patch conflict.");
 if(self_dan::install_hooks(slots,5)!=self_dan::HookStatus::installed)fatal("Audio virtual table conflict.");
 publish_practice();publish_video();publish_menu();published=true;log("in-game rate hooks ready");
}
void prepare(uintptr_t game){
 base=game;prepare_requests();
 const size_t getter_sites[]={0xa8ea8f,0xa8eae1,0xa8eafa,0xa8ebcd,0xa8ec33,0xa8ecac,0xa8f9b5,0xa8f9de,0xa8fd79,0xa8fee9,0xa8ff54,0xa8ff84,0xa8ffa9,0xa90009,0xa90059,0xa90111,0xa90161,0xa904fc};
 calls[0].site=native<uint8_t*>(0x826ad8);calls[0].expected_target=native<void*>(0x8269a0);calls[0].replacement=reinterpret_cast<void*>(&chart);
 for(size_t i=0;i<18;++i){calls[i+1].site=native<uint8_t*>(getter_sites[i]);calls[i+1].expected_target=native<void*>(0xa907d0);calls[i+1].replacement=reinterpret_cast<void*>(&get_voice);}
 calls[19].site=native<uint8_t*>(0x5c320b);calls[19].expected_target=native<void*>(0xab7b80);calls[19].replacement=reinterpret_cast<void*>(&request);
 if(!self_dan::prepare_calls(calls,20))fatal("Unsupported or already modified native call sites.");
 result_call.site=native<uint8_t*>(0x91de28);result_call.expected_target=native<void*>(0x895b00);result_call.replacement=reinterpret_cast<void*>(&result_options);
 if(!self_dan::prepare_calls(&result_call,1))fatal("Unsupported result option call.");
 // Publication happens on the scene thread before practice can be enabled.
 // No live unaligned displacement write is performed by the worker.
 const size_t locations[]={0xe77770,0xe77768,0xe775a8,0xe77610,0xe77618};
 const size_t targets[]={0x4cb050,0x4c8470,0x4c40c0,0x4c30a0,0x4c3160};
 void* replacements[]={reinterpret_cast<void*>(&feed),reinterpret_cast<void*>(&destroy_feeder),reinterpret_cast<void*>(&destroy_voice),reinterpret_cast<void*>(&voice_clock),reinterpret_cast<void*>(&voice_seek)};
 original_feed=native<Feed>(targets[0]);original_feeder_destroy=native<Destroy>(targets[1]);original_voice_destroy=native<Destroy>(targets[2]);original_clock=native<Clock>(targets[3]);original_seek=native<Seek>(targets[4]);
 for(int i=0;i<5;++i)slots[i]={native<void**>(locations[i]),native<void*>(targets[i]),replacements[i]};
 for(const auto& slot:slots){void* value{};SIZE_T got{};
  if(!ReadProcessMemory(GetCurrentProcess(),slot.slot,&value,sizeof(value),&got) ||
     got!=sizeof(value) || value!=slot.expected)fatal("Audio virtual table conflict.");}
 prepare_practice();prepare_video();prepare_menu();log("score protection ready");
}
#include "advanced_rate.inc"
} // namespace ingame_rate

extern "C" __declspec(dllexport) unsigned PracticePlayedOptionsV1(){
 return unsigned(ingame_rate::percent.load()) | (ingame_rate::scratch_played.load()?0x10000u:0u);
}
