#pragma once
#include <windows.h>
#include <cstddef>

namespace self_dan {
struct SlotHook {
    void** slot;
    void* expected;
    void* replacement;
    DWORD protection = 0;
    bool installed = false;
    bool protection_pending = false;
};
enum class HookStatus { installed, conflict, protect_failed, restore_failed, rollback_failed };

inline bool remove_hooks(SlotHook* hooks, size_t count) {
    bool success = true;
    // Reverse order also handles multiple slots on one page.
    while (count) {
        auto& h = hooks[--count];
        if (!h.installed && !h.protection_pending) continue;
        DWORD current = 0;
        if (!VirtualProtect(h.slot, sizeof(void*), PAGE_READWRITE, &current)) {
            success = false;
            continue;
        }
        // A subsequent mod's replacement is not ours to overwrite.
        if (h.installed) {
            auto previous = InterlockedCompareExchangePointer(h.slot, h.expected, h.replacement);
            if (previous == h.replacement || previous == h.expected) h.installed = false;
            else success = false;
        }
        DWORD ignored = 0;
        h.protection_pending = !VirtualProtect(h.slot, sizeof(void*), h.protection, &ignored);
        if (h.protection_pending) success = false;
    }
    return success;
}

inline HookStatus install_hooks(SlotHook* hooks, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        auto& h = hooks[i];
        void* current = nullptr;
        SIZE_T got = 0;
        if (h.installed || h.protection_pending || !h.slot || !h.replacement ||
            !ReadProcessMemory(GetCurrentProcess(), h.slot, &current, sizeof(current), &got) ||
            got != sizeof(current) || current != h.expected) return HookStatus::conflict;
        for (size_t j = 0; j < i; ++j) if (h.slot == hooks[j].slot) return HookStatus::conflict;
    }
    HookStatus failure = HookStatus::installed;
    for (size_t i = 0; i < count; ++i) {
        auto& h = hooks[i];
        if (!VirtualProtect(h.slot, sizeof(void*), PAGE_READWRITE, &h.protection)) {
            failure = HookStatus::protect_failed;
            break;
        }
        h.installed = InterlockedCompareExchangePointer(h.slot, h.replacement, h.expected) == h.expected;
        DWORD ignored = 0;
        const bool restored = VirtualProtect(h.slot, sizeof(void*), h.protection, &ignored) != FALSE;
        h.protection_pending = !restored;
        if (!h.installed || !restored) {
            failure = restored ? HookStatus::conflict : HookStatus::restore_failed;
            break;
        }
    }
    if (failure != HookStatus::installed && !remove_hooks(hooks, count)) return HookStatus::rollback_failed;
    return failure;
}
} // namespace self_dan
