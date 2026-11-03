#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include "native_dan.hpp"
#include "native_folder.hpp"
#include "native_selector.hpp"
#include "native_calls.hpp"
#include "native_hooks.hpp"
#include "native_compat.hpp"
#include "native_chain.hpp"
#include "trace_queue.hpp"
#include "addresses.hpp"
#include "dan_options.hpp"
#include <cstdio>

// Experimental integration for the exact supported executable. All game state
// changes and branch publication run on the scene thread. The worker only
// prepares unpublished relays, attaches the bootstrap slot and drains logs.
namespace {
namespace n=self_dan::native;
using self_dan::NativeSession;using self_dan::NativeSelector;
std::atomic<bool> custom_course_active{false};
uintptr_t base{},life_scene{},life_flow{},folder_group{},standard_scene{},standard_flow{},premium_flow{};
HMODULE module;std::atomic<bool> stopping{false};
self_dan::TraceQueue queue;
std::unique_ptr<self_dan::NativeDan> dan;
n::EntryFolder folder;NativeSelector selector;
bool attached=false,entry_fade=false,published=false;
std::atomic<bool> life_entered{false}, integration_ready{false};
DWORD owner_thread{};
std::vector<self_dan::CallHook> calls;
std::vector<self_dan::SlotHook> slots;
void (*dan_exit_next)(void*){};
std::vector<uintptr_t> slot_originals;
struct BranchBinding {uintptr_t original;void (*set_next)(uintptr_t);};
std::vector<BranchBinding> branch_bindings;
self_dan::SlotHook bootstrap{},standard_bootstrap{},mode_bootstrap{};
n::Update standard_update_next{},mode_update_next{};
n::Update life_update_next{};
ULONGLONG retry_publication_at{};
template<class T> T& field(uintptr_t p,uintptr_t off=0){return *reinterpret_cast<T*>(p+(p==base?addresses::rva(off):off));}
template<class F> F fn(uintptr_t rva){return reinterpret_cast<F>(self_dan::ChainScope::resolve(base+addresses::rva(rva)));}
template<class F> F slot_fn(uintptr_t rva){
    for(size_t i=0;i<slots.size();++i)if(slot_originals[i]==rva)
        return reinterpret_cast<F>(slots[i].expected);
    return reinterpret_cast<F>(base+addresses::rva(rva));
}
template<class T> bool read(uintptr_t p,T& value){
    SIZE_T got{};return p && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),&value,sizeof(value),&got) && got==sizeof(value);
}
struct Reader {template<class T> bool operator()(uintptr_t p,T& v)const{return read(p,v);}};
void log(const char* event)noexcept{try{queue.push(std::string("{\"event\":\"")+event+"\",\"tick_ms\":"+std::to_string(GetTickCount64())+"}\n");}catch(...){}}
void log_stage(const char* event)noexcept{
    try{const auto& s=dan->session();queue.push(std::string("{\"event\":\"")+event+"\",\"tick_ms\":"+std::to_string(GetTickCount64())+
        ",\"stage\":"+std::to_string(s.stage()+1)+",\"phase\":"+std::to_string(int(s.phase()))+
        ",\"music_id\":"+std::to_string(s.music_id(s.stage(),s.style()).value_or(-1))+
        ",\"style\":"+std::to_string(s.style())+",\"minimum_level\":["+
        std::to_string(s.minimum_level(0))+","+std::to_string(s.minimum_level(1))+"]"+
        ",\"raw_health\":["+std::to_string(s.health_raw()[0])+","+std::to_string(s.health_raw()[1])+"]}\n");}catch(...){}}
n::Settings& settings(){return field<n::Settings>(base,n::settings);}
bool pending(){return dan->session().phase()==NativeSession::Phase::entry_pending;}
void* ptr(uintptr_t p){return reinterpret_cast<void*>(p);}

// Capture precisely the native option fields reset by 897870 and changed by
// Dan's exit. Preserve Hi-Speed/lane settings that the player may edit later.
struct RestoreState {
    struct Saved {int* address{};int value{};};
    Saved values[40]{};size_t count{};int mode{};bool active{};
    uint8_t* flags[2]{};uint8_t flag_values[2]{};
    void save(int* address){for(size_t i=0;i<count;++i)if(values[i].address==address)return;values[count++]={address,*address};}
    void capture(){
        mode=settings().mode;count=0;
        auto manager=fn<uintptr_t(*)()>(0x8841e0)();
        for(int style=0;style<2;++style)for(int side=0;side<2;++side){
            const auto options=fn<uintptr_t(*)(uintptr_t,int,int)>(0x896560)(manager,side,style);
            for(const auto off:{0x30,0x34,0x38,0x3c,0x40,0x44,0x50,0x54})save(&field<int>(options,off));
        }
        save(&field<int>(manager,addresses::layout().options_tail0));save(&field<int>(manager,addresses::layout().options_tail1));
        flags[0]=&field<uint8_t>(fn<uintptr_t(*)()>(0x882200)(),9);
        flags[1]=addresses::layout().has_extra_course?&field<uint8_t>(fn<uintptr_t(*)()>(0x884d70)(),9):nullptr;
        for(int i=0;i<2;++i)if(flags[i]){flag_values[i]=*flags[i];*flags[i]=0;}
        active=true;
    }
    void restore(){
        if(!active)return;
        for(size_t i=0;i<count;++i)*values[i].address=values[i].value;
        for(int i=0;i<2;++i)if(flags[i])*flags[i]=flag_values[i];
        settings().mode=mode;active=false;
    }
} restore_state;

