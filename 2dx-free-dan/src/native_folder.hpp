#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>

namespace self_dan::native {
struct Vector {uintptr_t begin{},end{},capacity{};};
template<class T> bool small_vector_valid(const Vector& v) {
    return v.end>=v.begin && v.capacity>=v.end &&
        (v.capacity-v.begin)<4096 && (v.end-v.begin)%sizeof(T)==0 &&
        (v.capacity-v.begin)%sizeof(T)==0 &&
        ((!v.begin && !v.end && !v.capacity) || (v.begin && v.begin%alignof(T)==0));
}
struct FolderDescriptor {
    int kind=9,sort=2;
    uint8_t flags[8]{0,1};
    // Native descriptor layers: selected heading, folder title, list overlay,
    // then list background. 5f3900 draws the overlay at (30,7) over the bar.
    // This AFP node is a short heading (native ss_examination_mosi is 64px
    // high), not the large central title. Keep it distinct from the list label.
    const char* selected="ss_self_dan";
    const char* folder="ssm_self_dan";
    const char* auxiliary="so_self_dan";
    const char* label="so_b_all";
    const char* help="CUSTOM DAN: FOUR SONGS, SAME OR HIGHER LEVEL";
    int capacity=0,padding=0;
};
static_assert(sizeof(FolderDescriptor)==0x40);
struct InlineString {char data[16]{};size_t size=0,capacity=15;};
struct FolderHeader {int id{},reserved[3]{};InlineString text;};
struct Category {
    int id{};uint8_t opened{},padding[3]{};
    int count=1,available=0,songs=0,cursor=0,difficulty[2]{};
    Vector entries;
    FolderHeader header;
    int auxiliary_id{},reserved[3]{};
    Vector charts;
    uint8_t unknown[16]{},event[48]{};
};
struct Entry {int type{},reserved{};uintptr_t data{};};
static_assert(sizeof(Category)==0xd0);
static_assert(offsetof(Category,entries)==0x20 && offsetof(Category,header)==0x38);
static_assert(offsetof(Category,charts)==0x78 && offsetof(Category,event)==0xa0);
static_assert(sizeof(Entry)==16);

// Only small native vectors (<4096 bytes). Larger MSVC allocations have a
// different aligned-allocation header and are deliberately rejected here.
template<class T> struct SmallAppend {
    Vector* vector{};
    Vector previous{};
    T value{};
    T* allocation{};
    bool existing=false,committed=false;
    template<class Allocate> bool prepare(Vector& v,T item,Allocate&& allocate) {
        const auto bytes=v.end-v.begin,capacity_bytes=v.capacity-v.begin;
        if(!small_vector_valid<T>(v))return false;
        vector=&v;previous=v;value=item;
        auto* data=reinterpret_cast<T*>(v.begin);
        const size_t count=bytes/sizeof(T),capacity=capacity_bytes/sizeof(T);
        for(size_t i=0;i<count;++i)if(data[i]==item){existing=true;return true;}
        if(count<capacity)return true;
        const size_t grown=std::max(count+1,capacity+std::max(size_t(1),capacity/2));
        if(grown*sizeof(T)>=4096)return false;
        allocation=static_cast<T*>(allocate(grown*sizeof(T)));
        if(!allocation)return false;
        if(bytes)std::memcpy(allocation,data,bytes);
        allocation[count]=item;
        new_capacity_=grown;
        return true;
    }
    bool unchanged()const {return vector && std::memcmp(vector,&previous,sizeof(previous))==0;}
    void commit() {
        if(existing)return;
        if(allocation) {
            const size_t count=(previous.end-previous.begin)/sizeof(T);
            *vector={reinterpret_cast<uintptr_t>(allocation),reinterpret_cast<uintptr_t>(allocation+count+1),
                     reinterpret_cast<uintptr_t>(allocation+new_capacity_)};
        }else {
            *reinterpret_cast<T*>(vector->end)=value;vector->end+=sizeof(T);
        }
        committed=true;
    }
    template<class Free> void finish(Free&& free) {
        if(allocation) {
            if(committed){if(previous.begin)free(reinterpret_cast<void*>(previous.begin));}
            else free(allocation);
        }
        allocation=nullptr;
    }
private:size_t new_capacity_{};
};
template<class T> bool erase_item(Vector& v,T item) {
    if(!small_vector_valid<T>(v))return false;
    auto* data=reinterpret_cast<T*>(v.begin);size_t count=(v.end-v.begin)/sizeof(T),out=0;
    for(size_t i=0;i<count;++i)if(data[i]!=item)data[out++]=data[i];
    v.end=v.begin+out*sizeof(T);return true;
}

class EntryFolder {
public:
    static constexpr int id=0xe7; // e6 is used by omnifix's additional category.
    EntryFolder() {
        for(int bank=0;bank<2;++bank) {
            auto& c=categories_[bank];c.id=id;c.header.id=id;c.auxiliary_id=id;
            entries_[bank].data=reinterpret_cast<uintptr_t>(&c.header);
            c.entries={reinterpret_cast<uintptr_t>(&entries_[bank]),
                reinterpret_cast<uintptr_t>(&entries_[bank]+1),reinterpret_cast<uintptr_t>(&entries_[bank]+1)};
        }
    }
    EntryFolder(const EntryFolder&)=delete;
    EntryFolder& operator=(const EntryFolder&)=delete;
    Category* category(int bank){return &categories_[bank==1?1:0];}
    const FolderDescriptor* descriptor()const{return &descriptor_;}
    bool owns(uintptr_t p)const {return p==reinterpret_cast<uintptr_t>(&categories_[0]) || p==reinterpret_cast<uintptr_t>(&categories_[1]);}
    template<class Allocate,class Free> bool attach(uintptr_t group,Allocate&& allocate,Free&& free) {
        if(!group)return false;
        auto& ids=*reinterpret_cast<Vector*>(group+0x10);
        auto& bank0=*reinterpret_cast<Vector*>(group+0x78);
        auto& bank1=*reinterpret_cast<Vector*>(group+0x90);
        if(attached_group_ && attached_group_!=group)return false;
        if(!small_vector_valid<int>(ids))return false;
        // A bare ID can belong to an empty foreign category, absent from both
        // visible banks. Only accept it when this instance registered it.
        for(auto p=ids.begin;p<ids.end;p+=4)
            if(*reinterpret_cast<const int*>(p)==id && attached_group_!=group)return false;
        // An unrelated mod already using our ID is a conflict, not permission
        // to replace its category. Validate both banks before allocating.
        for(auto* bank:{&bank0,&bank1}) {
            if(!small_vector_valid<uintptr_t>(*bank))return false;
            for(auto p=bank->begin;p<bank->end;p+=8) {
                const auto cat=*reinterpret_cast<uintptr_t*>(p);
                if(cat && *reinterpret_cast<const int*>(cat)==id && !owns(cat))return false;
            }
        }
        SmallAppend<int> id_edit;
        SmallAppend<uintptr_t> a,b;
        const bool ready=id_edit.prepare(ids,id,allocate) &&
            a.prepare(bank0,reinterpret_cast<uintptr_t>(&categories_[0]),allocate) &&
            b.prepare(bank1,reinterpret_cast<uintptr_t>(&categories_[1]),allocate);
        const bool ok=ready && id_edit.unchanged() && a.unchanged() && b.unchanged();
        if(ok){id_edit.commit();a.commit();b.commit();attached_group_=group;}
        id_edit.finish(free);a.finish(free);b.finish(free);
        return ok;
    }
    bool detach(uintptr_t group) {
        if(!group || attached_group_!=group)return false;
        auto& ids=*reinterpret_cast<Vector*>(group+0x10);
        if(!small_vector_valid<int>(ids))return false;
        for(int bank=0;bank<2;++bank)
            if(!small_vector_valid<uintptr_t>(*reinterpret_cast<Vector*>(group+0x78+bank*24)))return false;
        for(int bank=0;bank<2;++bank) {
            auto& v=*reinterpret_cast<Vector*>(group+0x78+bank*24);
            erase_item(v,reinterpret_cast<uintptr_t>(&categories_[bank]));
            auto& cursor=*reinterpret_cast<int*>(group+0xa8+bank*4);
            const auto count=(v.end-v.begin)/8;
            if(cursor>=0 && size_t(cursor)>=count)cursor=count?int(count)-1:0;
            categories_[bank].opened=0;
        }
        erase_item(ids,id);attached_group_=0;return true;
    }
private:
    FolderDescriptor descriptor_;
    Category categories_[2];
    Entry entries_[2];
    uintptr_t attached_group_{};
};
} // namespace self_dan::native
