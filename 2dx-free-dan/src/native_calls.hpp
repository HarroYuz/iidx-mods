#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <algorithm>

namespace self_dan {
// For verified direct E8/E9 sites in scene callbacks only. Installation/removal
// must run on their owning game thread while all affected callbacks are idle.
// This is not a general concurrent executable-code patcher. The runtime saves
// each site's current destination and dispatches it through a typed adapter.
struct CallHook {
    uint8_t* site{};
    void* expected_target{};
    void* replacement{};
    uint8_t* relay{};
    std::shared_ptr<void> relay_allocation;
    uint8_t before[5]{}, after[5]{};
    DWORD protection{};
    bool prepared{}, installed{}, protection_pending{};
    uint8_t opcode=0xe8; // 0xe9 for a verified tail jump with the same ABI.
};
inline bool relative_call(uint8_t* site,const void* target,uint8_t (&bytes)[5]) {
    const auto delta=reinterpret_cast<int64_t>(target)-(reinterpret_cast<int64_t>(site)+5);
    if(delta<std::numeric_limits<int32_t>::min() || delta>std::numeric_limits<int32_t>::max()) return false;
    bytes[0]=0xe8;
    const int32_t displacement=static_cast<int32_t>(delta);
    std::memcpy(bytes+1,&displacement,4);
    return true;
}
inline bool call_matches(const CallHook& h,const uint8_t (&bytes)[5]) {
    uint8_t current[5]{}; SIZE_T got{};
    return h.site && ReadProcessMemory(GetCurrentProcess(),h.site,current,5,&got) && got==5 &&
        std::memcmp(current,bytes,5)==0;
}
struct RelaySearchStats {size_t queries{}, allocation_attempts{};};
inline uint8_t* allocate_relay_near(uintptr_t site,size_t bytes=4096,
                                  uintptr_t lower=0,uintptr_t upper=UINTPTR_MAX,
                                  RelaySearchStats* stats=nullptr) {
    SYSTEM_INFO info{}; GetSystemInfo(&info);
    const uintptr_t granularity=info.dwAllocationGranularity;
    const uintptr_t center=site&~(granularity-1);
    const auto limit=reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress);
    if(!bytes || bytes>limit)return nullptr;
    const auto minimum=std::max({reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress),lower,
        site>0x7fff0000?site-0x7fff0000:uintptr_t(0)});
    const auto maximum=std::min({limit-bytes+1,upper,site+0x7fff0000});
    if(minimum>maximum)return nullptr;
    uintptr_t cursor[2]{center+granularity,center>=granularity?center-granularity:0};
    bool active[2]{cursor[0]<=maximum,cursor[1]>=minimum};
    while(active[0] || active[1]) {
        for(int direction=0;direction<2;++direction) {
            if(!active[direction])continue;
            const auto at=cursor[direction];
            if(at<minimum || at>maximum){active[direction]=false;continue;}
            MEMORY_BASIC_INFORMATION region{};
            if(stats)++stats->queries;
            if(!VirtualQuery(reinterpret_cast<void*>(at),&region,sizeof(region))){active[direction]=false;continue;}
            const auto end=reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize;
            if(region.State==MEM_FREE && end>=at && bytes<=end-at){
                if(stats)++stats->allocation_attempts;
                auto* page=static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>(at),bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
                if(page)return page;
            }
            // Skip entire occupied regions/reservations instead of probing
            // every 64 KiB inside the same large mapped image 132 times.
            if(direction==0){
                const auto next=region.State==MEM_FREE?at+granularity:(end+granularity-1)&~(granularity-1);
                active[0]=next>at && next<=maximum;cursor[0]=next;
            }else{
                const auto start=region.State==MEM_FREE?at:reinterpret_cast<uintptr_t>(region.AllocationBase);
                const auto aligned=start&~(granularity-1);
                const auto next=aligned>=granularity?aligned-granularity:0;
                active[1]=next<at && next>=minimum;cursor[1]=next;
            }
        }
    }
    return nullptr;
}
inline bool prepare_calls(CallHook* hooks,size_t count,RelaySearchStats* stats=nullptr) {
    if(!count)return true;
    if(!hooks || count>(SIZE_MAX-4095)/16)return false;
    const size_t bytes=(count*16+4095)&~size_t(4095);
    uintptr_t first=UINTPTR_MAX,last=0;
    for(size_t i=0;i<count;++i){
        auto& h=hooks[i];
        if(h.prepared || h.installed || h.protection_pending || h.relay || h.relay_allocation ||
           !h.replacement || !h.expected_target || (h.opcode!=0xe8 && h.opcode!=0xe9) ||
           !relative_call(h.site,h.expected_target,h.before))return false;
        h.before[0]=h.opcode;
        if(!call_matches(h,h.before))return false;
        const auto at=reinterpret_cast<uintptr_t>(h.site);
        first=std::min(first,at);last=std::max(last,at);
    }
    // Every relay in the block must be within signed rel32 reach of every
    // site. The game sites span only a few MiB; do not allocate per hook.
    const auto lower=last+5>0x80000000?last+5-0x80000000:uintptr_t(0);
    if(bytes>first+5+0x7fffffff)return false;
    const auto upper=first+5+0x7fffffff-(bytes-1);
    auto* block=allocate_relay_near(first+(last-first)/2,bytes,lower,upper,stats);
    if(!block)return false;
    std::shared_ptr<void> allocation(block,[](void* p){VirtualFree(p,0,MEM_RELEASE);});
    // jmp qword ptr [rip+0], followed by the target pointer. No argument,
    // volatile register, stack argument or return address is changed.
    const uint8_t jump[]={0xff,0x25,0,0,0,0};
    for(size_t i=0;i<count;++i){
        auto& h=hooks[i];auto* relay=block+i*16;
        std::memcpy(relay,jump,sizeof(jump));
        std::memcpy(relay+6,&h.replacement,sizeof(h.replacement));
        if(!relative_call(h.site,relay,h.after))return false;
        h.after[0]=h.opcode;
    }
    DWORD old{};
    if(!VirtualProtect(block,bytes,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),block,count*16))return false;
    for(size_t i=0;i<count;++i){
        auto& h=hooks[i];h.relay=block+i*16;h.relay_allocation=allocation;h.prepared=true;
    }
    return true;
}
inline bool prepare_call(CallHook& h) {return prepare_calls(&h,1);}
inline bool restore_call(CallHook& h) {
    if(!h.installed && !h.protection_pending) return true;
    const bool ours=call_matches(h,h.after), original=call_matches(h,h.before);
    // Do not overwrite a hook installed later by another component.
    if(!ours && !original) return false;
    DWORD current{},ignored{};
    if(!VirtualProtect(h.site,5,PAGE_EXECUTE_READWRITE,&current)) return false;
    if(ours) std::memcpy(h.site,h.before,5);
    h.installed=false;
    const bool flushed=FlushInstructionCache(GetCurrentProcess(),h.site,5)!=FALSE;
    h.protection_pending=!VirtualProtect(h.site,5,h.protection,&ignored);
    return flushed && !h.protection_pending;
}
inline bool restore_calls(CallHook* hooks,size_t count) {
    bool ok=true;
    while(count) if(!restore_call(hooks[--count])) ok=false;
    return ok;
}
inline bool install_calls(CallHook* hooks,size_t count) {
    for(size_t i=0;i<count;++i) {
        auto& h=hooks[i];
        if(!h.prepared || h.installed || h.protection_pending || !call_matches(h,h.before)) return false;
        for(size_t j=0;j<i;++j) {
            const auto a=reinterpret_cast<uintptr_t>(h.site),b=reinterpret_cast<uintptr_t>(hooks[j].site);
            if(a<b+5 && b<a+5) return false;
        }
    }
    for(size_t i=0;i<count;++i) {
        auto& h=hooks[i]; DWORD ignored{};
        if(!VirtualProtect(h.site,5,PAGE_EXECUTE_READWRITE,&h.protection)) {restore_calls(hooks,i);return false;}
        std::memcpy(h.site,h.after,5);h.installed=true;
        const bool flushed=FlushInstructionCache(GetCurrentProcess(),h.site,5)!=FALSE;
        h.protection_pending=!VirtualProtect(h.site,5,h.protection,&ignored);
        if(!flushed || h.protection_pending) {restore_calls(hooks,i+1);return false;}
    }
    return true;
}
// Release only before publication, or after the owning thread has stopped all
// callbacks and restore_calls succeeded. Never free a potentially live relay.
inline bool release_call(CallHook& h) {
    if(h.installed || h.protection_pending) return false;
    h.relay_allocation.reset();
    h.relay=nullptr;h.prepared=false;return true;
}
} // namespace self_dan
