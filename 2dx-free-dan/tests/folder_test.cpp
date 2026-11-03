#include "../src/native_folder.hpp"
#include <cstdlib>
#include <cstdio>
#include <unordered_set>
namespace n=self_dan::native;
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
struct Heap {
    int calls{},fail=-1;std::unordered_set<void*> live;
    void* alloc(size_t bytes){if(calls++==fail)return nullptr;auto p=std::malloc(bytes);live.insert(p);return p;}
    void free(void* p){check(live.erase(p)==1,"free only owned native allocation");std::free(p);}
    ~Heap(){for(auto p:live)std::free(p);}
};
struct Group {
    alignas(8) unsigned char data[0xb0]{};
    uintptr_t ptr(){return reinterpret_cast<uintptr_t>(data);}
    n::Vector& vec(int offset){return *reinterpret_cast<n::Vector*>(data+offset);}
};
int main(){
    for(int fail=0;fail<3;++fail){
        Heap h;Group g;n::EntryFolder f;h.fail=fail;
        check(!f.attach(g.ptr(),[&](size_t b){return h.alloc(b);},[&](void* p){h.free(p);}),"allocation failure rejects entire registration");
        check(!g.vec(0x10).begin && !g.vec(0x78).begin && !g.vec(0x90).begin && h.live.empty(),"no partial vectors or allocation leaks");
    }
    Heap h;Group g;n::EntryFolder f;
    auto attach=[&]{return f.attach(g.ptr(),[&](size_t b){return h.alloc(b);},[&](void* p){h.free(p);});};
    check(attach() && h.live.size()==3,"three native allocations");
    check(attach() && h.calls==3,"repeated attach does not duplicate or allocate");
    check(f.category(0)!=f.category(1) && f.category(0)->entries.begin!=f.category(1)->entries.begin,"banks own separate headers");
    check(f.category(0)->songs==0 && f.category(0)->count==1,"entry folder contains only a header");
    // Population rebuild clears pointer banks but leaves the group ID list.
    g.vec(0x78).end=g.vec(0x78).begin;g.vec(0x90).end=g.vec(0x90).begin;
    check(attach() && h.calls==3,"reappend after native population without reallocating");
    n::EntryFolder foreign;
    check(!foreign.attach(g.ptr(),[&](size_t b){return h.alloc(b);},[&](void* p){h.free(p);}),"foreign instance cannot share registered ID");
    Group other;check(!f.detach(other.ptr()),"never remove foreign group's IDs");
    check(f.detach(g.ptr()) && g.vec(0x10).end==g.vec(0x10).begin && g.vec(0x78).end==g.vec(0x78).begin,"detach removes own entries");
    check(h.live.size()==3,"native vectors stay allocated for native teardown");
    // ID collision without a visible category is still a conflict.
    *reinterpret_cast<int*>(g.vec(0x10).begin)=n::EntryFolder::id;g.vec(0x10).end+=4;
    check(!attach(),"bare foreign ID rejected");
    g.vec(0x10).end=g.vec(0x10).begin;
    check(attach(),"reattach after clean detach");
    const auto good=g.vec(0x90);g.vec(0x90).capacity=g.vec(0x90).begin-8;
    check(!f.detach(g.ptr()) && g.vec(0x78).end>g.vec(0x78).begin,"corrupt detach cannot partially erase");
    g.vec(0x90)=good;check(f.detach(g.ptr()),"recover after invalid vector");
    n::Vector bad{0,8,8};check(!n::small_vector_valid<uintptr_t>(bad),"null storage rejected");
    // Growth copies existing native entries and frees old native allocation.
    n::Vector v{};n::SmallAppend<int> first;
    check(first.prepare(v,7,[&](size_t b){return h.alloc(b);}),"prepare first");first.commit();first.finish([&](void* p){h.free(p);});
    n::SmallAppend<int> second;check(second.prepare(v,8,[&](size_t b){return h.alloc(b);}),"prepare growth");
    second.commit();second.finish([&](void* p){h.free(p);});
    check(reinterpret_cast<int*>(v.begin)[0]==7 && reinterpret_cast<int*>(v.begin)[1]==8,"growth preserves native order");
    std::puts("PASS: folder allocation rollback, bank rebuild, ID conflicts and ownership");
}
