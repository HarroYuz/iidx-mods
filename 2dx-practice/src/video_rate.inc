// Scale only a movie started for the armed stage. System/menu movies retain
// their native clocks. Each decoder captures its rate at start, including loops.
std::mutex video_mutex;
std::unordered_map<void*,int> mf_rates;
using VideoClock=int64_t*(*)(void*,int64_t*);
int stage_video_rate(){
 const int scene=*native<int*>(0x107ecf0);
 return song_armed && addresses::is_stage(scene)?percent.load():100;
}
int64_t* mf_clock(void* context,int64_t* output){
 auto result=native<VideoClock>(0xa62640)(context,output);
 std::lock_guard<std::mutex> guard(video_mutex);
 const auto it=mf_rates.find(context);
 if(it!=mf_rates.end())*output=(*output/100)*it->second+(*output%100)*it->second/100;
 return result;
}
void mf_start(void* player,bool loop){
 {std::lock_guard<std::mutex> guard(video_mutex);mf_rates[field<void*>(player,0x18)]=stage_video_rate();}
 native<void(*)(void*,bool)>(0xa61eb0)(player,loop);
}
void mf_stop(void* player){
 native<void(*)(void*)>(0xa61f10)(player);
 std::lock_guard<std::mutex> guard(video_mutex);mf_rates.erase(field<void*>(player,0x18));
}
// The WMV reader exposes its scheduler clock as a callback/user pair. Scale
// that clock before the reader computes frame deadlines, pause and seek times.
struct WmvClock {Clock original;void* user;double anchor;int rate;};
std::unordered_map<void*,WmvClock> wmv_clocks;
double wmv_clock(void* context){
 std::lock_guard<std::mutex> guard(video_mutex);
 const auto& c=wmv_clocks.at(context);
 return c.anchor+(c.original(c.user)-c.anchor)*c.rate/100.0;
}
void* wmv_context(void* player){
 void* reader=field<void*>(player,0x18);return reader?field<void*>(reader,8):nullptr;
}
void restore_wmv(void* context){
 if(!context)return;
 std::lock_guard<std::mutex> guard(video_mutex);
 auto it=wmv_clocks.find(context);
 if(it!=wmv_clocks.end() && field<Clock>(context,0x50)==&wmv_clock){
  field<Clock>(context,0x50)=it->second.original;field<void*>(context,0x58)=it->second.user;
 }
 // Entries remain stable for an already-dispatched callback. Reuse replaces
 // the entry only after the previous player's native stop has returned.
}
void wmv_start(void* player,bool loop){
 void* context=wmv_context(player);
 if(context){
  restore_wmv(context);
  const int rate=stage_video_rate();
  if(rate!=100){
   auto clock=field<Clock>(context,0x50);void* user=field<void*>(context,0x58);
   if(clock){
    const double anchor=clock(user);
    std::lock_guard<std::mutex> guard(video_mutex);
    wmv_clocks[context]={clock,user,anchor,rate};
    field<void*>(context,0x58)=context;field<Clock>(context,0x50)=&wmv_clock;
   }
  }
 }
 native<void(*)(void*,bool)>(0xa63020)(player,loop);
}
void wmv_stop(void* player){
 native<void(*)(void*)>(0xa630a0)(player);restore_wmv(wmv_context(player));
}
self_dan::CallHook video_calls[3];self_dan::SlotHook video_slots[4];
void prepare_video(){
 const size_t sites[]={0xa6142b,0xa6151a,0xa62859};
 for(int i=0;i<3;++i){auto& h=video_calls[i];h.site=native<uint8_t*>(sites[i]);h.expected_target=native<void*>(0xa62640);h.replacement=reinterpret_cast<void*>(&mf_clock);}
 if(!self_dan::prepare_calls(video_calls,3))fatal("BGA clock call mismatch.");
 const size_t locations[]={0xdc9b00,0xdc9b10,0xdc9d00,0xdc9d10};
 const size_t targets[]={0xa61eb0,0xa61f10,0xa63020,0xa630a0};
 void* wrappers[]={reinterpret_cast<void*>(&mf_start),reinterpret_cast<void*>(&mf_stop),reinterpret_cast<void*>(&wmv_start),reinterpret_cast<void*>(&wmv_stop)};
 for(int i=0;i<4;++i)video_slots[i]={native<void**>(locations[i]),native<void*>(targets[i]),wrappers[i]};
}
void publish_video(){
 if(!self_dan::install_calls(video_calls,3) || self_dan::install_hooks(video_slots,4)!=self_dan::HookStatus::installed)fatal("BGA hook conflict.");
}
