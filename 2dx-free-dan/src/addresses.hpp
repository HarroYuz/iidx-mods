#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include "address_table.generated.hpp"

namespace addresses {
// The legacy RVAs are stable symbol IDs, not addresses in a newer image.
// Default identity mapping supports the synthetic 0909 regression fixtures.
inline bool current = false;
inline int dan_stage_id=56,life_stage_id=63;
inline const Layout& layout(){return layouts[current?1:0];}
inline void use_current(bool value){current=value;dan_stage_id=int(layout().dan_stage);life_stage_id=int(layout().life_stage);}
inline uintptr_t rva(uintptr_t symbol) {
    if (!current || symbol < 0x300000) return symbol;
    size_t lo=0,hi=sizeof(current_rvas)/sizeof(current_rvas[0]);
    while(lo<hi){const auto mid=lo+(hi-lo)/2;
        if(current_rvas[mid].symbol<symbol)lo=mid+1;else hi=mid;}
    return lo<sizeof(current_rvas)/sizeof(current_rvas[0]) &&
        current_rvas[lo].symbol==symbol?current_rvas[lo].value:0;
}
inline int scene_slot(int legacy){return legacy+(legacy>=13?int(layout().scene_slot_shift):0);}
inline int menu_rows(){return int(layout().menu_rows);}
inline size_t menu_count_offset(){return 0x30+size_t(menu_rows())*0x18;}
inline auto menu_count_operands(){return count_operands(current);}
inline bool is_stage(int scene){return scene>=int(layout().stage_first) && scene<=int(layout().stage_last);}
inline const char* version(){return current?"LDJ2026091700-010":"LDJ2026090900-010";}
template<class T> bool read(uintptr_t at,T& out) {
    SIZE_T got{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),
        &out,sizeof(out),&got) && got==sizeof(out);
}
inline bool select(uintptr_t base) {
    IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    if(!read(base,dos) || dos.e_magic!=IMAGE_DOS_SIGNATURE ||
       dos.e_lfanew<64 || dos.e_lfanew>4096 ||
       !read(base+dos.e_lfanew,nt) || nt.Signature!=IMAGE_NT_SIGNATURE ||
       nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64 ||
       nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
    for(int i=0;i<2;++i)if(nt.FileHeader.TimeDateStamp==identities[i].timestamp &&
       nt.OptionalHeader.AddressOfEntryPoint==identities[i].entry &&
       nt.OptionalHeader.SizeOfImage==identities[i].image_size){use_current(i==1);return true;}
    return false;
}
} // namespace addresses
