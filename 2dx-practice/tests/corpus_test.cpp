#include "../src/core.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <array>
#include <map>
using Signature=std::vector<std::array<int,3>>;
Signature sounds(const rate_mod::Event* events,size_t capacity,bool scratch){
 int bindings[2][8]{};Signature result;std::map<std::pair<int,int>,bool> scratch_onsets;
 for(size_t i=0;i<capacity;++i){auto e=events[i];if(e.type==6 || e.time==INT32_MAX)break;
  if((e.type==2 || e.type==3) && e.param<8)bindings[e.type-2][e.param]=e.value;
  if(e.type<2 && e.param<8){
   result.push_back({e.time,e.type,bindings[e.type][e.param]});
   if(scratch){
    if(e.value)throw std::runtime_error("ALL SCRATCH retained a hold");
    auto key=std::make_pair(e.time,e.type);auto [it,inserted]=scratch_onsets.emplace(key,false);
    (void)inserted;
    if(e.param==7){if(it->second)throw std::runtime_error("duplicate scratch onset");it->second=true;}
   }
  }
 }
 for(auto [key,found]:scratch_onsets){(void)key;if(!found)throw std::runtime_error("missing scratch onset");}
 std::sort(result.begin(),result.end());return result;
}
int main(){
 size_t charts=0,files=0,dp=0;
 for(auto& file:std::filesystem::recursive_directory_iterator("data_mods")){
  if(!file.is_regular_file() || file.path().extension()!=".1")continue;
  std::ifstream input(file.path(),std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
  if(bytes.size()<96)continue;
  ++files;
  for(size_t slot=0;slot<12;++slot){
   uint32_t offset{},length{};std::memcpy(&offset,bytes.data()+slot*8,4);std::memcpy(&length,bytes.data()+slot*8+4,4);
   if(!length)continue;
   if(offset<96 || uint64_t(offset)+length>bytes.size() || length%8 || length>0x18000){std::cerr<<"Invalid chart header "<<file.path()<<" slot "<<slot<<"\n";return 1;}
   ++charts;if(slot>=6)++dp;
   std::vector<rate_mod::Event> scratch(0x18000/8,{INT32_MAX,0,0,0});std::memcpy(scratch.data(),bytes.data()+offset,length);
   if(!rate_mod::all_scratch(scratch.data(),scratch.size())){std::cerr<<"Cannot ALL SCRATCH "<<file.path()<<" slot "<<slot<<"\n";return 4;}
   std::vector<rate_mod::Event> original(length/8);std::memcpy(original.data(),bytes.data()+offset,length);
   if(sounds(original.data(),original.size(),false)!=sounds(scratch.data(),scratch.size(),true)){
    std::cerr<<"Changed note time/side/sound "<<file.path()<<" slot "<<slot<<"\n";return 6;
   }
   for(int p=70;p<=150;p+=5){auto scaled=scratch;if(!rate_mod::transform(scaled.data(),scaled.size(),p))return 5;}

   for(int rate=70;rate<=150;rate+=5){std::vector<rate_mod::Event> events(length/8);std::memcpy(events.data(),bytes.data()+offset,length);
    if(!rate_mod::transform(events.data(),events.size(),rate)){std::cerr<<"Cannot transform "<<file.path()<<" slot "<<slot<<" rate "<<rate<<"\n";return 2;}
   }
  }
 }
 if(!charts)return 3;
 std::cout<<"PASS: "<<files<<" real .1 files, "<<charts<<" charts ("<<dp<<" DP slots), ALL SCRATCH and normal charts at all 17 speeds\n";
}