#include "dan_options_runtime.inc"
bool music_select_entry_mode(){return settings().mode==0 || settings().mode==6;}

void detach_folder(){
    if(!attached)return;
    if(folder.detach(folder_group)){attached=false;folder_group=0;log("folder_detached");}
    else log("folder_detach_failed");
}
void attach_folder(uintptr_t group){
    if((!life_scene && !standard_scene) || (settings().mode!=5 && !music_select_entry_mode()) || pending() || dan->session().active() || !group)return;
    uintptr_t vt{};if(!read(group,vt) || vt!=base+addresses::rva(settings().mode==5?0xcb5040:0xcb5090))return;
    if(attached && group!=folder_group){log("folder_group_changed");return;}
    const bool was=attached;
    attached=folder.attach(group,[](size_t bytes){return fn<void*(*)(size_t)>(0xb1ab58)(bytes);},
        [](void* p){fn<void(*)(void*,size_t)>(0xb1ab94)(p,0);});
    if(attached){folder_group=group;if(!was)log("folder_attached");}
    else if(!was)log("folder_registration_refused");
}
const n::FolderDescriptor* folder_descriptor(void* manager,int id){
    if(attached && id==n::EntryFolder::id)return folder.descriptor();
    return fn<const n::FolderDescriptor*(*)(void*,int)>(0x86dc40)(manager,id);
}
uintptr_t folder_lookup(void* manager,int id,int bank){
    if(attached && id==n::EntryFolder::id){
        if(bank==3)bank=field<int>(reinterpret_cast<uintptr_t>(manager),addresses::layout().category_bank)==1?1:0;
        if(bank<0 || bank>1)return 0;
        return reinterpret_cast<uintptr_t>(folder.category(bank));
    }
    return fn<uintptr_t(*)(void*,int,int)>(0x86db70)(manager,id,bank);
}
uintptr_t folder_current(void* manager){
    if(attached && field<int>(reinterpret_cast<uintptr_t>(manager),addresses::layout().category_opened)==n::EntryFolder::id)
        return folder_lookup(manager,n::EntryFolder::id,3);
    return fn<uintptr_t(*)(void*)>(0x86dbf0)(manager);
}
void folder_sort(void* category,int order,bool option){
    if(folder.owns(reinterpret_cast<uintptr_t>(category)))return;
    fn<void(*)(void*,int,bool)>(0x86faa0)(category,order,option);
}
void group_populate(void* group){
    fn<void(*)(void*)>(0x6377b0)(group);attach_folder(reinterpret_cast<uintptr_t>(group));
}
n::Selection current_selection(uintptr_t scene);
void folder_open(void* widget,bool sound){
    const bool standard=standard_scene && music_select_entry_mode();
    auto cursor=standard?current_selection(standard_scene):n::read_stepup_selection(base,Reader{});
    if(!attached || (!standard && (!life_scene || settings().mode!=5)) || !folder.owns(cursor.category)){
        fn<void(*)(void*,bool)>(0x602640)(widget,sound);return;
    }
    cursor.opened=true;
    if(dan->session().request_entry(settings(),cursor,cursor.category)){
        field<uint8_t>(cursor.category,4)=1;log("entry_requested");
    }
}
struct Callback {uintptr_t function{};int adjustment{},padding{};};
void life_transition(void* state,const Callback* callback,int param){
    // The native active callback may queue ordinary song confirmation in the
    // same frame as the folder-open input. Do not enter that song-only callback.
    if(life_scene && reinterpret_cast<uintptr_t>(state)==life_scene+addresses::layout().scene_state){
        if(pending())return;
        if(callback->function==base+addresses::rva(0x8d8280)){
            const auto cursor=n::read_stepup_selection(base,Reader{});
            if(folder.owns(cursor.category)){
                // Input can move onto our header in the SAME frame the timer
                // expires, after the outer update's precheck. Require an open.
                field<int>(life_scene,addresses::layout().scene_timer)=std::max(2,field<int>(life_scene,addresses::layout().scene_limit));
                field<uint8_t>(life_scene,addresses::layout().scene_timer_flag)=1;log("entry_header_timeout_held");return;
            }
        }
    }
    fn<void(*)(void*,const Callback*,int)>(0x8d41e0)(state,callback,param);
}
void entry_wait(void* scene){
    // Mode-selection curtains must reach close_loop before changing scenes.
    // MusicSelect's native base initializer starts open_in on the same curtain.
    if(!fn<bool(*)()>(0x5d6cc0)())return;
    fn<void(*)(void*)>(0x8d85b0)(scene);
}
void start_entry_fade(){
    // Mode Select 8e71df passes an optional mode: int mode + engaged byte.
    // Mode 5 selects dlbg_stepup. false starts close_in; this global effect
    // survives LifeMusicSelect destruction and opens over the new selector.
    fn<void(*)()>(0x5c4db0)();fn<void(*)()>(0x5c4ac0)();
    fn<void(*)(int,int)>(0x5c49c0)(0x43a0,0x32);
    fn<void(*)(bool,uint64_t)>(0x5d6a50)(false,UINT64_C(0x100000005));
    const Callback wait{reinterpret_cast<uintptr_t>(&entry_wait),0,0};
    fn<void(*)(void*,const Callback*,int)>(0x8d41e0)(ptr(life_scene+addresses::layout().scene_state),&wait,0);
    entry_fade=true;log("entry_native_fade");
}

