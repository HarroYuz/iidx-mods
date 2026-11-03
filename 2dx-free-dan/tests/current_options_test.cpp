// Execute the 0917 menu's real branching, option setters, reset and reload.
// Only sound, graphics notifications and singleton getters are substituted.
#define SELF_DAN_RUNTIME_TEST
#include "../src/native_runtime.cpp"
#include <cassert>
alignas(16) static unsigned char manager_memory[0x400]{};
static n::GradeManagerPrefix grade_memory{};
static uintptr_t options_manager(){return reinterpret_cast<uintptr_t>(manager_memory);}
static uintptr_t grade_manager(){return reinterpret_cast<uintptr_t>(&grade_memory);}
static int current_style(){return settings().style;}
static int pressed_key=-1;
static bool test_key(int key){return key==pressed_key;}
static bool no_touch(void*,int){return false;}
static int menu_open(){return -1;}
static bool loop_playing=false;
static int loop_starts=0,stream_stops=0,last_cue=0;
static bool loop_status(int sound,int kind){assert(sound==0xb240 && kind==0);return loop_playing;}
static void play_loop(int sound,int volume){assert(sound==0xb240 && volume==50);loop_playing=true;++loop_starts;}
static void stop_stream(){++stream_stops;}
static void play_cue(int sound,int volume){assert(volume==50);last_cue=sound;}

