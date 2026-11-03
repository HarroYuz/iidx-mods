#include "native_dan.hpp"
#include <cstring>

namespace self_dan {
namespace n=native;
static uintptr_t address(void* p){return reinterpret_cast<uintptr_t>(p);}
template<class T> static T& field(uintptr_t base,uintptr_t rva){return *reinterpret_cast<T*>(base+rva);}

uint64_t NativeDan::initialize(void* flow,void* parameter,n::Init original) {
    const bool pending=session_.phase()==NativeSession::Phase::entry_pending;
    const auto result=original(flow,parameter);
    if(pending && session_.begin(address(flow))) {
        // Native Dan reset has already initialized the rows and stage index.
        // Keep a valid internal template index; title and save paths are scoped
        // separately. Never use a negative index in native asset arrays.
        field<int>(base_,addresses::rva(n::dan_state))=0;
        auto& carry=field<std::array<int,2>>(base_,addresses::rva(0xb287480));
        carry=session_.health_raw();
        fault_=false;
    }
    return result;
}
int NativeDan::first(void* flow,int (*original)(void*)) {
    return session_.owns(address(flow))?n::music_select:original(flow);
}
void NativeDan::prepare(void* flow,int next,n::Prepare original) {
    if(!session_.owns(address(flow))){original(flow,next);return;}
    if(next==n::music_select)return;
    const int index=field<int>(base_,addresses::rva(n::dan_state)+4);
    if(next==n::dan_stage) {
        if(field<n::Settings>(base_,addresses::rva(n::settings)).mode!=3 || !session_.begin_stage(index)) {
            // 967550 re-reads +54 after prepare; a bare return would still
            // launch the previous chart. Route to the scoped result instead.
            fault_=true;
            field<int>(address(flow),addresses::layout().flow_next)=n::dan_result;
            original(flow,n::dan_result);
            return;
        }
        // 90e050 -> 9740d0 reads this exact native carry buffer in mode 3.
        // In particular, preserve 2923 instead of the displayed 58 percent.
        field<std::array<int,2>>(base_,addresses::rva(0xb287480))=session_.health_raw();
    }
    original(flow,next);
    if(next==n::dan_stage && field<int>(address(flow),addresses::layout().flow_next)==n::dan_select) {
        // The native loader retries through DAN_SELECT when a chart cannot be
        // loaded. Never expose an official course selector inside this session.
        fault_=true;field<int>(address(flow),addresses::layout().flow_next)=n::dan_result;
        original(flow,n::dan_result);return;
    }
    if(next==n::stage_result) {
        const auto& rows=field<std::array<n::DanRow,8>>(base_,addresses::rva(n::dan_records));
        const auto& gauges=field<std::array<n::Gauge,2>>(base_,addresses::rva(n::gauges));
        if(!session_.capture_rows(index,rows,{gauges[0].raw,gauges[1].raw})) fault_=true;
    }
}
int NativeDan::next(void* flow,int previous,n::Next original) {
    if(!session_.owns(address(flow)))return original(flow,previous);
    if(fault_) {
        if(previous==n::dan_result)original(flow,previous);
        return 0x7fffffff;
    }
    if(previous==n::music_select) {
        if(session_.phase()!=NativeSession::Phase::selected){fault_=true;return 0x7fffffff;}
        // Preserve the native post-selection flag handling. Original next has
        // no MUSIC_SELECT arm; DAN_SELECT is its native confirmation input.
        return original(flow,n::dan_select);
    }
    const int chosen=original(flow,previous);
    return session_.after_native_next(previous,chosen,field<int>(base_,addresses::rva(n::dan_state)+4));
}
n::GradeManagerPrefix* NativeDan::grade_manager() const {
    return reinterpret_cast<n::GradeManagerPrefix*(*)()>(base_+addresses::rva(0x882200))();
}
bool NativeDan::custom_result() const {return session_.suppress_grade_save() && result_depth_!=0;}
uint64_t NativeDan::result_initialize(void* scene,void* parameter,n::Init original) {
    if(!session_.active())return original(scene,parameter);
    result_scene_=scene;
    n::GradeResultScope scope(grade_manager(),true);
    struct Depth {unsigned& depth;explicit Depth(unsigned& d):depth(d){++depth;}~Depth(){--depth;}} depth(result_depth_);
    return original(scene,parameter);
}
void NativeDan::result_update(void* scene,bool resumed,void* parameter,n::Update original) {
    if(!session_.active() || scene!=result_scene_){original(scene,resumed,parameter);return;}
    n::GradeResultScope scope(grade_manager(),true);
    struct Depth {unsigned& depth;explicit Depth(unsigned& d):depth(d){++depth;}~Depth(){--depth;}} depth(result_depth_);
    original(scene,resumed,parameter);
}
void NativeDan::result_render(void* scene,void (*original)(void*)) {
    // Vtable slot 24 (8c75e0), separate from the slot 15 update callback.
    if(!session_.active() || scene!=result_scene_){original(scene);return;}
    n::GradeResultScope scope(grade_manager(),true);
    struct Depth {unsigned& depth;explicit Depth(unsigned& d):depth(d){++depth;}~Depth(){--depth;}} depth(result_depth_);
    original(scene);
}
int NativeDan::result_account(int side,int (*original)(int)) {
    return n::account_for_result_save(custom_result(),side,original);
}
bool NativeDan::result_joined(int side,bool (*original)(int)) {
    // 8c78fa begins each side's result-song rendering loop. Capture its side
    // before the native course difficulty API discards that argument.
    if(custom_result())result_side_=side;
    return original(side);
}
int NativeDan::course_music(void* manager,int grade,int stage,int style,int (*original)(void*,int,int,int)) {
    if(!session_.active())return original(manager,grade,stage,style);
    const auto id=session_.music_id(stage,style);
    if(id)return *id;
    // Early failure: unselected rows have no song. Native 950bc0(-1) returns
    // null before reading the DB; 82f2a0 explicitly skips null music pointers.
    // Do not fall back to a song from the internal template grade.
    if(!custom_result())fault_=true;
    return -1;
}
int NativeDan::course_difficulty(void* manager,int grade,int stage,int style,int side,int (*original)(void*,int,int,int)) {
    if(!session_.active())return original(manager,grade,stage,style);
    const auto difficulty=session_.difficulty(stage,style,side<0?result_side_:side);
    if(difficulty)return *difficulty;
    if(!custom_result())fault_=true;
    // Result renderer skips texture selection for difficulty >=5.
    return 5;
}
const char* NativeDan::result_title(const char* native_name,int part) const {
    if(!custom_result())return native_name;
    return part==0?"ti_self_dan_zi":"ti_self_dan_xuan";
}
void NativeDan::leave() {
    session_.leave();result_scene_=nullptr;result_depth_=0;result_side_=0;fault_=false;
}
} // namespace self_dan