bool custom_browsing(){return dan && dan->session().active() &&
    dan->session().phase()==NativeSession::Phase::selecting;}
bool chart_unavailable(void* manager,int mid,int difficulty,int style,int category,bool a,bool b){
    if(custom_browsing()){
        const auto& session=dan->session();
        if(style!=session.style())return true;
        const auto music=fn<uintptr_t(*)(int)>(0x950bc0)(mid);
        const auto slot=n::chart_slot(style,difficulty);int8_t level{};
        if(!music || slot<0 || !read(music+n::music_levels_offset+slot,level) ||
           level<session.browsing_minimum() || level>12)return true;
    }
    return fn<bool(*)(void*,int,int,int,int,bool,bool)>(0x892ce0)(manager,mid,difficulty,style,category,a,b);
}
bool style_button(int button){
    if(dan->session().active() || pending())return false;
    return fn<bool(*)(int)>(0x7ffbb0)(button);
}
void set_select_style(int style){
    if(dan->session().active() || pending())style=dan->session().style();
    fn<void(*)(int)>(0x948320)(style);
}
void select_bgm(int sound,int volume){
    if(custom_browsing())sound=int(addresses::layout().custom_select_bgm);
    fn<void(*)(int,int)>(0x5c4a00)(sound,volume);
}
void select_stream_bgm(int sound,int kind){
    if(custom_browsing()){fn<void(*)()>(0x5c4ac0)();fn<void(*)()>(0x86a1a0)();return;}
    fn<void(*)(int,int)>(0x5c4d30)(sound,kind);
}
void loading_bgm(int sound,int volume){
    if(dan->session().active())sound=int(addresses::layout().custom_decide_sound);
    fn<void(*)(int,int)>(0x5c49c0)(sound,volume);
}
void loading_title(void* object,void* music,int stage,int skin){
    if(dan->session().active()){stage=dan->session().loading_stage();skin=0;}
    fn<void(*)(void*,void*,int,int)>(0x600ae0)(object,music,stage,skin);
}

n::Selection current_selection(uintptr_t scene){
    n::Selection s;s.style=settings().style;
    s.category=fn<uintptr_t(*)(void*)>(0x605b90)(ptr(scene+addresses::layout().select_list));
    const int single_side=!settings().joined[1]?0:(!settings().joined[0]?1:-1);
    return n::read_category_selection(s,Reader{},single_side);
}
void selection_help(bool rejected){
    char text[160];selector.help(dan->session(),text,rejected);
    fn<void(*)(const char*,int)>(0x970ab0)(text,0x10);
}
void reject_selection(uintptr_t scene){
    selector.reject(scene,dan->session());
    field<uint8_t>(scene,addresses::layout().select_confirmed)=0;
    // Native timer is measured in refresh ticks. Reuse its original duration,
    // so rejecting automatic confirmation leaves enough time to pick a chart.
    if(field<int>(scene,addresses::layout().scene_timer)<=0)field<int>(scene,addresses::layout().scene_timer)=std::max(1,field<int>(scene,addresses::layout().scene_limit));
    const Callback browse{base+addresses::rva(0x8eced0),0,0};
    auto state=field<uintptr_t>(scene,addresses::layout().scene_state);
    fn<void(*)(void*,const Callback*,int)>(0x8e99d0)(ptr(state),&browse,0);
    selection_help(true);log_stage("selection_rejected");
}
bool select_input(void* widget,bool locked,void* parameter){
    const bool confirmed=fn<bool(*)(void*,bool,void*)>(0x606510)(widget,locked,parameter);
    const auto scene=reinterpret_cast<uintptr_t>(widget)-addresses::layout().select_list;
    if(confirmed && selector.owns(scene) &&
       selector.confirm(scene,current_selection(scene),dan->session())==NativeSelector::Decision::reject){
        reject_selection(scene);return false;
    }
    return confirmed;
}
void select_transition(void* state,const Callback* callback,int param){
    const auto scene=field<uintptr_t>(reinterpret_cast<uintptr_t>(state));
    if(standard_scene && scene==standard_scene){
        if(pending())return;
        if((callback->function==base+addresses::rva(0x8ec650) || callback->function==base+addresses::rva(0x8ebc90) || callback->function==base+addresses::rva(0x8ebd60)) && folder.owns(current_selection(scene).category)){
            field<int>(scene,addresses::layout().scene_timer)=std::max(2,field<int>(scene,addresses::layout().scene_limit));field<uint8_t>(scene,addresses::layout().scene_timer_flag)=1;return;
        }
    }
    if(selector.owns(scene) && (callback->function==base+addresses::rva(0x8ec650) ||
       callback->function==base+addresses::rva(0x8ebc90) || callback->function==base+addresses::rva(0x8ebd60))){
        const auto cursor=current_selection(scene);
        const auto decision=callback->function==base+addresses::rva(0x8ebd60)?
            selector.commit(scene,cursor,dan->session()):selector.confirm(scene,cursor,dan->session());
        if(decision==NativeSelector::Decision::reject){reject_selection(scene);return;}
        if(callback->function==base+addresses::rva(0x8ebd60))log_stage("song_selected");
    }
    fn<void(*)(void*,const Callback*,int)>(0x8e99d0)(state,callback,param);
}
void select_assign(void* scene_pointer){
    const auto scene=reinterpret_cast<uintptr_t>(scene_pointer);
    if(!selector.owns(scene)){fn<void(*)(void*)>(0x8eb6e0)(scene_pointer);return;}
    // A frozen normal chart is supplied directly: this avoids native special
    // folder fallbacks and Standard's profile/category bookkeeping.
    const auto* song=selector.chosen();
    if(!song){log("missing_frozen_song");return;}
    field<uintptr_t>(scene,addresses::layout().select_music)=song->music;
    fn<void(*)(void*)>(0x948330)(ptr(song->music));
    for(int side=0;side<2;++side)fn<void(*)(int,int)>(0x948390)(song->difficulty[side],side);
}
uint64_t select_init(void* scene,void* parameter){

    const auto p=reinterpret_cast<uintptr_t>(scene);
    const bool own=dan->session().active() && selector.enter(p,dan->session());
    if(music_select_entry_mode() && !own){standard_scene=p;entry_fade=false;}
    const auto result=slot_fn<n::Init>(0x8ea560)(scene,parameter);
    if(own){
        // 5c46c0/5c4800 are volume controls, NOT resource-bank loaders.
        // Versioned Arena loop helper: 0909 b790; 0917 BPL season4 b240.
        // The generic 0917 Arena helper points to the silent dummy sound.
        // Native start sites choose Arena before any sound reaches the mixer.
        // Loop-if-not-playing covers native paths that did not start a BGM.
        fn<void(*)()>(0x86a1a0)();selection_help(false);
        log_stage("custom_selector_initialized");
    }
    return result;
}
void select_teardown(void* scene){

    const auto p=reinterpret_cast<uintptr_t>(scene);const bool own=selector.owns(p);
    if(p==standard_scene){detach_folder();standard_scene=0;}
    slot_fn<void(*)(void*)>(0x8eabf0)(scene);
    if(own){selector.leave(p);fn<void(*)(int)>(0x5c4a40)(int(addresses::layout().custom_select_bgm));}
}

