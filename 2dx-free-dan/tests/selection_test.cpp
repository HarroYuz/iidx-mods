#include "../src/native_selection.hpp"
#include <unordered_map>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace n=self_dan::native;
static void check(bool ok,const char* message) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
struct Memory {
    std::unordered_map<uintptr_t,uint8_t> bytes;
    template<class T> void put(uintptr_t at,const T& value) {
        const auto* p=reinterpret_cast<const uint8_t*>(&value);
        for(size_t i=0;i<sizeof(T);++i)bytes[at+i]=p[i];
    }
    template<class T> bool operator()(uintptr_t at,T& value) const {
        auto* p=reinterpret_cast<uint8_t*>(&value);
        for(size_t i=0;i<sizeof(T);++i) {
            auto it=bytes.find(at+i);
            if(it==bytes.end()){std::memset(&value,0,sizeof(value));return false;}
            p[i]=it->second;
        }
        return true;
    }
    void vector(uintptr_t at,uintptr_t begin,size_t bytes) {
        put(at,begin);put(at+8,begin+bytes);put(at+16,begin+bytes);
    }
};
int main() {
    constexpr uintptr_t base=0x180000000,manager=0x200000,list=0x300000,head=0x400000,
        node=0x400100,group=0x500000,ids=0x600000,categories=0x600100,category=0x700000,
        entries=0x800000,chart=0x900000,music=0xa00000;
    Memory m;
    m.put(base+n::category_manager,manager);m.put(manager+0x2a420,162);m.put(manager+0x2a424,0);
    m.put(manager+0x175d0,list);m.put(list+0x78,1);m.put(list+0x7c,0);
    m.vector(list+0x60,ids,4);m.put(ids,7);m.put(list+0x10,head);m.put(head,node);
    m.put(node,head);m.put(node+0x10,7);m.put(node+0x18,group);m.put(group,base+0xcb5040);
    m.put(group+0xac,0);m.vector(group+0x78,categories,8);m.put(categories,category);
    m.put(category,196);m.put(category+4,uint8_t(1));m.put(category+8,3);
    m.put(category+0x10,4);m.put(category+0x14,2);m.vector(category+0x20,entries,48);
    m.put(entries+32,1);m.put(entries+40,chart);m.put(chart,music);
    const int difficulties[2]={3,3};m.put(chart+8,difficulties);
    m.put(music+n::music_id_offset,25007);
    const int8_t levels[10]={0,4,7,10,0,0,7,10,12,0};m.put(music+n::music_levels_offset,levels);
    auto s=n::read_stepup_selection(base,m);
    check(s.category_valid && s.preview_valid && s.style==1 && s.group_key==7,"DP group path");
    check(s.id==196 && s.last_opened==162,"cursor differs from last opened category");
    check(s.music_id==25007 && s.level[0]==12 && s.level[1]==12,"DP slot 8, not SP slot 3");
    const int unavailable[2]={3,5};m.put(chart+8,unavailable);
    n::Selection direct;direct.category=category;direct.style=1;
    auto single=n::read_category_selection(direct,m,0);
    check(single.preview_valid && single.difficulty[1]==3 && single.level[1]==12,"single-player mirrors active chart into unused native slot");
    check(!n::read_category_selection(direct,m).preview_valid,"two-player missing chart still rejected");
    m.put(chart+8,difficulties);
    m.put(entries+32,2);s=n::read_stepup_selection(base,m);
    check(s.category_valid && !s.preview_valid && s.music==0,"random row never resolves a fallback chart");
    m.put(entries+32,0);m.put(entries+40,category);s=n::read_stepup_selection(base,m);
    check(s.folder_id==196 && !s.preview_valid,"folder header has no chart");
    m.put(category+0x14,3);s=n::read_stepup_selection(base,m);
    check(s.category_valid && s.entry_type==-1,"cursor at end is rejected");
    m.put(category+0x14,2);m.put(category+0x30,entries+16);s=n::read_stepup_selection(base,m);
    check(s.category_valid && s.entry_type==-1,"corrupt vector capacity rejected");
    m.put(manager+0x2a424,1);s=n::read_stepup_selection(base,m);
    check(!s.category_valid,"missing second bank never falls back to first");
    m.vector(group+0x90,categories,8);s=n::read_stepup_selection(base,m);
    check(s.category_valid && s.bank==1,"second bank path");
    m.put(group,base+0xcb5000);s=n::read_stepup_selection(base,m);
    check(!s.category_valid,"unverified group layout rejected");
    m.put(node+0x10,9);m.put(node,node);s=n::read_stepup_selection(base,m);
    check(!s.category_valid,"malformed map terminates");
    Memory empty;s=n::read_stepup_selection(base,empty);
    check(!s.category_valid && !s.preview_valid,"unavailable memory rejected");
    std::puts("PASS: selection cursor, DP levels, both banks and malformed memory");
}
