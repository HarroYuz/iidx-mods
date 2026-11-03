// Execute the real integration wrappers against synthetic memory and native
// API stubs. Never load bm2dx.dll or create a game/device process.
#define SELF_DAN_RUNTIME_TEST
#include "../src/native_runtime.cpp"
#include <cstdlib>
#include <unordered_set>
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static uintptr_t category_pointer{};
static bool input_confirmed=false;
static int original_open_calls{},original_transition_calls{},song_assign_calls{},resets{},arena_starts{},arena_stops{};
static int recorder_exits{},native_exits{};
static int transition_starts{},transition_ends{},played_sound{},played_loop{},stream_calls{},title_stage{},title_skin{};
static bool transition_done=false,native_blocked=false;
static uintptr_t fixture_music{};
alignas(8) static unsigned char options_manager[0x300]{};
static n::GradeManagerPrefix grades{};
static unsigned char extra_manager[32]{};
static std::unordered_set<void*> allocations;
static uintptr_t get_options(){return reinterpret_cast<uintptr_t>(options_manager);}
static uintptr_t get_grade(){return reinterpret_cast<uintptr_t>(&grades);}
static uintptr_t get_extra(){return reinterpret_cast<uintptr_t>(extra_manager);}
static uintptr_t get_option_block(uintptr_t p,int side,int style){return p+8+(style==1?2:side)*0xb4;}
static void* native_alloc(size_t bytes){auto p=std::malloc(bytes);allocations.insert(p);return p;}
static void native_free(void* p,size_t){check(allocations.erase(p)==1,"native allocation ownership");std::free(p);}
static void noop(){}
static void scene_noop(void*){}
static void entry_start_stub(bool loop,uint64_t mode){
    check(!loop && mode==UINT64_C(0x100000005),"mode transition starts close_in with engaged Step Up mode");
    ++transition_starts;
}
static bool entry_done_stub(){return transition_done;}
static void entry_end_stub(void*){++transition_ends;}
static void sound_stub(int sound,int){played_sound=sound;}
static void loop_stub(int sound,int){played_loop=sound;}
static void stream_stub(int,int){++stream_calls;}
static bool button_stub(int){return true;}
static void style_stub(int style){settings().style=style;}
static void title_stub(void*,void*,int stage,int skin){title_stage=stage;title_skin=skin;}
static uintptr_t music_lookup_stub(int){return fixture_music;}
static bool blocked_stub(void*,int,int,int,int,bool,bool){return native_blocked;}
static void native_exit_stub(void*){++native_exits;}
static void recorder_exit_stub(void* flow){
    ++recorder_exits;
    if(dan->session().active())check(settings().mode==3,"recorder exits before restoring Step Up mode");
    fn<void(*)(void*)>(0x854740)(flow);
}
static void reset_players(){++resets;}
static void start_arena(){++arena_starts;}
static void stop_arena(int sound){check(sound==0xb790,"stop only Arena loop");++arena_stops;}
static void show_help(const char* text,int){check(std::strstr(text,"CUSTOM DAN")!=nullptr,"help text emitted");}
static uint64_t init_stub(void*,void*){return 1;}
static int life_init_count{},life_update_count{};
static uint64_t life_init_stub(void*,void*){++life_init_count;return 77;}
static void life_update_stub(void*,bool,void*){++life_update_count;}
static void open_stub(void*,bool){++original_open_calls;}
static void transition_stub(void* state,const Callback* c,int p){
    ++original_transition_calls;const auto at=reinterpret_cast<uintptr_t>(state);
    field<Callback>(at,0x38)=*c;field<int>(at,0x4c)=p;field<uint8_t>(at,0x50)=1;
}
static bool input_stub(void*,bool,void*){return input_confirmed;}
static uintptr_t category_stub(void*){return category_pointer;}
static void music_stub(void* music){++song_assign_calls;settings().active_music=reinterpret_cast<uintptr_t>(music);}
static void diff_stub(int difficulty,int side){settings().difficulty[side]=difficulty;}
static void prepare_stub(void*,int){}
static int life_next_stub(void*,int){return 63;}
static uint64_t dan_init_stub(void*,void*){
    check(settings().mode==3 && grades.special_course==0 && extra_manager[9]==0,"formal mode set before native initialization");
    field<int>(base,n::dan_state+4)=0;field<int>(get_options(),0x224)=0;
    field<int>(get_option_block(get_options(),0,0),0x30)=0;return 0;
}
static void commit_page(uintptr_t address){
    check(VirtualAlloc(ptr(address&~uintptr_t(4095)),4096,MEM_COMMIT,PAGE_EXECUTE_READWRITE)!=nullptr,"commit fixture page");
}
template<class F> static void stub(uintptr_t rva,F target){
    const auto address=base+rva;commit_page(address);auto* code=reinterpret_cast<uint8_t*>(address);
    const uint8_t jump[]{0xff,0x25,0,0,0,0};std::memcpy(code,jump,6);
    auto p=reinterpret_cast<uintptr_t>(target);std::memcpy(code+6,&p,8);
    check(FlushInstructionCache(GetCurrentProcess(),code,14),"flush fixture stub");
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    base=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,n::image_size,MEM_RESERVE,PAGE_NOACCESS));check(base!=0,"reserve fake PE");
    for(auto rva:{n::settings,n::dan_state,n::category_manager,uintptr_t(0xb287480)})commit_page(base+rva);
    #define BIND(purpose,site_rva,target,opcode_value) { \
        auto address=base+site_rva;commit_page(address);uint8_t bytes[5]; \
        check(self_dan::relative_call(reinterpret_cast<uint8_t*>(address),ptr(base+target),bytes),"encode original fixture branch"); \
        bytes[0]=opcode_value;std::memcpy(ptr(address),bytes,5); }
    #include "../src/native_bindings.inc"
    #undef BIND
    check(prepare(),"prepare all integration relays");
    for(auto& slot:slots){commit_page(reinterpret_cast<uintptr_t>(slot.slot));*slot.slot=slot.expected;}
    // A changed target must leave every vtable and branch untouched.
    auto saved=*slots.back().slot;*slots.back().slot=ptr(0x1234);
    owner_thread=GetCurrentThreadId();check(!publish() && !published,"conflicting slot rejects integration");
    for(const auto& h:calls)check(self_dan::call_matches(h,h.before),"branch rollback after slot conflict");
    *slots.back().slot=saved;check(publish(),"publish whole integration on scene thread");
    for(const auto& h:calls)check(self_dan::call_matches(h,h.after),"all branch relays published");
    check(self_dan::remove_hooks(slots.data(),slots.size()) && self_dan::restore_calls(calls.data(),calls.size()),"restore synthetic installation");
    // Reproduce firenet's actual collision: DanFlow slot 14 is its recorder
    // wrapper, with the native callback saved separately inside firenet.
    auto compat=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x1a6000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    check(compat!=0,"allocate recorder fixture");
    field<IMAGE_DOS_HEADER>(compat).e_magic=IMAGE_DOS_SIGNATURE;
    field<IMAGE_DOS_HEADER>(compat).e_lfanew=0x100;
    auto& nt=field<IMAGE_NT_HEADERS64>(compat,0x100);nt.Signature=IMAGE_NT_SIGNATURE;
    nt.FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt.FileHeader.TimeDateStamp=0x6aacbc3a;
    nt.OptionalHeader.SizeOfImage=0x191000;nt.OptionalHeader.AddressOfEntryPoint=0x87c7c;
    auto exit_address=compat+self_dan::firenet_exit_rva;
    std::memcpy(ptr(exit_address),self_dan::firenet_exit_code,sizeof(self_dan::firenet_exit_code));
    auto& saved_exit=field<uintptr_t>(compat,self_dan::firenet_saved_exit_rva);saved_exit=base+0x854740;
    auto& exit_slot=field<uintptr_t>(base,0xda9c98+14*8);exit_slot=exit_address;
    published=false;
    exit_slot=reinterpret_cast<uintptr_t>(&dan_teardown);
    check(!publish(),"own callback cannot become its predecessor");
    exit_slot=0x1234;check(!publish(),"non-executable predecessor refused");
    exit_slot=exit_address;
    check(self_dan::compatible_firenet_exit(base,compat,exit_address,Reader{}),"old recorder fingerprint");
    nt.FileHeader.TimeDateStamp=0x6abaa846;nt.OptionalHeader.SizeOfImage=0x1a6000;nt.OptionalHeader.AddressOfEntryPoint=0x8808c;
    exit_address=compat+self_dan::firenet_new_exit_rva;
    std::memcpy(ptr(exit_address),self_dan::firenet_new_exit_code,sizeof(self_dan::firenet_new_exit_code));
    field<uintptr_t>(compat,self_dan::firenet_new_saved_exit_rva)=base+0x854740;exit_slot=exit_address;
    check(self_dan::compatible_firenet_exit(base,compat,exit_address,Reader{}),"new recorder fingerprint");
    auto other=*slots.back().slot;*slots.back().slot=ptr(0x1234);
    check(!publish(compat) && exit_slot==exit_address,"another conflict preserves recorder wrapper");*slots.back().slot=other;
    check(publish(compat) && dan_exit_next==reinterpret_cast<void(*)(void*)>(exit_address),"publish with verified recorder chain");
    // Use a test double only after the audited bytes pass validation.
    const uint8_t jump[]{0xff,0x25,0,0,0,0};auto recorder=reinterpret_cast<uintptr_t>(&recorder_exit_stub);
    std::memcpy(ptr(exit_address),jump,6);std::memcpy(ptr(exit_address+6),&recorder,8);
    check(FlushInstructionCache(GetCurrentProcess(),ptr(exit_address),14),"flush recorder test double");
    stub(0x854740,&native_exit_stub);
    reinterpret_cast<void(*)(void*)>(exit_slot)(ptr(0x1234));
    check(recorder_exits==1 && native_exits==1,"ordinary Dan teardown calls recorder and native exactly once");
    check(self_dan::remove_hooks(slots.data(),slots.size()) && self_dan::restore_calls(calls.data(),calls.size()) &&
        exit_slot==exit_address,"rollback restores recorder callback, not vanilla callback");
    // Install native API test doubles only after proving original signatures.
    stub(0x8841e0,&get_options);stub(0x896560,&get_option_block);stub(0x882200,&get_grade);stub(0x884d70,&get_extra);
    stub(0xb1ab58,&native_alloc);stub(0xb1ab94,&native_free);stub(0x602640,&open_stub);
    stub(0x8d41e0,&transition_stub);stub(0x8e99d0,&transition_stub);stub(0x5c4db0,&noop);stub(0x5c4ac0,&noop);
    stub(0x8d56b0,&life_init_stub);stub(0x8d6a80,&life_update_stub);
    stub(0x8d62c0,&scene_noop);stub(0x8ea560,&init_stub);stub(0x8eabf0,&scene_noop);
    stub(0x855f60,&life_next_stub);stub(0x856490,&prepare_stub);stub(0x8541d0,&dan_init_stub);
    stub(0x973e00,&reset_players);stub(0x86a1a0,&start_arena);stub(0x5c4a40,&stop_arena);
    stub(0x970ab0,&show_help);stub(0x606510,&input_stub);stub(0x605b90,&category_stub);
    stub(0x948330,&music_stub);stub(0x948390,&diff_stub);
    stub(0x5d6a50,&entry_start_stub);stub(0x5d6cc0,&entry_done_stub);stub(0x8d85b0,&entry_end_stub);
    stub(0x5c49c0,&sound_stub);stub(0x5c4a00,&loop_stub);stub(0x5c4d30,&stream_stub);
    stub(0x7ffbb0,&button_stub);stub(0x948320,&style_stub);stub(0x600ae0,&title_stub);
    stub(0x950bc0,&music_lookup_stub);stub(0x892ce0,&blocked_stub);
    settings()={};settings().mode=5;settings().style=1;settings().joined[0]=1;
    std::vector<uint64_t> manager_storage((0x2a428+7)/8),list_storage(24),group_storage(24),life_storage(0x800/8);
    const auto manager=reinterpret_cast<uintptr_t>(manager_storage.data()),list=reinterpret_cast<uintptr_t>(list_storage.data());
    const auto group=reinterpret_cast<uintptr_t>(group_storage.data());
    uintptr_t head[5]{},node[5]{};int group_key=7;
    head[0]=reinterpret_cast<uintptr_t>(node);node[0]=reinterpret_cast<uintptr_t>(head);node[2]=7;node[3]=group;
    field<uintptr_t>(base,n::category_manager)=manager;field<uintptr_t>(manager,0x175d0)=list;
    field<int>(manager,0x2a424)=0;field<uintptr_t>(list,0x10)=reinterpret_cast<uintptr_t>(head);
    field<int>(list,0x78)=1;field<int>(list,0x7c)=0;
    auto gp=reinterpret_cast<uintptr_t>(&group_key);field<n::Vector>(list,0x60)={gp,gp+4,gp+4};
    field<uintptr_t>(group)=base+0xcb5040;
    // Reproduce rc2's missed-init case: Step Up is already open before the
    // worker installs its bootstrap. No second native init is allowed.
    const auto open_scene=reinterpret_cast<uintptr_t>(life_storage.data());
    check(fn<n::Init>(0x8d56b0)(ptr(open_scene),nullptr)==77,"native scene already initialized");
    field<uintptr_t>(base,0xd7f898+15*8)=base+0x8d6a80;
    commit_page(base+0xd818d8);stub(0x8eaf30,&life_update_stub);
    field<uintptr_t>(base,0xd82768+15*8)=base+0x8eaf30;
    field<uintptr_t>(base,0xd818d8+15*8)=base+0x8eaf30;
    check(install_bootstrap(),"install bootstrap after native init");
    published=false;integration_ready=false;life_scene=0;
    const auto native_last=*slots.back().slot;*slots.back().slot=ptr(0x1234);
    auto update=reinterpret_cast<n::Update>(field<uintptr_t>(base,0xd7f898+15*8));
    update(ptr(open_scene),false,nullptr);
    check(!published && !attached && life_update_count==1,"publication failure still forwards native update");
    *slots.back().slot=native_last;retry_publication_at=0;
    update(ptr(open_scene),false,nullptr);
    check(published && life_scene==open_scene && life_init_count==1 && life_update_count==2,"next update adopts existing scene without reinitializing it");
    update(ptr(open_scene),false,nullptr);
    check(life_update_count==3 && life_init_count==1,"later ticks forward native update once");
    check(attached && folder_group==group,"runtime folder attached");
    check(std::strcmp(folder.descriptor()->selected,"ss_self_dan")==0 &&
        std::strcmp(folder.descriptor()->auxiliary,"so_self_dan")==0 &&
        std::strcmp(folder.descriptor()->label,"so_b_all")==0,"custom folder text overlays native background bar");
    const Callback ordinary{base+0x8d8280,0,0};
    field<int>(life_scene,0x84)=3600;
    life_transition(ptr(life_scene+0x90),&ordinary,0);
    check(!pending() && original_transition_calls==0 && field<int>(life_scene,0x80)==3600,"same-frame header timeout cannot enter song confirmation");
    folder_open(ptr(life_scene+0x358),true);check(pending() && !original_open_calls,"own entry bypasses native sort/open");
    life_transition(ptr(life_scene+0x90),&ordinary,0);check(original_transition_calls==0,"suppress same-frame ordinary confirmation");
    start_entry_fade();check(field<Callback>(life_scene+0x90,0x38).function==reinterpret_cast<uintptr_t>(&entry_wait) &&
        transition_starts==1 && played_sound==0x43a0,"entry starts mode-selection Step Up curtain and sound");
    entry_wait(ptr(life_scene));check(!transition_ends,"entry cannot end before animation completes");
    transition_done=true;entry_wait(ptr(life_scene));check(transition_ends==1,"completed animation reaches native audio/end wait");
    uint64_t life_parent[16]{},dan_parent[16]{};
    check(life_next(life_parent,n::life_music_select)==4,"Step Up parent pushes formal Dan child");
    field<int>(get_options(),0x224)=1;field<int>(get_option_block(get_options(),0,0),0x30)=3;
    grades.special_course=1;extra_manager[9]=1;
    life_teardown(ptr(life_scene));check(!attached && !life_scene,"detach folder before native select destruction");
    life_prepare(life_parent,4);check(settings().mode==3,"switch only at native parent prepare");
    dan_init(dan_parent,nullptr);check(resets==1 && dan->session().active() && dan->session().health_raw()[0]==5000,"formal Dan session starts at 100 percent");
    std::vector<uint64_t> select_storage(0x2040/8),state_storage(0x70/8),music_storage(0x700/8);
    auto scene=reinterpret_cast<uintptr_t>(select_storage.data()),state=reinterpret_cast<uintptr_t>(state_storage.data());
    field<uintptr_t>(scene,0x90)=state;field<uintptr_t>(state)=scene;field<int>(scene,0x84)=3600;
    n::Category category;category.cursor=0;category.count=1;category.songs=1;
    struct Chart {uintptr_t music;int difficulty[2];} chart{reinterpret_cast<uintptr_t>(music_storage.data()),{3,3}};
    n::Entry entry{1,0,reinterpret_cast<uintptr_t>(&chart)};auto ep=reinterpret_cast<uintptr_t>(&entry);
    category.entries={ep,ep+16,ep+16};category_pointer=reinterpret_cast<uintptr_t>(&category);
    field<int>(chart.music,n::music_id_offset)=25007;
    field<int8_t>(chart.music,n::music_levels_offset+8)=12;
    fixture_music=chart.music;
    select_init(ptr(scene),nullptr);check(arena_starts==1 && selector.owns(scene),"custom selector starts Arena loop");
    check(!style_button(3),"SP/DP toggle input disabled");set_select_style(0);check(settings().style==1,"profile reload cannot change course style");
    select_bgm(0x1234,50);check(played_loop==0xb790,"native BGM start is replaced before playback");
    select_stream_bgm(0x1234,2);check(!stream_calls && arena_starts==2,"streamed generic BGM is also replaced");
    loading_bgm(0x1234,50);check(played_sound==0xb7e0,"loading uses Arena title cue");
    loading_title(nullptr,nullptr,5,1);check(title_stage==0 && title_skin==0,"first native loading stage");
    native_blocked=true;check(chart_unavailable(nullptr,25007,3,1,10,true,true),"native chart locks remain enforced");native_blocked=false;
    check(!chart_unavailable(nullptr,25007,3,1,10,true,true),"valid first chart available");
    check(chart_unavailable(nullptr,25007,3,0,10,true,true),"other play style unavailable");
    entry.type=2;field<int>(scene,0x80)=0;field<uint8_t>(scene,0x2010)=1;
    const Callback confirm{base+0x8ec650,0,0};select_transition(ptr(state),&confirm,0);
    check(field<uint8_t>(scene,0x2010)==0 && field<int>(scene,0x80)==3600 && dan->session().phase()==NativeSession::Phase::selecting,"invalid timeout returns to selection and rearms timer");
    entry.type=1;input_confirmed=true;check(select_input(ptr(scene+0x198),false,nullptr),"valid native input accepted");
    select_transition(ptr(state),&confirm,0);check(dan->session().phase()==NativeSession::Phase::selecting,"preliminary callback leaves choice mutable");
    const Callback outgoing{base+0x8ebd60,0,0};select_transition(ptr(state),&outgoing,0);
    check(dan->session().phase()==NativeSession::Phase::selected,"final outgoing callback freezes chosen chart");
    select_assign(ptr(scene));check(song_assign_calls==1 && settings().active_music==chart.music && settings().difficulty[0]==3,"native assignment uses frozen chart");
    select_teardown(ptr(scene));check(!selector.owns(scene) && arena_stops==1,"selector teardown stops own Arena loop");
    // The shared availability predicate is used while building categories,
    // cycling difficulties and refreshing bars, before confirmation is possible.
    auto& session=dan->session();
    check(session.begin_stage(0),"fixture begins first stage");std::array<n::DanRow,8> rows{};
    check(session.capture_rows(0,rows,{4000,0}) && session.after_native_next(n::stage_result,n::dan_stage,1)==n::music_select,"fixture advances with Lv12 minimum");
    field<int8_t>(chart.music,n::music_levels_offset+8)=11;
    check(chart_unavailable(nullptr,25007,3,1,10,true,true),"lower-level chart disabled during browsing");
    field<int8_t>(chart.music,n::music_levels_offset+8)=12;
    check(!chart_unavailable(nullptr,25007,3,1,10,true,true),"equal-level chart remains selectable");
    loading_title(nullptr,nullptr,0,1);check(title_stage==1,"second loading page uses second-stage texture");
    dan_teardown(dan_parent);check(settings().mode==5 && !dan->session().active(),"return restores entry mode");
    check(recorder_exits==2 && native_exits==2,"custom Dan teardown preserves recorder exactly once");
    check(field<int>(get_options(),0x224)==1 && field<int>(get_option_block(get_options(),0,0),0x30)==3 && grades.special_course==1 && extra_manager[9]==1,"restore exact original options and course flags");
    check(life_next(life_parent,4)==0x7fffffff,"entry parent exits without Step Up result");
    check(style_button(3),"ordinary style button preserved");set_select_style(0);check(settings().style==0,"ordinary style setter preserved");
    select_bgm(123,50);loading_bgm(456,50);select_stream_bgm(789,2);loading_title(nullptr,nullptr,5,1);
    check(played_loop==123 && played_sound==456 && stream_calls==1 && title_stage==5 && title_skin==1,"ordinary selection audio and title unchanged");
    check(reinterpret_cast<n::Init>(field<uintptr_t>(base,0xd7f898+13*8))(ptr(open_scene),nullptr)==77 &&
        life_init_count==2 && attached,"next Step Up visit preserves native init and reattaches folder");
    life_teardown(ptr(open_scene));
    // STANDARD uses the Version carousel group and its own parent flow.
    for(int entry_mode:{0,6})for(int play_style:{0,1}){
    settings().mode=entry_mode;settings().style=play_style;field<uintptr_t>(group)=base+0xcb5090;
    field<int>(group,0xa8)=field<int>(group,0xac)=0;
    standard_scene=scene;entry_fade=false;
    attach_folder(group);check(attached,"STANDARD registers custom folder in version group");
    category_pointer=reinterpret_cast<uintptr_t>(folder.category(0));
    field<int>(scene,0x80)=0;field<int>(scene,0x84)=3600;
    select_transition(ptr(state),&outgoing,0);
    check(!pending() && field<int>(scene,0x80)==3600,"STANDARD header cannot auto-confirm a random song");
    folder_open(ptr(scene+0x4c0),true);check(pending(),"STANDARD native folder-open requests custom course");
    const auto transitions_before=original_transition_calls;
    select_transition(ptr(state),&outgoing,0);check(original_transition_calls==transitions_before,"STANDARD same-frame song transition suppressed");
    uint64_t standard_parent[16]{};
    check((entry_mode==6?premium_next:standard_next)(standard_parent,n::music_select)==4,"STANDARD pushes real Dan child");
    select_teardown(ptr(scene));check(!standard_scene && !attached,"STANDARD detaches before selector destruction");
    (entry_mode==6?premium_prepare:standard_prepare)(standard_parent,4);check(settings().mode==3,"STANDARD switches mode only at child prepare");
    dan_init(dan_parent,nullptr);check(SelfDanCourseActiveV1() && dan->session().active(),"custom course publishes optional cooperation state");
    dan_teardown(dan_parent);check(settings().mode==entry_mode && !SelfDanCourseActiveV1(),"course exit restores STANDARD and clears cooperation state");
    check((entry_mode==6?premium_next:standard_next)(standard_parent,4)==0x7fffffff && !dan->session().active(),"STANDARD parent exits without song result accounting");
    }
    // A translated vtable is required when attaching the 0917 folder.
    addresses::use_current(true);commit_page(base+addresses::rva(n::settings));
    stub(addresses::rva(0xb1ab58),&native_alloc);stub(addresses::rva(0xb1ab94),&native_free);
    for(int entry_mode:{0,5,6}){
        settings()={};settings().mode=entry_mode;
        standard_scene=entry_mode==5?0:scene;life_scene=entry_mode==5?open_scene:0;
        field<uintptr_t>(group)=base+addresses::rva(entry_mode==5?0xcb5040:0xcb5090);
        attach_folder(group);check(attached,"0917 translated folder vtable accepted in STANDARD/PF/STEP UP");detach_folder();
    }
    standard_scene=life_scene=0;addresses::use_current(false);
    check(self_dan::remove_hooks(&bootstrap,1) && self_dan::remove_hooks(&standard_bootstrap,1) && self_dan::remove_hooks(&mode_bootstrap,1) && self_dan::remove_hooks(slots.data(),slots.size()) &&
        self_dan::restore_calls(calls.data(),calls.size()),"cleanup late-entry installation");
    for(auto& h:calls)check(self_dan::release_call(h),"release unpublished test relays");
    for(auto p:allocations)std::free(p);
    allocations.clear();
    VirtualFree(ptr(base),0,MEM_RELEASE);
    VirtualFree(ptr(compat),0,MEM_RELEASE);
    std::puts("PASS: experimental runtime hooks, native entry routing, timeout gate, frozen assignment and restoration (synthetic APIs only)");
}