int course_music(void* manager,int grade,int stage,int style){return dan->course_music(manager,grade,stage,style,fn<int(*)(void*,int,int,int)>(0x875260));}
template<int Side> int course_diff(void* manager,int grade,int stage,int style){return dan->course_difficulty(manager,grade,stage,style,Side,fn<int(*)(void*,int,int,int)>(0x8752e0));}
int result_save_account(int side){if(option_history.blocked())return 0;return dan->result_account(side,fn<int(*)(int)>(0x948110));}
bool result_render_side(int side){return dan->result_joined(side,fn<bool(*)(int)>(0x948120));}
template<int Part> void title_sprite(void* object,const char* node,const char* name){fn<void(*)(void*,const char*,const char*)>(0x833250)(object,node,dan->result_title(name,Part));}
uint64_t result_init(void* scene,void* p){
    capture_dan_practice();
    options_draw_logged=0;options_missing_logged=false;
    n::GradeResultScope scope(option_history.blocked()?reinterpret_cast<n::GradeManagerPrefix*>(fn<uintptr_t(*)()>(0x882200)()):nullptr,option_history.blocked());
    const auto result=dan->result_initialize(scene,p,slot_fn<n::Init>(0x8c6f10));
    initialize_dan_option_font();
    return result;
}
void result_update(void* scene,bool resumed,void* p){
    n::GradeResultScope scope(option_history.blocked()?reinterpret_cast<n::GradeManagerPrefix*>(fn<uintptr_t(*)()>(0x882200)()):nullptr,option_history.blocked());
    dan->result_update(scene,resumed,p,slot_fn<n::Update>(0x8c7d70));
}
void result_render(void* scene){
    n::GradeResultScope scope(option_history.blocked()?reinterpret_cast<n::GradeManagerPrefix*>(fn<uintptr_t(*)()>(0x882200)()):nullptr,option_history.blocked());
    dan->result_render(scene,slot_fn<void(*)(void*)>(0x8c75e0));
    draw_dan_options(scene);
}
uint64_t dan_init(void* flow,void* p){
    option_history.begin(pending());options_stage=-1;course_save_blocked.store(option_history.blocked());
    if(pending()){custom_course_active.store(true);fn<void(*)()>(0x973e00)();}
    auto result=dan->initialize(flow,p,slot_fn<n::Init>(0x8541d0));
    if(dan->session().owns(reinterpret_cast<uintptr_t>(flow)))log("native_dan_started");
    return result;
}
int dan_first(void* flow){return dan->first(flow,slot_fn<int(*)(void*)>(0x853f20));}
void dan_prepare(void* flow,int next){
    if(next==n::dan_stage)capture_dan_options();
    if(next==n::stage_result || next==n::dan_result)capture_dan_practice();
    dan->prepare(flow,next,slot_fn<n::Prepare>(0x8543b0));
    if(dan->session().active())log_stage("dan_prepare");
}
int dan_next(void* flow,int previous){return dan->next(flow,previous,slot_fn<n::Next>(0x853c60));}
void dan_teardown(void* flow){
    const bool own=dan->session().owns(reinterpret_cast<uintptr_t>(flow));
    dan_exit_next(flow);
    option_history.leave();options_stage=-1;course_save_blocked.store(false);
    if(own){restore_state.restore();dan->leave();custom_course_active.store(false);log("native_dan_left");}
}
int life_next(void* flow,int previous){
    if(previous==n::life_music_select && pending()){
        life_flow=reinterpret_cast<uintptr_t>(flow);return 4;
    }
    if(reinterpret_cast<uintptr_t>(flow)==life_flow && previous==4){
        life_flow=0;restore_state.restore();dan->leave();custom_course_active.store(false);return 0x7fffffff;
    }
    return slot_fn<n::Next>(0x855f60)(flow,previous);
}
void life_prepare(void* flow,int next){
    if(reinterpret_cast<uintptr_t>(flow)==life_flow && next==4 && pending()){
        restore_state.capture();settings().mode=3;return;
    }
    slot_fn<n::Prepare>(0x856490)(flow,next);
}
bool publish(uintptr_t firenet_base=reinterpret_cast<uintptr_t>(GetModuleHandleW(L"firenet.dll")));
void standard_entry_wait(void* scene){
    if(!fn<bool(*)()>(0x5d6cc0)() || !fn<bool(*)()>(0x5c4f10)())return;
    const auto vtable=*static_cast<uintptr_t*>(scene);
    reinterpret_cast<void(*)(void*,int)>(*reinterpret_cast<uintptr_t*>(vtable+0x58))(scene,0);
}
void standard_update(void* scene,bool resumed,void* parameter){
    if(!published){owner_thread=GetCurrentThreadId();publish();}
    const auto p=reinterpret_cast<uintptr_t>(scene);
    if(published && music_select_entry_mode() && !dan->session().active()){
        if(!standard_scene){standard_scene=p;entry_fade=false;}
        const auto cursor=n::read_stepup_selection(base,Reader{},0xcb5090);attach_folder(cursor.group);
        if(folder.owns(current_selection(p).category) && field<int>(p,addresses::layout().scene_timer)<=1)field<int>(p,addresses::layout().scene_timer)=2;
    }
    standard_update_next(scene,resumed,parameter);
    if(p==standard_scene && pending() && !entry_fade){
        fn<void(*)()>(0x5c4db0)();fn<void(*)()>(0x5c4ac0)();fn<void(*)(int,int)>(0x5c49c0)(0x43a0,0x32);
        fn<void(*)(bool,uint64_t)>(0x5d6a50)(false,UINT64_C(0x100000000)|uint32_t(settings().mode));
        const Callback wait{reinterpret_cast<uintptr_t>(&standard_entry_wait),0,0};
        fn<void(*)(void*,const Callback*,int)>(0x8e99d0)(ptr(field<uintptr_t>(p,addresses::layout().scene_state)),&wait,0);
        entry_fade=true;log("standard_entry_fade");
    }
}
int standard_next(void* flow,int previous){
    if(previous==n::music_select && pending()){standard_flow=reinterpret_cast<uintptr_t>(flow);return 4;}
    if(reinterpret_cast<uintptr_t>(flow)==standard_flow && previous==4){
        standard_flow=0;restore_state.restore();dan->leave();custom_course_active.store(false);return 0x7fffffff;
    }
    return slot_fn<n::Next>(0x857160)(flow,previous);
}
void standard_prepare(void* flow,int next){
    if(reinterpret_cast<uintptr_t>(flow)==standard_flow && next==4 && pending()){
        restore_state.capture();settings().mode=3;return;
    }
    slot_fn<n::Prepare>(0x858170)(flow,next);
}
int premium_next(void* flow,int previous){
    if(previous==n::music_select && pending()){premium_flow=reinterpret_cast<uintptr_t>(flow);return 4;}
    if(reinterpret_cast<uintptr_t>(flow)==premium_flow && previous==4){
        premium_flow=0;restore_state.restore();dan->leave();custom_course_active.store(false);return 0x7fffffff;
    }
    return slot_fn<n::Next>(0x856a80)(flow,previous);
}
void premium_prepare(void* flow,int next){
    if(reinterpret_cast<uintptr_t>(flow)==premium_flow && next==4 && pending()){
        restore_state.capture();settings().mode=3;return;
    }
    slot_fn<n::Prepare>(0x857030)(flow,next);
}
void mode_update(void* scene,bool resumed,void* parameter){
    if(!published){owner_thread=GetCurrentThreadId();publish();}
    mode_update_next(scene,resumed,parameter);
}
void life_update(void* scene,bool resumed,void* parameter){
    // This tick is delivered even if preparation finished after native init.
    // Publish before invoking the original update, on the scene's own thread.
    if(!life_entered.exchange(true))log("life_update_entered");
    if(!published && GetTickCount64()>=retry_publication_at){
        owner_thread=GetCurrentThreadId();
        if(!publish())retry_publication_at=GetTickCount64()+1000;
    }
    const auto p=reinterpret_cast<uintptr_t>(scene);
    if(published && !life_scene){life_scene=p;entry_fade=false;log("life_scene_adopted");}
    if(!published){life_update_next(scene,resumed,parameter);return;}
    if(p==life_scene && !pending()){
        const auto cursor=n::read_stepup_selection(base,Reader{});attach_folder(cursor.group);
        if(folder.owns(cursor.category) && field<int>(p,addresses::layout().scene_timer)<=1)field<int>(p,addresses::layout().scene_timer)=2;
    }
    life_update_next(scene,resumed,parameter);
    if(p==life_scene && pending() && !entry_fade)start_entry_fade();
}
void life_teardown(void* scene){

    if(reinterpret_cast<uintptr_t>(scene)==life_scene){detach_folder();life_scene=0;}
    slot_fn<void(*)(void*)>(0x8d62c0)(scene);
}
uint64_t life_init(void* scene,void* parameter){

    life_entered.store(true);log("life_init_entered");
    if(!published){owner_thread=GetCurrentThreadId();publish();}
    if(published){life_scene=reinterpret_cast<uintptr_t>(scene);entry_fade=false;}
    const auto result=slot_fn<n::Init>(0x8d56b0)(scene,parameter);
    if(published){auto cursor=n::read_stepup_selection(base,Reader{});attach_folder(cursor.group);}
    return result;
}

