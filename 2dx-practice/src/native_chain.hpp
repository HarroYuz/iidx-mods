#pragma once
#include <windows.h>
#include <cstdint>
#include <utility>

namespace self_dan {
// Callback slots and direct branches already specify the ABI. Retain their
// current target, regardless of which optional module installed it.
inline bool executable_target(uintptr_t address) {
    MEMORY_BASIC_INFORMATION region{};
    if (!address || !VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) ||
        region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto access = region.Protect & 0xff;
    return access == PAGE_EXECUTE || access == PAGE_EXECUTE_READ ||
           access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
}

// A distinct typed adapter for every call site preserves register/stack
// arguments, return values and site-specific predecessors. The scope is local
// to this thread and nests when a predecessor calls another patched site.
struct ChainScope {
    inline static thread_local ChainScope* active{};
    uintptr_t canonical, downstream;
    ChainScope* parent;
    ChainScope(uintptr_t original, uintptr_t next):
        canonical(original), downstream(next), parent(active) { active=this; }
    ~ChainScope() { active=parent; }
    ChainScope(const ChainScope&)=delete;
    static uintptr_t resolve(uintptr_t original) {
        return active && active->canonical==original ? active->downstream : original;
    }
};
template<uintptr_t Site, class F> struct BranchChain;
template<uintptr_t Site, class R, class... Args>
struct BranchChain<Site, R(*)(Args...)> {
    inline static R(*wrapper)(Args...){};
    inline static uintptr_t canonical{}, downstream{};
    static R invoke(Args... args) {
        ChainScope scope(canonical,downstream);
        return wrapper(std::forward<Args>(args)...);
    }
    static void set_next(uintptr_t next) { downstream=next; }
};
} // namespace self_dan
