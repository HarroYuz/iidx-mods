#pragma once
#include <cstdint>
#include <cstring>
#include <vector>

namespace practice_network {
struct Entry {int32_t id;char method[64];int32_t flags;};
static_assert(sizeof(Entry)==0x48);
// Method names are stable across supported versions; numeric IDs are not.
inline constexpr const char* writes[]={"pc.save","pc.save2pp","pc.eaappliresult",
 "pc.setBestresult","music.reg","music.play","music.nosave","music.sc","grade.raised"};
inline const char* method(const Entry& e){
 if(!std::memchr(e.method,0,sizeof(e.method)) || !e.method[0])return nullptr;
 if(std::strncmp(e.method,"IIDX",4))return e.method;
 if(std::strlen(e.method)<7 || e.method[4]<'0' || e.method[4]>'9' ||
    e.method[5]<'0' || e.method[5]>'9')return nullptr;
 return e.method+6;
}
inline bool is_write(const Entry& e){
 const char* name=method(e);if(!name)return false;
 for(const char* write:writes)if(!std::strcmp(name,write))return true;
 return false;
}
inline bool valid(const std::vector<Entry>& entries){
 if(entries.empty() || entries.size()>128)return false;
 for(size_t i=0;i<entries.size();++i){
  if(entries[i].id<0 || !method(entries[i]))return false;
  for(size_t j=0;j<i;++j)if(entries[i].id==entries[j].id ||
      !std::strcmp(entries[i].method,entries[j].method))return false;
 }
 for(const char* required:writes){
  bool found=false;
  for(const auto& e:entries)if(!std::strcmp(method(e),required))found=true;
  if(!found)return false;
 }
 return true;
}
inline const Entry* find(const std::vector<Entry>& entries,int id){
 for(const auto& e:entries)if(e.id==id)return &e;
 return nullptr;
}
} // namespace practice_network