void add_slot(uintptr_t table,int index,uintptr_t expected,void* replacement){slots.push_back({reinterpret_cast<void**>(base+addresses::rva(table)+addresses::scene_slot(index)*8),ptr(base+addresses::rva(expected)),replacement});slot_originals.push_back(expected);}
void log_slot(const self_dan::SlotHook& h,const char* event)noexcept{
    try{
        uintptr_t current{};read(reinterpret_cast<uintptr_t>(h.slot),current);
        MEMORY_BASIC_INFORMATION region{};VirtualQuery(ptr(current),&region,sizeof(region));
        char path[MAX_PATH]{};
        GetModuleFileNameA(static_cast<HMODULE>(region.AllocationBase),path,MAX_PATH);
        const char* name=std::strrchr(path,'\\');name=name?name+1:path;
        queue.push(std::string("{\"event\":\"")+event+"\",\"slot_rva\":"+
            std::to_string(reinterpret_cast<uintptr_t>(h.slot)-base)+",\"expected\":"+
            std::to_string(reinterpret_cast<uintptr_t>(h.expected))+",\"current\":"+
            std::to_string(current)+",\"module\":\""+name+"\",\"module_offset\":"+
            std::to_string(current-reinterpret_cast<uintptr_t>(region.AllocationBase))+"}\n");
    }catch(...){}
}
bool usable_predecessor(uintptr_t current){
    if(!self_dan::executable_target(current))return false;
    // Never adopt our own wrapper/relay: reinstalling it would form a cycle.
    if(current==reinterpret_cast<uintptr_t>(&life_update) || current==reinterpret_cast<uintptr_t>(&standard_update) || current==reinterpret_cast<uintptr_t>(&mode_update))return false;
    for(const auto& h:slots)if(current==reinterpret_cast<uintptr_t>(h.replacement))return false;
    for(const auto& h:calls)if(current==reinterpret_cast<uintptr_t>(h.replacement) ||
        current==reinterpret_cast<uintptr_t>(h.relay))return false;
    return true;
}
bool resolve_slot_chain(uintptr_t firenet_base){
    std::vector<uintptr_t> next(slots.size());
    bool valid=true;
    for(size_t i=0;i<slots.size();++i){
        auto& h=slots[i];
        if(h.installed || h.protection_pending || !read(reinterpret_cast<uintptr_t>(h.slot),next[i]) ||
           (next[i]!=base+addresses::rva(slot_originals[i]) && !usable_predecessor(next[i]))){
            log_slot(h,"slot_conflict");valid=false;
        }
    }
    if(!valid)return false;
    for(size_t i=0;i<slots.size();++i){
        auto& h=slots[i];h.expected=ptr(next[i]);
        if(slot_originals[i]==0x854740)dan_exit_next=reinterpret_cast<void(*)(void*)>(next[i]);
        if(next[i]!=base+addresses::rva(slot_originals[i])){
            const bool recorder=slot_originals[i]==0x854740 && self_dan::compatible_firenet_exit(base,firenet_base,next[i],Reader{});
            log_slot(h,recorder?"firenet_exit_chained":"slot_chained");
        }
    }
    return true;
}
void log_branch(const self_dan::CallHook& h,const char* event)noexcept{
    try{queue.push(std::string("{\"event\":\"")+event+"\",\"site_rva\":"+
        std::to_string(reinterpret_cast<uintptr_t>(h.site)-base)+"}\n");}catch(...){}
}
bool resolve_branch(size_t i){
    auto& h=calls[i];uint8_t bytes[5]{};
    if(h.installed || h.protection_pending || !read(reinterpret_cast<uintptr_t>(h.site),bytes) || bytes[0]!=h.opcode)return false;
    int32_t displacement{};std::memcpy(&displacement,bytes+1,4);
    const auto current=reinterpret_cast<uintptr_t>(h.site)+5+displacement;
    if(current!=base+addresses::rva(branch_bindings[i].original) && !usable_predecessor(current))return false;
    h.expected_target=ptr(current);std::memcpy(h.before,bytes,5);
    branch_bindings[i].set_next(current);
    if(current!=base+addresses::rva(branch_bindings[i].original))log_branch(h,"branch_chained");
    return true;
}
bool publish(uintptr_t firenet_base){
    if(GetCurrentThreadId()!=owner_thread || published)return false;
    if(!resolve_slot_chain(firenet_base)){log("slot_publication_failed");return false;}
    // Other DLL workers may have completed after prepare(). Capture again on
    // the scene thread immediately before publishing, without assuming order.
    for(size_t i=0;i<calls.size();++i)if(!resolve_branch(i)){
        log_branch(calls[i],"branch_conflict");return false;
    }
    if(!self_dan::install_calls(calls.data(),calls.size())){log("branch_publication_failed");return false;}
    const auto status=self_dan::install_hooks(slots.data(),slots.size());
    if(status!=self_dan::HookStatus::installed){
        self_dan::restore_calls(calls.data(),calls.size());
        for(const auto& h:slots)log_slot(h,"slot_install_state");
        log("slot_publication_failed");return false;
    }
    published=true;integration_ready.store(true);log("integration_published");return true;
}
template<uintptr_t Site,class F> auto branch_wrapper(F fallback){
    if constexpr(Site==0x8544c6)return &course_diff<0>;
    else if constexpr(Site==0x8544ee)return &course_diff<1>;
    else if constexpr(Site==0x8c7844)return &title_sprite<1>;
    else return fallback;
}
bool prepare(){
    dan=std::make_unique<self_dan::NativeDan>(base);
    // Per-site difficulty side and title part are explicit, never inferred from
    // volatile registers or a return address after passing through a relay.
    #define BIND(purpose,site_rva,target,opcode_value) { \
        using Chain=self_dan::BranchChain<site_rva,decltype(+purpose)>; \
        Chain::canonical=base+addresses::rva(target);Chain::wrapper=branch_wrapper<site_rva>(purpose); \
        self_dan::CallHook h;h.site=reinterpret_cast<uint8_t*>(base+addresses::rva(site_rva)); \
        h.expected_target=ptr(base+addresses::rva(target));h.replacement=reinterpret_cast<void*>(&Chain::invoke);h.opcode=opcode_value; \
        calls.push_back(h);branch_bindings.push_back({target,&Chain::set_next}); }
    auto course_difficulty=&course_diff<-1>;
    auto result_title_sprite=&title_sprite<0>;
    if(addresses::current){
    #include "native_bindings_0917.inc"
    }else{
    #include "native_bindings.inc"
    }
    #undef BIND
    for(size_t i=0;i<calls.size();++i){
        if(!resolve_branch(i)){log_branch(calls[i],"branch_validation_failed");return false;}
    }
    self_dan::RelaySearchStats search{};
    const auto relay_started=GetTickCount64();
    if(!self_dan::prepare_calls(calls.data(),calls.size(),&search)){
        log("relay_allocation_failed");return false;
    }
    queue.push(std::string("{\"event\":\"relays_prepared\",\"count\":")+std::to_string(calls.size())+
        ",\"elapsed_ms\":"+std::to_string(GetTickCount64()-relay_started)+
        ",\"memory_queries\":"+std::to_string(search.queries)+
        ",\"allocation_attempts\":"+std::to_string(search.allocation_attempts)+"}\n");
    add_slot(0xd7f898,14,0x8d62c0,reinterpret_cast<void*>(&life_teardown));
    add_slot(0xd7f898,13,0x8d56b0,reinterpret_cast<void*>(&life_init));
    add_slot(0xdacb98,20,0x857030,reinterpret_cast<void*>(&premium_prepare));
    add_slot(0xdacb98,22,0x856a80,reinterpret_cast<void*>(&premium_next));
    add_slot(0xdaab78,20,0x858170,reinterpret_cast<void*>(&standard_prepare));
    add_slot(0xdaab78,22,0x857160,reinterpret_cast<void*>(&standard_next));
    add_slot(0xdacad8,20,0x856490,reinterpret_cast<void*>(&life_prepare));
    add_slot(0xdacad8,22,0x855f60,reinterpret_cast<void*>(&life_next));
    add_slot(0xda9c98,13,0x8541d0,reinterpret_cast<void*>(&dan_init));
    add_slot(0xda9c98,14,0x854740,reinterpret_cast<void*>(&dan_teardown));
    add_slot(0xda9c98,20,0x8543b0,reinterpret_cast<void*>(&dan_prepare));
    add_slot(0xda9c98,21,0x853f20,reinterpret_cast<void*>(&dan_first));
    add_slot(0xda9c98,22,0x853c60,reinterpret_cast<void*>(&dan_next));
    add_slot(0xd82768,13,0x8ea560,reinterpret_cast<void*>(&select_init));
    add_slot(0xd82768,14,0x8eabf0,reinterpret_cast<void*>(&select_teardown));
    add_slot(0xd7c5d0,24,0x8c75e0,reinterpret_cast<void*>(&result_render));
    add_slot(0xd7c5d0,13,0x8c6f10,reinterpret_cast<void*>(&result_init));
    add_slot(0xd7c5d0,15,0x8c7d70,reinterpret_cast<void*>(&result_update));
    return true;
}
bool install_bootstrap(){
    bootstrap={reinterpret_cast<void**>(base+addresses::rva(0xd7f898)+addresses::scene_slot(15)*8),ptr(base+addresses::rva(0x8d6a80)),reinterpret_cast<void*>(&life_update)};
    uintptr_t current{};
    if(read(reinterpret_cast<uintptr_t>(bootstrap.slot),current) &&
       (current==base+addresses::rva(0x8d6a80) || usable_predecessor(current))){
        bootstrap.expected=ptr(current);life_update_next=reinterpret_cast<n::Update>(current);
        if(current!=base+addresses::rva(0x8d6a80))log_slot(bootstrap,"bootstrap_chained");
        log(self_dan::install_hooks(&bootstrap,1)==self_dan::HookStatus::installed?"bootstrap_ready":"bootstrap_conflict");
    }else log_slot(bootstrap,"bootstrap_conflict");
    standard_bootstrap={reinterpret_cast<void**>(base+addresses::rva(0xd82768)+addresses::scene_slot(15)*8),nullptr,reinterpret_cast<void*>(&standard_update)};
    mode_bootstrap={reinterpret_cast<void**>(base+addresses::rva(0xd818d8)+addresses::scene_slot(15)*8),nullptr,reinterpret_cast<void*>(&mode_update)};
    for(auto* h:{&standard_bootstrap,&mode_bootstrap}){
        current=reinterpret_cast<uintptr_t>(*h->slot);if(!usable_predecessor(current))return false;
        h->expected=ptr(current);
        if(h==&standard_bootstrap)standard_update_next=reinterpret_cast<n::Update>(current);else mode_update_next=reinterpret_cast<n::Update>(current);
        if(self_dan::install_hooks(h,1)!=self_dan::HookStatus::installed)return false;
    }
    return bootstrap.installed && !bootstrap.protection_pending;
}
void log_loaded_mods()noexcept{
    for(const auto* name:{"firenet.dll","IIDXScore.dll","2dxtra.dll","2dx-gsm.dll","ifs_hook.dll","omnifix.dll"}){
        try{
            const auto address=reinterpret_cast<uintptr_t>(GetModuleHandleA(name));
            IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
            const bool identified=read(address,dos) && dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>=64 &&
                dos.e_lfanew<=4096 && read(address+dos.e_lfanew,nt) && nt.Signature==IMAGE_NT_SIGNATURE;
            queue.push(std::string("{\"event\":\"module_observed\",\"name\":\"")+name+"\",\"loaded\":"+
                (address?"true":"false")+",\"timestamp\":"+std::to_string(identified?nt.FileHeader.TimeDateStamp:0)+
                ",\"image_size\":"+std::to_string(identified?nt.OptionalHeader.SizeOfImage:0)+"}\n");
        }catch(...){}
    }
}
DWORD WINAPI worker(void*)noexcept{
    HANDLE output=INVALID_HANDLE_VALUE;
    try{
        HMODULE pinned{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&worker),&pinned))return 1;
        wchar_t path[32768]{};GetModuleFileNameW(module,path,32768);
        std::wstring dir(path);dir.resize(dir.find_last_of(L"\\/")+1);
        dir+=L"self-dan-mod";CreateDirectoryW(dir.c_str(),nullptr);
        dir+=L"\\logs";CreateDirectoryW(dir.c_str(),nullptr);
        SYSTEMTIME time{};GetLocalTime(&time);wchar_t suffix[100]{};
        swprintf(suffix,100,L"\\self-dan-%04u%02u%02u-%02u%02u%02u-%lu.jsonl",time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,GetCurrentProcessId());
        output=CreateFileW((dir+suffix).c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(output==INVALID_HANDLE_VALUE)return 2;
        log("self_dan_standalone_v2");
        const auto started=GetTickCount64();
        while(!stopping && GetTickCount64()-started<180000){auto game=GetModuleHandleW(L"bm2dx.dll");if(game){base=reinterpret_cast<uintptr_t>(game);break;}Sleep(100);}
        log(base?"game_module_detected":"game_module_wait_timeout");
        const bool supported=addresses::select(base);
        log(supported?addresses::version():"unknown_game_build");
        log("preparation_started");
        if(!GetModuleHandleW(L"ifs_hook.dll"))log("ifs_hook_missing");
        if(supported && GetModuleHandleW(L"ifs_hook.dll") && prepare()){
            log("preparation_finished");
            install_bootstrap();
        }else log("unsupported_or_conflicting_build");
        log_loaded_mods();
        size_t total{};uintptr_t last_bootstrap{};int last_scene=-1;
        std::vector<uintptr_t> last_slots;
        ULONGLONG next_check=0;bool waiting_reported=false;

        while(!stopping && total<32*1024*1024){
            for(const auto& line:queue.take()){
                size_t offset{};while(offset<line.size()){
                    DWORD wrote{};if(!WriteFile(output,line.data()+offset,DWORD(line.size()-offset),&wrote,nullptr)||!wrote){CloseHandle(output);return 3;}
                    offset+=wrote;total+=wrote;
                }
            }
            if(supported && GetTickCount64()>=next_check){
                next_check=GetTickCount64()+1000;
                uintptr_t current{};int scene=-1;
                if(bootstrap.installed && read(reinterpret_cast<uintptr_t>(bootstrap.slot),current) && current!=last_bootstrap){
                    if(current!=reinterpret_cast<uintptr_t>(&life_update))log_slot(bootstrap,"bootstrap_replaced");
                    last_bootstrap=current;
                }
                if(integration_ready.load()){
                    if(last_slots.empty())last_slots.resize(slots.size());
                    for(size_t i=0;i<slots.size();++i){
                        if(read(reinterpret_cast<uintptr_t>(slots[i].slot),current) && current!=last_slots[i]){
                            if(current!=reinterpret_cast<uintptr_t>(slots[i].replacement))log_slot(slots[i],"published_slot_replaced");
                            last_slots[i]=current;
                        }
                    }
                }
                if(read(base+addresses::rva(n::scene_id),scene) && scene!=last_scene){
                    queue.push(std::string("{\"event\":\"scene_observed\",\"scene\":")+std::to_string(scene)+"}\n");last_scene=scene;
                }
                if(!waiting_reported && !life_entered.load() && GetTickCount64()-started>60000){
                    log("waiting_for_selector_update");waiting_reported=true;
                }
            }
            Sleep(50);
        }
        CloseHandle(output);return 0;
    }catch(...){if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);return 4;}
}
} // namespace
extern "C" __declspec(dllexport) BOOL FreeDanBlocksGradeSaveV1(){return course_save_blocked.load()?TRUE:FALSE;}
// Queried by practice on the menu/scene thread. Every selecting phase is editable.
extern "C" __declspec(dllexport) BOOL SelfDanOptionsEditableV1(){return custom_browsing()?TRUE:FALSE;}
extern "C" __declspec(dllexport) BOOL SelfDanCourseActiveV1(){return custom_course_active.load()?TRUE:FALSE;}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
    if(reason==DLL_PROCESS_DETACH)stopping.store(true);
    return TRUE;
}
