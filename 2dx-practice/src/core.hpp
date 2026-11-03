#pragma once
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>
#include <algorithm>
#include <stdexcept>
namespace rate_mod {
inline bool valid_rate(int p) { return p>=70 && p<=150 && p%5==0; }
struct Event { int32_t time; uint8_t type,param; uint16_t value; };
static_assert(sizeof(Event)==8);
// ALL-SCR: one scratch per side/onset, keeping the other chord notes on keys.
// Rebind sounds at their new lanes; LEGACY removes CN duration and BSS tails.
// Work on a copy so a full native chart buffer cannot be partially overwritten.
inline bool all_scratch(Event* events,size_t capacity) {
 size_t length=0;bool ended=false;int32_t end_time=0;
 for(;length<capacity;++length){
  const auto& e=events[length];
  if(ended && (e.time!=end_time || e.type!=6))break;
  if(e.time==INT32_MAX)break;
  if(e.type==6){ended=true;end_time=e.time;}
 }
 if(!ended)return false;
 uint16_t sounds[2][8]{};int emitted[2][8];int32_t previous[2][8];
 for(auto& row:emitted)std::fill(std::begin(row),std::end(row),-1);
 for(auto& row:previous)std::fill(std::begin(row),std::end(row),INT32_MIN);
 unsigned counts[2]{};std::vector<Event> out;out.reserve(length);
 for(size_t begin=0;begin<length;){
  size_t end=begin+1;while(end<length && events[end].time==events[begin].time)++end;
  size_t chosen[2]={end,end};
  for(size_t i=begin;i<end;++i){const auto& e=events[i];
   if(e.type<2 && e.param<8 && (chosen[e.type]==end || e.param==7))chosen[e.type]=i;
  }
  for(size_t i=begin;i<end;++i){auto e=events[i];
   if((e.type==2 || e.type==3) && e.param<8){sounds[e.type-2][e.param]=e.value;continue;}
   if(e.type<2){
    if(e.param==107)continue;
    if(e.param>=8)return false;
    const auto sound=sounds[e.type][e.param];
    if(i==chosen[e.type])e.param=7;
    e.value=0;++counts[e.type];
    if(emitted[e.type][e.param]!=sound){
     // Arm the next sound between notes, so early hits also get the right
     // sample. Installing it only at the note time plays the previous sound.
     const auto last=previous[e.type][e.param];
     const int32_t at=last==INT32_MIN?std::min(0,e.time):int32_t(int64_t(last)+(int64_t(e.time)-last+1)/2);
     out.push_back({at,uint8_t(e.type+2),e.param,sound});emitted[e.type][e.param]=sound;
    }
    previous[e.type][e.param]=e.time;
   }
   out.push_back(e);
  }
  begin=end;
 }
 if(out.size()>capacity)return false;
 std::stable_sort(out.begin(),out.end(),[](const Event& a,const Event& b){return a.time<b.time;});
 for(auto& e:out)if(e.type==16 && e.param<2){if(counts[e.param]>UINT16_MAX)return false;e.value=uint16_t(counts[e.param]);}
 std::fill(events,events+capacity,Event{INT32_MAX,0,0,0});
 std::copy(out.begin(),out.end(),events);return true;
}
inline int64_t scaled(int64_t n,int p) { return std::llround(double(n)*100/p); }
// Validate and transform a copy first: malformed charts cannot be half changed.
inline bool transform(Event* events,size_t capacity,int p) {
 if(!valid_rate(p))return false;
 if(p==100)return true;
 std::vector<Event> copy; bool ended=false;int32_t end_time=0;
 for(size_t i=0;i<capacity;++i) {
  Event e=events[i];
  if(ended && (e.time!=end_time || e.type!=6))break;
  if(e.time==INT32_MAX)break;
  auto t=scaled(e.time,p);
  if(t<INT32_MIN || t>=INT32_MAX)return false;
  if(e.type==0 || e.type==1 || e.type==100 || e.type==101) {
   auto duration=scaled(int64_t(e.time)+e.value,p)-t;
   if(duration<0 || duration>UINT16_MAX)return false;
   e.value=uint16_t(duration);
  } else if(e.type==4) {
   if(!e.value){e.time=int32_t(t);copy.push_back(e);continue;}
   const double bpm=double(e.value)*p/(100.0*std::max(1,int(e.param)));
   int denominator=std::min(255,int(std::floor(65535.0/bpm)));
   if(denominator<1)return false;
   e.param=uint8_t(denominator);e.value=uint16_t(std::lround(bpm*denominator));
  }
  if(e.type==6){ended=true;end_time=e.time;}
  e.time=int32_t(t);copy.push_back(e);
 }
 if(!ended)return false;
 std::copy(copy.begin(),copy.end(),events);return true;
}
// Continuous stereo linear resampling. Integer phase prevents drift at all 17 rates.
// read(float*, frames) returns source frames; scratch has extra SIMD padding.
struct Stream {
 std::vector<float> fifo, scratch;
 int64_t phase=0;
 bool eof=false;
 void reset(){fifo.clear();phase=0;eof=false;}
 double pending()const{return double(fifo.size()/2)-double(phase)/100;}
 template<class Read> int render(float* out,int frames,int p,Read read) {
  if(frames<=0 || !valid_rate(p))return 0;
  const int64_t end=phase+int64_t(frames)*p;
  const auto needed=size_t(std::max((phase+int64_t(frames-1)*p)/100+2,end/100));
  while(fifo.size()/2<needed && !eof) {
   int ask=int(std::min<size_t>(needed-fifo.size()/2,2048));
   scratch.resize(size_t(ask)*2+32);
   int got=read(scratch.data(),ask);
   if(got<0 || got>ask)throw std::runtime_error("invalid native PCM length");
   fifo.insert(fifo.end(),scratch.begin(),scratch.begin()+got*2);
   if(got<ask)eof=true;
  }
  int produced=0;const size_t available=fifo.size()/2;
  for(;produced<frames && phase<int64_t(available)*100;++produced,phase+=p) {
   size_t a=size_t(phase/100),b=std::min(a+1,available-1);
   float weight=float(phase%100)/100;
   for(int c=0;c<2;++c)out[produced*2+c]=fifo[a*2+c]+(fifo[b*2+c]-fifo[a*2+c])*weight;
  }
  const auto discard=std::min(size_t(phase/100),available);
  fifo.erase(fifo.begin(),fifo.begin()+discard*2);phase-=int64_t(discard)*100;
  return produced;
 }
};
}
