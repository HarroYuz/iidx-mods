#pragma once
#include "native_layout.hpp"
#include <limits>

namespace self_dan::native {
struct Selection {
    uintptr_t manager{}, list{}, group{}, category{}, chart{}, music{};
    int last_opened=-1, bank=-1, style=-1, group_key=-1, category_cursor=-1;
    int id=-1, count{}, song_count{}, cursor=-1, entry_type=-1, folder_id=-1;
    int music_id{}, difficulty[2]{-1,-1}, level[2]{-1,-1};
    bool opened{}, category_valid{}, preview_valid{};
};

template<class Read> bool vector_at(Read& read,uintptr_t vector,size_t width,
                                  size_t limit,size_t index,uintptr_t& address) {
    uintptr_t begin{},end{},capacity{};
    if(!width || !read(vector,begin) || !read(vector+8,end) || !read(vector+16,capacity) ||
       !begin || begin%width || end<begin || capacity<end ||
       (end-begin)%width || (capacity-begin)%width ||
       (capacity-begin)/width>limit || index>=(end-begin)/width) return false;
    address=begin+index*width;
    return true;
}

// Read the raw visible entry for either selector; never resolve random headers.
template<class Read> Selection read_category_selection(Selection s,Read&& read,int single_side=-1) {
    if(!s.category) return s;
    uint8_t opened{};
    if(!read(s.category,s.id) || !read(s.category+4,opened) ||
       !read(s.category+8,s.count) || !read(s.category+0x10,s.song_count) ||
       !read(s.category+0x14,s.cursor) || s.count<0 || s.count>100000 ||
       s.song_count<0 || s.song_count>100000) return s;
    s.opened=opened!=0;
    s.category_valid=true;
    uintptr_t entry{};
    if(s.cursor<0 || s.cursor>=s.count ||
       !vector_at(read,s.category+0x20,16,100000,s.cursor,entry) ||
       !read(entry,s.entry_type) || !read(entry+8,s.chart)) return s;
    if(s.entry_type==0 && s.chart) read(s.chart,s.folder_id);
    // Never call 5fb180: on a folder header it can choose a random fallback.
    if(s.entry_type!=1 || !s.chart || !read(s.chart,s.music) || !s.music ||
       !read(s.chart+8,s.difficulty) || !read(s.music+music_id_offset,s.music_id) || s.music_id<=0) return s;
    int8_t levels[10]{};
    if(!read(s.music+music_levels_offset,levels)) return s;
    // MUSIC_SELECT's native single-player assignment copies the active side's
    // difficulty to BOTH slots (8eb9cc..8eb9fa). Mirror that before validation.
    if(single_side==0 || single_side==1)s.difficulty[1-single_side]=s.difficulty[single_side];
    for(int side=0;side<2;++side) {
        const int slot=chart_slot(s.style,s.difficulty[side]);
        if(slot<0) return s;
        s.level[side]=levels[slot];
    }
    s.preview_valid=true;
    return s;
}

// This follows the cursor's group. manager+2a420 is only the last OPENED
// category and can differ from the visible folder by multiple frames.
// Read must return false (and not dereference) for unavailable memory.
template<class Read> Selection read_stepup_selection(uintptr_t base,Read&& read,uintptr_t expected_group=0xcb5040) {
    Selection s;
    uintptr_t group_id{},head{},node{};
    int group_cursor=-1;
    if(!read(base+addresses::rva(category_manager),s.manager) || !s.manager ||
       !read(s.manager+addresses::layout().category_opened,s.last_opened) || !read(s.manager+addresses::layout().category_bank,s.bank) ||
       !read(s.manager+addresses::layout().category_list,s.list) || !s.list ||
       !read(s.list+0x78,s.style) || s.style<0 || s.style>1 ||
       !read(s.list+0x7c,group_cursor) || group_cursor<0 ||
       !vector_at(read,s.list+0x48+s.style*24,4,256,group_cursor,group_id) ||
       !read(group_id,s.group_key) || !read(s.list+0x10,head) || !head || !read(head,node)) return s;
    for(int i=0;i<64 && node && node!=head;++i) {
        int key=-1;
        if(!read(node+0x10,key)) return s;
        if(key==s.group_key) {
            uintptr_t vt{},category_slot{};
            if(!read(node+0x18,s.group) || !s.group || !read(s.group,vt) || vt!=base+addresses::rva(expected_group) ||
               !read(s.group+0xa8+s.style*4,s.category_cursor) || s.category_cursor<0 ||
               !vector_at(read,s.group+0x78+(s.bank==1?24:0),8,256,s.category_cursor,category_slot) ||
               !read(category_slot,s.category) || !s.category) return s;
            break;
        }
        uintptr_t next{};
        if(!read(node,next) || next==node) return s;
        node=next;
    }
    return read_category_selection(s,read);
}

} // namespace self_dan::native