static void noop(){}
static int draws=0,accounts=0;
static std::string drawn;
static std::vector<self_dan::OptionSpan> drawn_spans;
static int previous_end=100;
static int account(int){++accounts;return 1;}
static uint64_t grade_init(void*,void*){
    grade_memory.result_grade[0]=0;
    assert(result_save_account(0)==0);return 77;
}
static void grade_update(void*,bool,void*){grade_memory.result_grade[1]=0;assert(result_save_account(1)==0);}
static uintptr_t result_node(uintptr_t,const char* name){assert(std::strcmp(name,"1p_1st_music")==0 || std::strcmp(name,"1p_2nd_music")==0 || std::strcmp(name,"1p_3rd_music")==0 || std::strcmp(name,"1p_4th_music")==0);return 1;}
static int node_x(uintptr_t){return 100;}
static int node_y(uintptr_t){return 200;}
static int node_width(uintptr_t){return 500;}
template<class F> static void stub_at(uintptr_t rva,F callback);
static float measure(int font,const char* text,void* parameters){
    return font==39?float(std::strlen(text))*30.f*field<float>(reinterpret_cast<uintptr_t>(parameters),0x5c):0.f;
}
alignas(16) static unsigned char font_manager_memory[0x2400]{};
static uintptr_t font_manager(){return reinterpret_cast<uintptr_t>(font_manager_memory);}
static unsigned char glyphs[128*0x2c]{};
static int buckets[10007]{},links[128]{};
static int font_registrations=0;
static int register_font(uintptr_t manager,int id,const char* family,int size,int flags,bool wide,
                         const char* characters,int border,const float* foreground,const float* outline,unsigned char charset){
    assert(manager==font_manager() && id==39 && size==24 && flags==0 && !wide && border==1 && charset==0x80);
    assert(family==fn<const char*>(0xca1890));
    assert(foreground[0]==1.f && foreground[1]==1.f && foreground[2]==1.f && foreground[3]==1.f);
    assert(outline[0]==0.f && outline[1]==0.f && outline[2]==0.f && outline[3]==1.f);
    assert(std::strchr(characters,':') && std::strchr(characters,'[') && std::strchr(characters,'.'));
    std::memset(glyphs,0,sizeof(glyphs));std::memset(buckets,0,sizeof(buckets));std::memset(links,0,sizeof(links));
    for(int c=32;c<127;++c){
        const int index=c-31,hash=(39*257+c)%10007;
        const auto glyph=reinterpret_cast<uintptr_t>(glyphs+index*0x2c);
        field<unsigned char>(glyph)=39;field<unsigned char>(glyph,1)=1;
        field<int>(glyph,0x1c)=20;field<unsigned short>(glyph,0x28)=c;
        links[index]=buckets[hash];buckets[hash]=index;
    }
    ++font_registrations;return 0;
}
static void native_font_regression(){
    // Execute real 0917 registration/charset wrappers and width/lookup routines.
    // Replace the singleton, locks and glyph rasterizer; width reads the cache.
    stub_at(0x32e470,&font_manager);
    stub_at(0xad9b6c,&noop);stub_at(0xad9ba4,&noop);
    stub_at(0x32ff20,&register_font);
    const auto manager=font_manager();
    field<uintptr_t>(manager,0x2398)=reinterpret_cast<uintptr_t>(glyphs);
    field<uintptr_t>(manager,0x23a8)=reinterpret_cast<uintptr_t>(buckets);
    field<uintptr_t>(manager,0x23b0)=reinterpret_cast<uintptr_t>(links);
    alignas(16) unsigned char parameters[0x68]{};
    fn<void*(*)(void*)>(0x3435c0)(parameters);
    const auto width=[&](int font,const char* text){return fn<float(*)(int,const char*,void*)>(0x5e80f0)(font,text,parameters);};
    const std::string label="OPTION: 0.85x [NO SAVE]";
    assert(width(39,label.c_str())==0.f); // Fresh process: no ordinary song result yet.
    assert(self_dan::result_option_font(39,label,width)==-1);
    initialize_dan_option_font(); // Actual native wrapper + ASCII set; rasterizer substituted.
    assert(font_registrations==1);
    assert(width(8,label.c_str())==0.f);
    assert(width(39,label.c_str())==20.f*label.size());
    assert(self_dan::result_option_font(39,label,width)==39);
    field<int>(reinterpret_cast<uintptr_t>(parameters),0x44)=-1;
    assert(width(39,label.c_str())==19.f*label.size());
    // Also reject partially available glyph sets: losing the rate's dot must
    // not silently turn 0.85 into 085.
    glyphs[('.'-31)*0x2c+1]=0;
    assert(self_dan::result_option_font(39,label,width)==-1);
    glyphs[('.'-31)*0x2c+1]=1;
    std::puts("PASS: real native font cache reproduces missing result font and uses exact song-result font and tracking");
}
static void text_draw(int font,int x,int y,int layer,void* parameters,const char* text){
    assert(font==39 && y==254 && layer==0x63);
    const auto p=reinterpret_cast<uintptr_t>(parameters);
    assert(field<float>(p,0x60)==1.f && field<int>(p,0x44)==-1);
    if(x==100){drawn.clear();drawn_spans.clear();previous_end=100;++draws;}
    assert(std::abs(x-previous_end)<=1);
    const bool red=field<float>(p,0x18)==1.f;
    assert(field<float>(p,0x1c)==(red?0.2f:0.85f));
    assert(field<float>(p,0x20)==(red?0.2f:0.85f) && field<float>(p,0x24)==1.f);
    drawn+=text;drawn_spans.push_back({text,red});
    previous_end=x+int(measure(font,text,parameters)+0.5f);
    assert(previous_end<=601);
}
static uintptr_t block(int side,int style){
    return fn<uintptr_t(*)(uintptr_t,int,int)>(0x896560)(options_manager(),side,style);
}
template<class F> static void stub_at(uintptr_t rva,F callback){
    auto* code=reinterpret_cast<uint8_t*>(base+rva);
    DWORD old{},ignored{};assert(VirtualProtect(code,14,PAGE_EXECUTE_READWRITE,&old));
    const uint8_t jump[]{0xff,0x25,0,0,0,0};std::memcpy(code,jump,6);
    const auto target=reinterpret_cast<uintptr_t>(callback);std::memcpy(code+6,&target,8);
    assert(FlushInstructionCache(GetCurrentProcess(),code,14));
    assert(VirtualProtect(code,14,old,&ignored));
}
static void clear_options(){std::memset(manager_memory,0,sizeof(manager_memory));}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    auto game=LoadLibraryExW(L"bm2dx.dll",nullptr,DONT_RESOLVE_DLL_REFERENCES);
    assert(game);base=reinterpret_cast<uintptr_t>(game);
    assert(addresses::select(base) && addresses::current && prepare());
    owner_thread=GetCurrentThreadId();assert(publish(0));
    stub_at(addresses::rva(0x8841e0),&options_manager);
    stub_at(addresses::rva(0x882200),&grade_manager);
    stub_at(0x81a710,&current_style);
    for(auto rva:{0x7f1b60,0x7f1b80,0x7f88f0,0x5b7f70})stub_at(rva,&noop);
    settings()={};settings().mode=3;settings().joined[0]=1;
    alignas(16) unsigned char basic_menu[0x80]{};
    auto menu=reinterpret_cast<uintptr_t>(basic_menu);
    const auto cycle=reinterpret_cast<void(*)(void*,int)>(base+0x7f7e30);
    const auto reload=reinterpret_cast<void(*)(void*)>(base+0x7f9c30);
    // Native input IDs: 0/1 RANDOM +/-; 2/3 GAUGE +/-; 4/5 ASSIST +/-.
    for(int side:{0,1}) {
        settings().joined[0]=side==0;settings().joined[1]=side==1;
        clear_options();std::memset(basic_menu,0,sizeof(basic_menu));field<int>(menu,8)=side;
        for(int expected=1;expected<=3;++expected){
            cycle(basic_menu,0);
            assert(field<int>(block(side,0),0x30+side*4)==expected);
            assert(settings().mode==3); // No global mode spoofing.
        }
        cycle(basic_menu,0);assert(field<int>(block(side,0),0x38+side*4)==1);
        cycle(basic_menu,0);assert(field<int>(block(side,0),0x30+side*4)==0 && field<int>(block(side,0),0x38+side*4)==0);
        cycle(basic_menu,2);assert(field<int>(block(side,0),0x40)==0); // GAUGE remains locked.
        cycle(basic_menu,4);assert(field<int>(block(side,0),0x50)==1 && field<int>(block(side,0),0x54)==0);
        cycle(basic_menu,4);assert(field<int>(block(side,0),0x50)==0 && field<int>(block(side,0),0x54)==1);
        cycle(basic_menu,4);assert(field<int>(block(side,0),0x50)==1 && field<int>(block(side,0),0x54)==1);
        std::puts("PASS: real SP menu cycles RANDOM/ASSIST while native gauge remains locked");
    }
    settings().style=1;settings().joined[0]=1;settings().joined[1]=0;clear_options();
    std::memset(basic_menu,0,sizeof(basic_menu));
    cycle(basic_menu,0);assert(field<int>(block(0,1),0x30)==1 && field<int>(block(0,1),0x34)==0);
    cycle(basic_menu,8);assert(field<int>(block(0,1),0x34)==1);
    cycle(basic_menu,6);assert(field<int>(options_manager(),addresses::layout().options_tail1)==1);
    assert(field<int>(block(0,1),0x30)==1 && field<int>(block(0,1),0x34)==1);
    std::puts("PASS: real DP menu supports independent lane RANDOM and FLIP");
    // Full native keyboard input dispatch, not only the inner setter. Repeat
    // for all four custom-course selection phases and both SP/DP styles.
    stub_at(0x7f07c0,&menu_open);stub_at(0x585260,&test_key);stub_at(0x7f9bb0,&no_touch);
    const auto update=reinterpret_cast<void(*)(void*)>(base+0x7fa410);
    const auto key_for=reinterpret_cast<int(*)(void*,int,int)>(base+0x7f8fa0);
    for(int style:{0,1}){
        auto& session=dan->session();session.leave();
        settings().mode=0;settings().style=style;settings().joined[0]=1;settings().joined[1]=0;
        n::Selection cursor;cursor.category=1;cursor.category_valid=true;cursor.opened=true;cursor.style=style;
        assert(session.request_entry(settings(),cursor,1) && session.begin(0x4321));
        settings().mode=3;custom_course_active=true;option_history.begin(true);
        for(int stage=0;stage<4;++stage){
            assert(SelfDanOptionsEditableV1());clear_options();std::memset(basic_menu,0,sizeof(basic_menu));
            // Lookup raw key through the real version's mapping table.
            pressed_key=key_for(basic_menu,style?2:0,0);update(basic_menu);
            assert(field<int>(block(0,style),0x30)==1);
            pressed_key=key_for(basic_menu,style?2:0,4);update(basic_menu);
            assert(field<int>(block(0,style),0x50)==1);
            pressed_key=key_for(basic_menu,style?2:0,2);update(basic_menu);
            assert(field<int>(block(0,style),0x40)==0 && settings().style==style && settings().mode==3);
            field<int>(base+addresses::rva(n::dan_state),4)=stage;capture_dan_options();
            assert(option_history.stages()[stage].side[0].random[0]==1 && option_history.blocked());
            self_dan::NativeSession::Song song{0x1234,123,{3,3},{10,10}};
            assert(session.select(song) && !SelfDanOptionsEditableV1() && session.begin_stage(stage));
            std::array<n::DanRow,8> rows{};assert(session.capture_rows(stage,rows,{4000,0}));
            assert(session.after_native_next(n::stage_result,stage==3?n::dan_result:n::dan_stage,stage==3?stage:stage+1)==(stage==3?n::dan_result:n::music_select));
        }
        session.leave();custom_course_active=false;option_history.leave();course_save_blocked=false;
    }
    std::puts("PASS: native keyboard RANDOM/ASSIST input on all four custom selections in SP/DP; gauge and play style retained");
    // Execute the actual 0917 BPL loop helper, with only the mixer boundary
    // replaced. The generic Arena helper in this build plays a silent dummy.
    stub_at(0x5b80f0,&loop_status);stub_at(0x5b7fb0,&play_loop);
    stub_at(0x5b8070,&stop_stream);stub_at(0x5b7f70,&play_cue);
    auto& session=dan->session();settings().mode=0;settings().style=0;
    n::Selection cursor;cursor.category=1;cursor.category_valid=true;cursor.opened=true;cursor.style=0;
    assert(session.request_entry(settings(),cursor,1) && session.begin(0x4321));settings().mode=3;
    assert(addresses::layout().custom_select_bgm==0xb240 && addresses::layout().custom_decide_sound==0xb290);
    select_stream_bgm(123,2);assert(loop_starts==1 && stream_stops==1);
    fn<void(*)()>(0x86a1a0)();assert(loop_starts==1); // No restart while already playing.
    select_bgm(123,50);assert(loop_starts==2);
    loading_bgm(123,50);assert(last_cue==0xb290);session.leave();
    stub_at(0x5b7f70,&noop); // Restore the menu notification substitute.
    std::puts("PASS: actual 0917 BPL loop checks/starts correct sound once; versioned selection and decide cues");
    // Dan reset keeps precisely chart-affecting options, while native gauge
    // reset still runs. Hi-Speed and lane settings are never restored over.
    for(int style=0;style<2;++style)for(int side=0;side<2;++side){
        for(int off:{0x30,0x34,0x38,0x3c,0x50,0x54})field<int>(block(side,style),off)=1;
        field<int>(block(side,style),0x40)=3;field<int>(block(side,style),0x44)=1;
        field<float>(block(side,style),0x58)=2.5f;
    }
    dan_option_reset(manager_memory);
    for(int style=0;style<2;++style)for(int side=0;side<2;++side){
        for(int off:{0x30,0x34,0x38,0x3c,0x50,0x54})assert(field<int>(block(side,style),off)==1);
        assert(field<int>(block(side,style),0x40)==0 && field<int>(block(side,style),0x44)==0);
        assert(field<float>(block(side,style),0x58)==2.5f);
    }
    clear_options();settings().style=0;reload(basic_menu);settings().mode=0;
    cycle(basic_menu,2);assert(field<int>(block(0,0),0x40)==1); // STANDARD gauge unaffected.
    std::puts("PASS: actual native Dan reset, native gauge, Hi-Speed preservation and ordinary STANDARD options");
    clear_options();settings().mode=3;settings().joined[0]=settings().joined[1]=1;
    field<int>(options_manager(),addresses::layout().options_tail0)=1;
    field<int>(base+addresses::rva(n::dan_state),4)=0;option_history.begin(false);capture_dan_options();
    assert(!FreeDanBlocksGradeSaveV1() && option_history.stages()[0].side[0].label(0)=="");
    settings().joined[1]=0;field<int>(block(0,0),0x30)=1;field<int>(block(0,0),0x50)=1;
    capture_dan_options();assert(FreeDanBlocksGradeSaveV1());
    stub_at(addresses::rva(0x948110),&account);
    stub_at(addresses::rva(0x8c6f10),&grade_init);stub_at(addresses::rva(0x8c7d70),&grade_update);
    stub_at(addresses::rva(0x8c75e0),&noop);
    native_font_regression();
    stub_at(addresses::rva(0x3424e0),&result_node);
    stub_at(addresses::rva(0x340180),&node_x);stub_at(addresses::rva(0x3401c0),&node_y);
    stub_at(addresses::rva(0x3407b0),&node_width);stub_at(addresses::rva(0x5e80f0),&measure);
    stub_at(addresses::rva(0x5eb0b0),&text_draw);
    alignas(16) unsigned char result[0x200]{};
    field<uintptr_t>(reinterpret_cast<uintptr_t>(result),addresses::layout().result_root)=1;
    grade_memory.result_grade[0]=13;grade_memory.result_grade[1]=10;
    assert(result_init(result,nullptr)==77);result_update(result,false,nullptr);result_render(result);
    assert(grade_memory.result_grade[0]==13 && grade_memory.result_grade[1]==10 && accounts==0);
    assert(draws==1 && drawn=="OPTION: RANDOM A-SCR [NO SAVE]");
    assert(font_registrations==2); // Final-result init owns its font dependency.
    // Ordinary Dan fails on each possible stage; only attempted songs have labels.
    for(int failed_stage=0;failed_stage<4;++failed_stage){
        clear_options();option_history.begin(false);
        for(int stage=0;stage<=failed_stage;++stage){
            field<int>(base+addresses::rva(n::dan_state),4)=stage;
            capture_dan_options();option_history.practice(stage,100,true);
        }
        course_save_blocked.store(option_history.blocked()); // capture_dan_practice publishes this snapshot.
        std::memset(glyphs,0,sizeof(glyphs));
        const int before=draws,registrations=font_registrations;
        assert(result_init(result,nullptr)==77);result_render(result);
        assert(font_registrations==registrations+1 && draws==before+failed_stage+1);
        assert(drawn=="OPTION: ALL-SCR [NO SAVE]" && FreeDanBlocksGradeSaveV1());
        assert(drawn_spans.size()==2 && drawn_spans[1].red && drawn_spans[1].text=="ALL-SCR [NO SAVE]");
    }
    clear_options();option_history.begin(false);
    field<int>(base+addresses::rva(n::dan_state),4)=0;capture_dan_options();
    const int before_plain=draws;result_render(result);assert(draws==before_plain);
    option_history.practice(0,85,false);
    field<int>(base+addresses::rva(n::dan_state),4)=1;capture_dan_options();
    result_render(result);assert(drawn=="[NO SAVE]" && drawn_spans.size()==1 && drawn_spans[0].red);
    draws=1;
    // Match the reported case: ordinary Dan, four songs, 0.85x, no RANDOM.
    clear_options();option_history.begin(false);
    for(int stage=0;stage<4;++stage){
        field<int>(base+addresses::rva(n::dan_state),4)=stage;
        capture_dan_options();option_history.practice(stage,85,false);
        assert(option_history.stages()[stage].side[0].label(0)=="0.85x");
    }
    assert(option_history.blocked());
    result_render(result);
    assert(draws==5 && drawn=="OPTION: 0.85x [NO SAVE]");
    assert(drawn_spans.size()==2 && drawn_spans[0].text=="OPTION: " && !drawn_spans[0].red);
    assert(drawn_spans[1].text=="0.85x [NO SAVE]" && drawn_spans[1].red);
    // Native song result explicitly selects font 39 in both measure and draw.
    assert(*reinterpret_cast<unsigned char*>(base+0x909f04)==0xb9);
    assert(field<int>(base+0x909f04,1)==int(addresses::layout().result_option_font));
    option_history.leave();course_save_blocked=false;
    assert(result_save_account(0)==1 && accounts==1);
    std::puts("PASS: SP two-player scope, ordinary Dan result cache/upload guard, native text ABI/anchors and exit reset");
}
