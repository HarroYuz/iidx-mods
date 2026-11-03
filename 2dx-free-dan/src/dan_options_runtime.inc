// Only the two basic-menu eligibility calls and three RANDOM/FLIP cycling
// queries use STANDARD rules. The actual global mode stays Dan at all times.
bool chart_option_blocked(void* menu,int id,int mode){
    return fn<bool(*)(void*,int,int)>(0x80e130)(menu,id,self_dan::chart_menu_mode(mode,id));
}
int chart_random_mode(){
    return self_dan::random_cycle_mode(fn<int(*)()>(0x948100)());
}
void dan_option_reset(void* manager){
    struct Value {int* at;int value;};
    std::vector<Value> saved;
    const auto m=reinterpret_cast<uintptr_t>(manager);
    for(int style=0;style<2;++style)for(int side=0;side<2;++side){
        const auto block=fn<uintptr_t(*)(uintptr_t,int,int)>(0x896560)(m,side,style);
        for(int off:{0x30,0x34,0x38,0x3c,0x50,0x54})
            saved.push_back({&field<int>(block,off),field<int>(block,off)});
    }
    for(auto off:{addresses::layout().options_tail0,addresses::layout().options_tail1})
        saved.push_back({&field<int>(m,off),field<int>(m,off)});
    fn<void(*)(void*)>(0x897870)(manager);
    // Deliberately exclude +40/+44: native gauge selection/reset remains intact.
    for(const auto& value:saved)*value.at=value.value;
}
self_dan::DanOptionHistory option_history;
std::atomic<bool> course_save_blocked{false};
int options_stage=-1;
unsigned options_draw_logged=0;
bool options_missing_logged=false;
void log_option_state(const char* event,int stage,int side,const char* status,int font=-1,int x=0,int y=0){
    try {
        std::string label;
        if(stage>=0 && stage<4 && side>=0 && side<2)
            label=option_history.stages()[stage].side[side].label(option_history.stages()[stage].style);
        queue.push(std::string("{\"event\":\"")+event+"\",\"stage\":"+std::to_string(stage+1)+
            ",\"side\":"+std::to_string(side+1)+",\"status\":\""+status+"\",\"label\":\""+label+
            "\",\"blocked\":"+(option_history.blocked()?"true":"false")+",\"font\":"+std::to_string(font)+
            ",\"x\":"+std::to_string(x)+",\"y\":"+std::to_string(y)+"}\n");
    } catch(...) {}
}
void capture_dan_options(){
    if(!option_history.active())return;
    options_stage=field<int>(base+addresses::rva(n::dan_state),4);
    self_dan::DanOptionHistory::Stage stage;stage.style=settings().style;
    const auto manager=fn<uintptr_t(*)()>(0x8841e0)();
    for(int side=0;side<2;++side){
        stage.joined[side]=settings().joined[side]!=0;
        auto& value=stage.side[side];
        const auto block=fn<uintptr_t(*)(uintptr_t,int,int)>(0x896560)(manager,side,stage.style);
        for(int lane=0;lane<2;++lane){
            const int native_lane=stage.style==1?lane:side;
            value.random[lane]=field<int>(block,0x30+4*native_lane);
            value.mirror[lane]=field<int>(block,0x38+4*native_lane);
        }
        value.autoscratch=field<int>(block,0x50);value.legacy=field<int>(block,0x54);
        // In SP this native field also marks a two-player credit, not BATTLE.
        value.battle=stage.style==1?field<int>(manager,addresses::layout().options_tail0):0;
        value.flip=field<int>(manager,addresses::layout().options_tail1);
    }
    option_history.capture(options_stage,stage);
    for(int side=0;side<2;++side)if(stage.joined[side])log_option_state("dan_options_capture",options_stage,side,"captured");
    course_save_blocked.store(option_history.blocked());
}
void capture_dan_practice(){
    if(!option_history.active())return;
    auto mod=GetModuleHandleW(L"2dx-practice.dll");
    if(!mod)return;
    using Query=unsigned(*)();
    const auto proc=GetProcAddress(mod,"PracticePlayedOptionsV1");
    Query query{};static_assert(sizeof(query)==sizeof(proc));std::memcpy(&query,&proc,sizeof(query));
    if(!query)return;
    const unsigned value=query();
    option_history.practice(options_stage,int(value&0xffff),(value&0x10000)!=0);
    log_option_state("dan_practice_capture",options_stage,0,"captured");
    course_save_blocked.store(option_history.blocked());
}
void initialize_dan_option_font(){
    if(!option_history.active())return;
    // Dan (including failure) does not construct the ordinary song-result
    // widget that registers this font. Match that constructor exactly; do
    // not rely on a previous song-result scene or substitute a title font.
    static constexpr float foreground[]{1.f,1.f,1.f,1.f};
    static constexpr float outline[]{0.f,0.f,0.f,1.f};
    const auto characters=fn<const char*(*)(int)>(0x5ee5c0)(0);
    const bool ok=fn<bool(*)(int,const char*,int,const char*,const float*,int,const float*,int,unsigned char)>(0x5ebd70)(
        int(addresses::layout().result_option_font),fn<const char*>(0xca1890),24,characters,
        foreground,1,outline,0,0x80);
    log_option_state("dan_options_font",-1,-1,ok?"initialized":"initialization_failed",
                     int(addresses::layout().result_option_font));
}
void draw_dan_options(void* scene){
    if(!option_history.active()){
        if(!options_missing_logged){log_option_state("dan_options_draw",-1,-1,"inactive_history");options_missing_logged=true;}
        return;
    }
    uintptr_t root{};
    if(!read(reinterpret_cast<uintptr_t>(scene)+addresses::layout().result_root,root) || !root){
        if(!options_missing_logged){log_option_state("dan_options_draw",-1,-1,"missing_root");options_missing_logged=true;}
        return;
    }
    const char* ordinal[]{"1st","2nd","3rd","4th"};
    for(int stage=0;stage<4;++stage){
        const auto& saved=option_history.stages()[stage];
        if(!saved.known)continue;
        for(int side=0;side<2;++side){
            if(!saved.joined[side])continue;
            const auto spans=self_dan::result_option_spans(saved.side[side],saved.style,option_history.blocked());
            if(spans.empty())continue;
            char name[32];std::snprintf(name,sizeof(name),"%dp_%s_music",side+1,ordinal[stage]);
            const auto node=fn<uintptr_t(*)(uintptr_t,const char*)>(0x3424e0)(root,name);
            if(!node){
                if(!options_missing_logged){log_option_state("dan_options_draw",stage,side,"missing_anchor");options_missing_logged=true;}
                continue;
            }
            const int x=fn<int(*)(uintptr_t)>(0x340180)(node);
            const int y=fn<int(*)(uintptr_t)>(0x3401c0)(node);
            const int width=fn<int(*)(uintptr_t)>(0x3407b0)(node);
            alignas(16) unsigned char font[0x68]{};
            fn<void*(*)(void*)>(0x3435c0)(font);
            const auto f=reinterpret_cast<uintptr_t>(font);
            field<unsigned>(f,0x44)=0xffffffff;
            field<int>(f,0x54)=0;field<int>(f,0x58)=1;
            field<unsigned>(f,0x40)=0x80000001;
            // Match native song-result tracking, scale and color. +0x44 is
            // tracking (-1), NOT a packed color; RGBA floats start at +0x18.
            std::string label;for(const auto& span:spans)label+=span.text;
            const auto measure=[&](int id,const char* text){return fn<float(*)(int,const char*,void*)>(0x5e80f0)(id,text,font);};
            const int font_id=self_dan::result_option_font(int(addresses::layout().result_option_font),label,measure);
            if(font_id<0){
                if(!options_missing_logged){log_option_state("dan_options_draw",stage,side,"missing_glyphs");options_missing_logged=true;}
                fn<void(*)(void*)>(0x343640)(font);
                continue;
            }
            const float text_width=measure(font_id,label.c_str());
            if(width>0 && text_width>width)
                field<float>(f,0x5c)*=float(width)/text_width;
            // Use each native song-name anchor, so SP/DP, player side and
            // animation placement agree with the game's own result layout.
            // The native title uses layer 99. Keep the label in that layer,
            // 12px below the previous position to clear the native difficulty
            // text, rather than layer 127 from the
            // ordinary song-result overlay (which has a different backdrop).
            std::string prefix;
            for(const auto& span:spans){
                // Measuring the entire prefix avoids accumulating per-span
                // rounding errors, including after horizontal fit-to-width.
                const int offset=int(measure(font_id,prefix.c_str())+0.5f);
                field<float>(f,0x18)=span.red?1.f:0.85f;
                field<float>(f,0x1c)=field<float>(f,0x20)=span.red?0.2f:0.85f;
                field<float>(f,0x24)=1.f;
                fn<void(*)(int,int,int,int,void*,const char*)>(0x5eb0b0)(font_id,x+offset,y+54,0x63,font,span.text.c_str());
                prefix+=span.text;
            }
            const unsigned bit=1u<<(stage*2+side);
            if(!(options_draw_logged&bit)){
                log_option_state("dan_options_draw",stage,side,"queued",font_id,x,y+54);
                options_draw_logged|=bit;
            }
            fn<void(*)(void*)>(0x343640)(font);
        }
    }
}
