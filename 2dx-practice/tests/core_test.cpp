#include "../src/core.hpp"
#include "../src/request_policy.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace rate_mod;
std::vector<float> run(int rate,int block,int frames=10001){
 Stream s;int cursor=0;std::vector<float> result,buf(size_t(block)*2);
 auto read=[&](float* p,int n){n=std::min(n,frames-cursor);for(int i=0;i<n;++i){p[i*2]=float(cursor+i);p[i*2+1]=-p[i*2];}cursor+=n;return n;};
 for(;;){int n=s.render(buf.data(),block,rate,read);result.insert(result.end(),buf.begin(),buf.begin()+n*2);if(n<block)break;}
 assert(result.size()/2==size_t((int64_t(frames)*100+rate-1)/rate));
 for(size_t i=0;i<result.size()/2;++i){float expected=std::min(float(i*rate)/100,float(frames-1));assert(std::abs(result[i*2]-expected)<0.002f);assert(result[i*2+1]==-result[i*2]);}
 return result;
}
int main(){
 // Synthetic renumbering verifies semantics independent of version/ID;
 // this is not a substitute for the unavailable 0909 game binary.
 for(const char* prefix:{"IIDX33","IIDX34"}){
  std::vector<practice_network::Entry> table;
  for(const char* name:practice_network::writes){
   practice_network::Entry e{};e.id=100-int(table.size());
   std::snprintf(e.method,sizeof(e.method),"%s%s",prefix,name);table.push_back(e);
  }
  assert(practice_network::valid(table));
  for(const auto& e:table)assert(practice_network::is_write(e));
  for(const char* name:{"pc.get","music.getrank","music.getranksub","pc.playstart","pc.playend","pc.logout","shop.keepalive"}){
   practice_network::Entry e{};e.id=int(table.size());
   std::snprintf(e.method,sizeof(e.method),"%s%s",prefix,name);
   assert(!practice_network::is_write(e));table.push_back(e);
  }
  assert(practice_network::valid(table));
  auto broken=table;broken[0].id=broken[1].id;assert(!practice_network::valid(broken));
  broken=table;std::memset(broken[0].method,'X',64);assert(!practice_network::valid(broken));
  broken=table;broken.erase(broken.begin());assert(!practice_network::valid(broken));
 }

 for(int p=70;p<=150;p+=5){
  for(int side=0;side<2;++side){
   Event chart[]={{0,4,100,18000},{1000,uint8_t(side),3,1000},{2000,2,3,42},{2500,8,0,3},{3000,16,0,1234},{3500,101,4,500},{5000,6,0,0},{INT32_MAX,0,0,0}};
   Event copy[8];std::memcpy(copy,chart,sizeof(chart));assert(transform(chart,8,p));
   assert(chart[1].time==scaled(1000,p));assert(chart[1].value==scaled(2000,p)-scaled(1000,p));
   assert(chart[2].value==42 && chart[3].value==3 && chart[4].value==1234);
   assert(chart[5].value==scaled(4000,p)-scaled(3500,p));assert(chart[6].time==scaled(5000,p));assert(chart[7].time==INT32_MAX);
   assert(std::abs(double(chart[0].value)/chart[0].param-180.0*p/100)<0.01);
   if(p==100)assert(std::memcmp(copy,chart,sizeof(chart))==0);
  }
  Event legacy[]={{0,4,0,110},{100,4,0,0},{200,6,0,0}};assert(transform(legacy,3,p));assert(std::abs(double(legacy[0].value)/std::max(1,int(legacy[0].param))-110.0*p/100)<0.01);assert(legacy[1].value==0);
  auto reference=run(p,4096);
  for(int block:{1,2,7,64,257,1024})assert(run(p,block)==reference);
  for(int length:{0,1,2,3,7,1024})run(p,31,length);
  Stream s;float out[20];int counter=0;
  auto loop=[&](float* dst,int n){for(int i=0;i<n*2;++i)dst[i]=float((counter++)%16);return n;};
  assert(s.render(out,10,p,loop)==10);s.reset();counter=0;float again[20];assert(s.render(again,10,p,loop)==10);assert(std::memcmp(out,again,sizeof(out))==0);
 }
 Event bad[]={{INT32_MAX-1,4,0,180},{100,6,0,0}},before[2];std::memcpy(before,bad,sizeof(bad));assert(!transform(bad,2,70));assert(!std::memcmp(before,bad,sizeof(bad)));
 Event overflow[]={{0,0,0,65535},{100000,6,0,0}};assert(!transform(overflow,2,70));
 Event no_end[]={{0,2,0,3}};assert(!transform(no_end,1,150));assert(!transform(no_end,1,71));
 Event dp_ends[]={{1000,6,0,0},{1000,6,1,0},{INT32_MAX,0,0,0}};
 assert(transform(dp_ends,3,70));assert(dp_ends[0].time==dp_ends[1].time && dp_ends[2].time==INT32_MAX);
 // Chord + existing scratch + independent DP side + CN/BSS LEGACY.
 Event scr[64]={{0,16,0,6},{0,16,1,2},{0,2,0,10},{0,2,1,20},{0,2,7,30},{0,3,2,40},
  {100,0,0,300},{100,0,1,0},{100,1,2,0},{200,0,0,0},{200,0,7,0},{300,0,107,0},
  {400,1,2,500},{1000,6,0,0},{1000,6,1,0}};
 assert(all_scratch(scr,64));unsigned counts[2]{};unsigned scratches[2]{};int end_count=0;
 for(const auto& e:scr){if(e.time==INT32_MAX)break;if(e.type==6)++end_count;
  if(e.type<2){assert(e.param<8 && e.value==0);++counts[e.type];if(e.param==7)++scratches[e.type];}
 }
 assert(counts[0]==4 && counts[1]==2 && scratches[0]==2 && scratches[1]==2 && end_count==2);
 assert(scr[0].value==4 && scr[1].value==2);
 Event sound_switch[16]={{0,2,0,10},{0,2,1,20},{100,0,0,0},{200,0,1,0},{1000,6,0,0}};
 assert(all_scratch(sound_switch,16));bool armed_early=false;
 for(const auto& e:sound_switch)if(e.type==2 && e.param==7 && e.value==20){assert(e.time==150);armed_early=true;}
 assert(armed_early);
 Event full[]={{0,0,0,0},{1,6,0,0}},full_copy[2];std::memcpy(full_copy,full,sizeof(full));
 assert(!all_scratch(full,2));assert(!std::memcmp(full,full_copy,sizeof(full)));
 std::cout<<"PASS: all 17 rates; SP/DP notes, holds, BPM, metadata, invalid charts; audio block continuity, EOF and reset\n";
}
