#define NO_FAIL_TEST
#include "../src/runtime.cpp"
#include "../../2dx-free-dan/src/native_session.hpp"
#include <cassert>
static unsigned choices[2][2]{};
static bool assisted_invalid=false,practice=false;
static int native_counts[5]{},provider_calls=0,request_count=0;
static unsigned char timing[0x70]{};
static unsigned char history_manager_data[0x90]{};
static void* history_manager(){return history_manager_data;}
static bool not_hidden(int){return false;}
static int threshold(int,int){return 80;}
static int line_count=0,first_y=0,last_y=0;
static void draw_line(void*,int,int,int y1,int,int y2,float,int,float){if(!line_count)first_y=y1;last_y=y2;++line_count;}
static int current_notes=123;
static void* timing_manager(){return timing;}
static int notes_original(void*,int side){return current_notes+side;}
static int side_index(int side){return side;}
static void opaque_judge(void*,int,void** object){
    auto& amount=nf::field<int>(*object,12);const unsigned hp=nf::field<unsigned>(*object,0);
    amount=hp>0 && hp<1666?-50:-100;
}
static unsigned painted_type=99;
static int painted_value=0;
static unsigned char plot_object[0x2000]{};
static void plot_original(void* object,int side,bool,void*){
    painted_type=nf::type(side);painted_value=nf::field<int>(object,0xc8+side*384*4);
}
static void* result_original(void* object){
    assert(!(nf::at<unsigned>(0xbcce288)&(1u<<19)));
    assert(nf::field<int>(object,0x1818)==1);
    nf::fn<void(*)(void*,int,bool,void*)>(0x9b65a0)(plot_object,0,false,nullptr);
    return object;
}
static int mode(){return nf::at<int>(0xb60b3a0);}
static int style(){return nf::at<int>(0xb60b3a4);}
static void* manager(){return reinterpret_cast<void*>(1);}
static unsigned get(void*,int side,int dp){return choices[dp][side];}
static void* set(void*,int side,unsigned g,int dp){choices[dp][side]=g;return nullptr;}
static bool is_practice(void*,int,int){return practice;}
static int hard_type(int side){return mode()==3 || nf::type(side)==3 || nf::type(side)==4;}
static bool exhard(void*,int side,int){return nf::type(side)==4;}
static unsigned char course[64]{};
static void* course_manager(){return course;}
static int course_index(void*){return 0;}
static int course_length(void*,int){return 3;}
static bool joined(int side){return nf::at<int>(0xb60b3b0+side*4)==1;}
static int note_count(void*,int){return 1000;}
static int64_t judge(int side,int kind,int){
    const auto g=nf::type(side);
    return kind<2?50:kind==2?(g>=3?150:100):(g>=3?1500:100);
}
static void* chart_original(int64_t,int64_t){for(int s=0;s<2;++s)nf::raw(s)=nf::type(s)>=3?5000:1100;return nullptr;}
static bool invalid_original(int,bool){return assisted_invalid;}
static int clear_original(int side){return nf::at<int>(0xb60b3b0+4*side)!=0;}
static int request_original(void*,int*,int,void*,void*){++request_count;return 9;}
static void provider(int,int){++provider_calls;}
static LONG WINAPI fault(EXCEPTION_POINTERS* e){
    std::printf("FAULT %lx at game+%llx\n",e->ExceptionRecord->ExceptionCode,static_cast<unsigned long long>(e->ContextRecord->Rip-nf::base));
    std::fflush(stdout);ExitProcess(4);
}
static void stub(uintptr_t rva,void* target){
    auto p=reinterpret_cast<unsigned char*>(nf::base+rva);DWORD old{},ignored{};
    assert(VirtualProtect(p,14,PAGE_EXECUTE_READWRITE,&old));unsigned char j[14]={0xff,0x25};
    std::memcpy(j+6,&target,8);std::memcpy(p,j,14);assert(VirtualProtect(p,14,old,&ignored));FlushInstructionCache(GetCurrentProcess(),p,14);
}
static void setup(int dp,int side,bool automatic,unsigned selected=4,int play_mode=6){
    nf::initialized=false;nf::at<int>(0xb60b3a0)=play_mode;nf::at<int>(0xb60b3a4)=dp;
    nf::at<void*>(0xb3e9f28)=reinterpret_cast<void*>(1);nf::at<int>(0xbcbd3f8)=1;
    for(auto& row:choices)for(auto& g:row)g=selected;
    for(int s=0;s<2;++s){nf::at<int>(0xb60b3b0+s*4)=s==side;nf::at<uintptr_t>(0xbcb66c8+s*16)=0;}
    nf::fn<void*(*)(int64_t,int64_t)>(0x95d060)(1000,1000);
    assert(nf::sides[side].active && !nf::sides[1-side].active);
    nf::sides[side].policy.automatic=automatic;
    nf::sides[side].policy.bottom=no_fail::Gauge::assisted;
}
static bool hard_option(void*,int side,int){return nf::type(side)==3;}
static bool assisted_option(void*,int side,int){return nf::type(side)==1;}
static std::string background_texture;
static void graph_texture(void*,const char* node,void*,const char* texture){assert(!std::strcmp(node,"graph_bg"));background_texture=texture;}
static void graph_visible(void*,const char*,bool){}
struct Node{bool visible=true;int alpha=100;float r=1,g=1,b=1;std::string texture;int x=0,y=900;};
static Node nodes[2][3];
static void* node(void*,const char* name){for(int side=0;side<2;++side)for(int i=0;i<3;++i)if(!std::strcmp(name,nf::fn<const char**>(0x103e960+side*24)[i]))return &nodes[side][i];return nullptr;}
static void texture(void* n,const char* name){static_cast<Node*>(n)->texture=name;}
static void visible(void* n,bool show){static_cast<Node*>(n)->visible=show;}
static void color(void* n,int alpha,float r,float g,float b){auto& v=*static_cast<Node*>(n);v.alpha=alpha;v.r=r;v.g=g;v.b=b;}
static int node_x(void* n){return static_cast<Node*>(n)->x;}
static int node_y(void* n){return static_cast<Node*>(n)->y;}
static void position(void* n,int x,int y){static_cast<Node*>(n)->x=x;static_cast<Node*>(n)->y=y;}
struct DeadRoot{void** table;bool visible=true;int measure=-1,notes=-1;};
static void dead_visible(void* p,bool show){static_cast<DeadRoot*>(p)->visible=show;}
static void* dead_vtable[6]{nullptr,nullptr,nullptr,nullptr,nullptr,reinterpret_cast<void*>(&dead_visible)};
static DeadRoot dead_roots[2]{{dead_vtable},{dead_vtable}};
static void* make_dead(void*,void*,const char* name,int,int,int){return &dead_roots[std::strcmp(name,"deadpoint_1p")?1:0];}
struct DeadText{DeadRoot* root;bool measure;};
static DeadText dead_texts[2][2];
static void* dead_find(void* root,const char* name){
    if(!root)return nullptr;
    const int side=root==&dead_roots[0]?0:1;const bool measure=!std::strcmp(name,"deadpoint");
    auto& t=dead_texts[side][measure];t={static_cast<DeadRoot*>(root),measure};return &t;
}
static void draw_dead(void*,int,int value,void* node,int,void*,int,int){auto t=static_cast<DeadText*>(node);(t->measure?t->root->measure:t->root->notes)=value;}
int main(int argc,char**argv){
    assert(argc==2);setbuf(stdout,nullptr);SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);AddVectoredExceptionHandler(1,&fault);
    nf::base=reinterpret_cast<uintptr_t>(LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES));assert(nf::base && addresses::select(nf::base));
    stub(0x92fca0,reinterpret_cast<void*>(&mode));stub(0x81a710,reinterpret_cast<void*>(&style));stub(0x871910,reinterpret_cast<void*>(&manager));
    stub(0x8829b0,reinterpret_cast<void*>(&get));stub(0x885ff0,reinterpret_cast<void*>(&set));stub(0x884a00,reinterpret_cast<void*>(&is_practice));
    stub(0x95c420,reinterpret_cast<void*>(&hard_type));stub(0x884a60,reinterpret_cast<void*>(&exhard));
    stub(0x86fab0,reinterpret_cast<void*>(&course_manager));stub(0x863ee0,reinterpret_cast<void*>(&course_index));
    stub(0x862be0,reinterpret_cast<void*>(&course_length));stub(0x92fcc0,reinterpret_cast<void*>(&joined));
    stub(0x871f70,reinterpret_cast<void*>(&manager));stub(0x88cd10,reinterpret_cast<void*>(&note_count));
    stub(0x815d40,reinterpret_cast<void*>(&timing_manager));stub(0x8e8f70,reinterpret_cast<void*>(&notes_original));
    stub(0x92fae0,reinterpret_cast<void*>(&side_index));
    stub(0x95b4a0,reinterpret_cast<void*>(&judge));stub(0x95d060,reinterpret_cast<void*>(&chart_original));
    stub(0x81ab30,reinterpret_cast<void*>(&invalid_original));stub(0x95c2e0,reinterpret_cast<void*>(&clear_original));stub(0xaa3c00,reinterpret_cast<void*>(&request_original));
    // Replay two prior inline providers; takeover must not simulate 25 times.
    auto first=safetyhook::create_inline(nf::fn<void*>(0x95bc20),reinterpret_cast<void*>(&provider));assert(first);
    auto second=safetyhook::create_inline(nf::fn<void*>(0x95bc20),reinterpret_cast<void*>(&provider));assert(second);
    nf::install();assert(nf::installed);
    auto update=nf::fn<void(*)(int,int)>(0x95bc20);
    for(int dp=0;dp<2;++dp)for(int side=0;side<2;++side){
        setup(dp,side,false);update(side,4);assert(nf::raw(side)==2000);
        update(side,4);assert(nf::raw(side)==-1000 && nf::blocked(side));
        update(side,0);assert(nf::raw(side)==-950);
        for(int i=0;i<25;++i)update(side,0);
        assert(nf::raw(side)>0 && nf::blocked(side));
        assert(nf::fn<int(*)(int)>(0x95c2e0)(side)==0);
        assert(!nf::fn<bool(*)(int,bool)>(0x81ab30)(side,false));
        assisted_invalid=true;assert(nf::fn<bool(*)(int,bool)>(0x81ab30)(side,false));assisted_invalid=false;
        setup(dp,side,true);update(side,4);assert(nf::type(side)==4);
        update(side,4);assert(nf::type(side)==3 && nf::sides[side].policy.tracks[4].value==-1000);
        for(int i=0;i<4;++i)update(side,4);
        assert(nf::sides[side].policy.tracks[4].value==-13000);
        assert(nf::sides[side].policy.tracks[3].failed && nf::type(side)<3);
        for(int i=0;i<100;++i)update(side,0);
        assert(nf::type(side)==0 && !nf::blocked(side));
        assert(nf::sides[side].policy.tracks[3].failed && nf::sides[side].policy.tracks[4].failed);
        assisted_invalid=true;assert(nf::fn<bool(*)(int,bool)>(0x81ab30)(side,false));assisted_invalid=false;
        setup(dp,side,false,0);for(int i=0;i<30;++i)update(side,4);assert(nf::raw(side)==100);
    }
    assert(provider_calls==0);
    // Keep the real judgement and CN low-health formula active at/below zero.
    for(int play_mode:{6,3})for(int hp:{1666,1665,0,-1000}){
        setup(0,0,false,3,play_mode);nf::delta(0,hp-nf::raw(0));
        nf::at<int>(0xbcbb2d0)=101;const int before=nf::raw(0);update(0,4);
        assert(nf::raw(0)==before-(hp<1666?50:101));
    }
    // Exercise the alternate gauge-object route too (its callback can reject
    // nonpositive or unsigned health before applying the low-health rule).
    stub(0x64a1e0,reinterpret_cast<void*>(&opaque_judge));nf::at<int>(0xb10a8a0)=1;
    for(int play_mode:{6,3})for(int side=0;side<2;++side)for(int hp:{0,-1000}){
        setup(0,side,false,3,play_mode);nf::delta(side,hp-nf::raw(side));
        int object[5]{hp,0,0,0,5000};nf::at<void*>(0xbcb66c8+16*side)=object;
        update(side,4);assert(nf::raw(side)==hp-50 && object[0]==hp-50);
        nf::at<void*>(0xbcb66c8+16*side)=nullptr;
    }
    setup(0,0,false,3);
    auto hold=nf::fn<void(*)(int,double,bool)>(0x95be70);
    hold(0,-120.0,false);assert(nf::raw(0)==-1000 && nf::blocked(0));
    hold(0,20.0,false);assert(nf::raw(0)==0 && nf::blocked(0));
    setup(1,1,true);hold(1,-120.0,false);
    assert(nf::sides[1].policy.tracks[3].value==-1000 && nf::sides[1].policy.tracks[4].value==-1000);
    assert(nf::type(1)<3);
    for(int sample : {0,383,384,-1}){
        nf::at<int16_t>(0xb123870+874)=static_cast<int16_t>(sample);
        nf::graph(nf::fn<void*>(0xb123870),0,0);
        if(sample>=0 && sample<384)assert(nf::sides[1].history[4][sample]==-1000);
    }
    setup(0,0,false,3);assert(!nf::blocked(0) && nf::sides[0].policy.tracks[3].value==5000);
    setup(0,0,false);update(0,4);update(0,4);
    const auto write=std::find_if(nf::requests.begin(),nf::requests.end(),[](auto&e){return practice_network::is_write(e);});
    const auto read=std::find_if(nf::requests.begin(),nf::requests.end(),[](auto&e){return std::strcmp(practice_network::method(e),"music.getrank")==0;});
    assert(write!=nf::requests.end() && read!=nf::requests.end());
    assert(nf::request(nullptr,nullptr,write->id,nullptr,nullptr)==9 && request_count==1);
    assert(nf::request(nullptr,nullptr,read->id,nullptr,nullptr)==9 && request_count==2);
    setup(0,0,true);for(int i=0;i<30;++i)update(0,4);
    assert(nf::request(nullptr,nullptr,write->id,nullptr,nullptr)==3 && request_count==2);
    setup(0,0,false);assert(!nf::blocked(0));
    assert(nf::request(nullptr,nullptr,write->id,nullptr,nullptr)==9);
    // Real native coordinate transformer, with the production signed adapter.
    unsigned char graph[0xc0]{};nf::field<int>(graph,0xb8)=0;
    auto transform=nf::fn<int(*)(void*,int,int)>(0x9b6b40);
    const int bottom=transform(graph,0,5000);nf::signed_plot=true;
    assert(transform(graph,-1000,5000)>bottom);nf::signed_plot=false;
    assert(transform(graph,-1000,5000)==bottom);
    // Execute native local-history storage and shared subscreen line renderer.
    // A later chart/reset and its current gauge must not overwrite old records.
    stub(0x871930,reinterpret_cast<void*>(&history_manager));
    DWORD old{},ignored{};auto import=nf::fn<void*>(0xc66f18);
    assert(VirtualProtect(import,8,PAGE_READWRITE,&old));
    nf::at<void*>(0xc66f18)=reinterpret_cast<void*>(&std::memcpy);
    assert(VirtualProtect(import,8,old,&ignored));
    unsigned char records[2][0xd08]{};
    for(int side=0;side<2;++side){
        auto state=history_manager_data+0x10+side*0x40;
        nf::field<unsigned char>(state,0)=1;nf::field<int>(state,4)=0;
        nf::field<void*>(state,8)=records[side];nf::field<void*>(state,0x10)=records[side]+0xd08;
        setup(0,side,false,3);nf::sides[side].history[3].fill(-1000);
        std::array<int,384> safe{};safe.fill(42);
        nf::fn<void(*)(void*,const void*,int)>(0x9e60f0)(state,safe.data(),sizeof(safe));
        assert(nf::field<int>(records[side],0xb8)==-1000 && safe[0]==42);
    }
    setup(0,0,true);update(0,4);update(0,4); // Best H, while viewing failed EXH.
    nf::sides[0].view=4;nf::sides[0].history[3].fill(2500);nf::sides[0].history[4].fill(-1000);
    std::array<int,384> safe{};
    nf::history(history_manager_data+0x10,safe.data(),sizeof(safe));
    assert(nf::field<int>(records[0],0xb8)==2500);
    stub(0x919fb0,reinterpret_cast<void*>(&not_hidden));stub(0x95b7d0,reinterpret_cast<void*>(&threshold));
    stub(0x9b5b60,reinterpret_cast<void*>(&draw_line));
    for(int kind:{1,2})for(int side=0;side<2;++side)for(int hp:{5000,2500,0,-1,-1000,-100000}){
        unsigned char subgraph[0x2000]{};nf::field<int>(subgraph,0xb8)=kind;nf::field<int>(subgraph,0xbc)=2;
        nf::field<float>(subgraph,side*4)=1.f;nf::field<int>(subgraph,0xc0+side*4)=384;
        for(int i=0;i<384;++i)nf::field<int>(subgraph,0xc8+(side*384+i)*4)=hp;
        setup(0,0,false,4);nf::in_result=true;nf::sides[0].history[4].fill(-9999);
        line_count=0;nf::fn<void(*)(void*,int,bool,void*)>(0x9b65a0)(subgraph,side,false,nullptr);
        const int top=nf::at<int>(0xd8ab28+kind*4),height=nf::at<int>(0xd8ab48+kind*4);
        const int expected=top+int((5000-int64_t(hp))*height/5000);
        assert(line_count>0 && first_y==expected && last_y==expected && first_y>=top);
        assert(nf::field<int>(subgraph,0xc8+side*384*4)==hp);
        auto hover=nf::fn<int(*)(void*,int,int)>(0x9b6ab0);
        const int width=nf::at<int>(0xd8ab38+kind*4);
        for(int x:{0,width/2,width})assert(hover(subgraph,side,x)==(hp<0?-int((-(int64_t(hp))+49)/50):hp/50));
    }
    // Browsing a failed graph must never change the upload/clear gauge. Two
    // provider callbacks see VEFX suppressed, so one press advances once.
    nf::plot_hook->reset();stub(0x9b65a0,reinterpret_cast<void*>(&plot_original));
    *nf::plot_hook=safetyhook::create_inline(nf::fn<void*>(0x9b65a0),reinterpret_cast<void*>(&nf::plot));assert(*nf::plot_hook);
    nf::result_hook->reset();stub(0x90b3a0,reinterpret_cast<void*>(&result_original));
    *nf::result_hook=safetyhook::create_inline(nf::fn<void*>(0x90b3a0),reinterpret_cast<void*>(&nf::result));assert(*nf::result_hook);
    setup(0,0,true);update(0,4);update(0,4);assert(nf::type(0)==3);
    nf::sides[0].history[4][0]=-1000;nf::sides[0].history[3][0]=nf::raw(0);nf::field<int>(plot_object,0xc8)=42;
    stub(0x884aa0,reinterpret_cast<void*>(&hard_option));stub(0x884980,reinterpret_cast<void*>(&assisted_option));
    stub(0x820bc0,reinterpret_cast<void*>(&graph_texture));stub(0x81fe00,reinterpret_cast<void*>(&graph_visible));
    unsigned char background_object[0x150]{};
    auto background=nf::fn<void(*)(void*,void*,int)>(0x909360);
    unsigned char result_object[0x1820]{};nf::field<int>(result_object,0x1818)=2;
    nf::at<unsigned>(0xbcce288)=0;background(background_object,nullptr,0);nf::result(result_object);
    assert(background_texture=="graph_hard" && painted_type==3 && painted_value==nf::raw(0) && nf::type(0)==3);
    assert(nf::field<int>(plot_object,0xc8)==42 && nf::field<int>(result_object,0x1818)==2);
    assert(nf::clear(0)==1 && !nf::blocked(0));
    nf::at<unsigned>(0xbcce288)=1u<<19;background(background_object,nullptr,0);nf::result(result_object);
    assert(background_texture=="graph_exhard" && painted_type==4 && painted_value==-1000);
    assert(nf::sides[0].view==4 && nf::type(0)==3);
    nf::result(result_object);background(background_object,nullptr,0);assert(nf::sides[0].view==4);
    nf::at<unsigned>(0xbcce288)=0;nf::result(result_object);
    nf::at<unsigned>(0xbcce288)=1u<<19;nf::result(result_object);background(background_object,nullptr,0);
    assert(nf::sides[0].view==0 && background_texture=="graph_normal" && painted_type==0 && nf::type(0)==3);
    // Native dead-point widget creation and drawing, with renderer endpoints
    // substituted. Automatic browsing selects independent first-death data.
    stub(0x32d900,reinterpret_cast<void*>(&manager));stub(0x32da80,reinterpret_cast<void*>(&make_dead));
    stub(0x81e9f0,reinterpret_cast<void*>(&dead_find));stub(0x5d8540,reinterpret_cast<void*>(&manager));
    stub(0x5d7ea0,reinterpret_cast<void*>(&draw_dead));
    setup(0,0,true);nf::field<int>(timing,0x64)=7;current_notes=77;
    update(0,4);update(0,4);assert(nf::sides[0].deaths[4].measure==7);
    nf::field<int>(timing,0x64)=19;current_notes=190;
    for(int i=0;i<4;++i)update(0,4);
    assert(nf::sides[0].deaths[3].measure==19 && nf::sides[0].deaths[4].measure==7);
    for(int i=0;i<100;++i)update(0,0);
    unsigned char dead_object[24]{};nf::at<unsigned char>(0xb4dd620)=0;nf::at<int>(0xb4dd624)=0;
    nf::dead_init(dead_object,nullptr);assert(nf::field<void*>(dead_object,8)==&dead_roots[0]);
    assert(nf::at<unsigned char>(0xb4dd620)==0);
    nf::at<unsigned>(0xbcce288)=0;nf::in_result=true;nf::at<int>(0xb4dd734)=999;
    for(unsigned view:{3u,4u,0u,4u,3u}){
        nf::sides[0].view=view;nf::dead_render(dead_object);
        assert(dead_roots[0].visible==(view>=3));
        if(view>=3){assert(dead_roots[0].measure==(view==3?19:7));assert(dead_roots[0].notes==(view==3?190:77));}
        assert(nf::at<int>(0xb4dd734)==999 && nf::type(0)==0);
    }
    setup(0,0,false,3);nf::field<int>(timing,0x64)=0;nf::delta(0,-5000);
    assert(nf::sides[0].deaths[3].recorded && nf::sides[0].deaths[3].measure==0);
    nf::field<int>(timing,0x64)=99;nf::delta(0,1000);
    nf::native_death(nf::fn<void*>(0xb4dd620),0,true);
    assert(nf::at<int>(0xb4dd734)==0); // Zero is a valid first-death measure.
    // Dan keeps native judgement rules and inter-song carry, but no immediate
    // death callback. Execute the actual native next-scene branch at song end.
    for(int dp=0;dp<2;++dp)for(int side=0;side<2;++side){
        setup(dp,side,false,0,3);assert(nf::sides[side].dan);
        nf::publish_value(side,2731); // Native prepare carries the previous song.
        nf::delta(side,-2800);assert(nf::raw(side)==-69 && nf::blocked(side));
        nf::delta(side,300);assert(nf::raw(side)==231 && nf::blocked(side));
        assert(nf::clear(side)==0 && nf::clear(-1)==0);
        nf::at<int>(0xb3e82a4)=1;nf::at<unsigned char>(0xb3e83b0)=0;
        nf::at<int>(0xb3e82a8+side*4)=1;
        nf::fn<void(*)(void*,int)>(0x841f30)(nullptr,24);
        assert(nf::at<int>(0xb3e82a8+side*4)==1);
        assert(nf::at<unsigned char>(0xb3e82b0+32*(side*4+1)+28)==0);
        assert(nf::at<int>(0xb3e82b0+32*(side*4+1)+20)==4);
        assert(nf::fn<int(*)(void*,int)>(0x8417e0)(nullptr,24)==26);
        assert(nf::at<int>(0xb3e82a4)==1);
        assert(nf::request(nullptr,nullptr,write->id,nullptr,nullptr)==3);
        setup(dp,side,false,0,3);nf::publish_value(side,3210);
        nf::delta(side,-10);assert(nf::raw(side)==3200 && !nf::blocked(side));
        nf::delta(side,-3200);assert(nf::raw(side)==0 && nf::blocked(side));
        // Real native judgement and CN drain must keep running below zero.
        nf::at<int>(0xbcbb2c4+20*side)=50;nf::at<int>(0xbcbb2d0+20*side)=100;
        update(side,4);assert(nf::raw(side)==-50);
        hold(side,-2.0,true);assert(nf::raw(side)==-100);
        update(side,0);assert(nf::raw(side)==-50 && nf::blocked(side));
    }
    setup(0,0,false);
    // Feed real no-fail/native Dan rows and scene decisions into the production
    // custom-course state machine, at every possible failure song and side.
    assert(addresses::select(nf::base));
    for(int dp=0;dp<2;++dp)for(int side=0;side<2;++side)for(int failed_stage=0;failed_stage<4;++failed_stage)for(bool recovered:{false,true}){
        self_dan::NativeSession session;
        self_dan::native::Settings settings{};settings.mode=6;settings.style=dp;settings.joined[side]=1;
        self_dan::native::Selection cursor;cursor.category_valid=true;cursor.category=0x1234;cursor.opened=true;cursor.style=dp;
        assert(session.request_entry(settings,cursor,0x1234) && session.begin(0x9876));
        std::memset(nf::fn<void*>(0xb3e82b0),0,8*sizeof(self_dan::native::DanRow));
        for(int stage=0;stage<=failed_stage;++stage){
            self_dan::NativeSession::Song song{uintptr_t(0x2000+stage),14014+stage,{3,3},{10,10}};
            assert(session.select(song) && session.begin_stage(stage));
            setup(dp,side,false,0,3);nf::publish_value(side,session.health_raw()[side]);
            if(stage==failed_stage){nf::delta(side,-nf::raw(side)-1000);if(recovered)nf::delta(side,1250);}
            else nf::delta(side,-123);
            nf::at<int>(0xb3e82a4)=stage;nf::at<int>(0xb3e82a8+side*4)=stage;
            nf::fn<void(*)(void*,int)>(0x841f30)(nullptr,24);
            std::array<self_dan::native::DanRow,8> rows{};
            std::memcpy(rows.data(),nf::fn<void*>(0xb3e82b0),sizeof(rows));
            std::array<int,2> health{};health[side]=nf::raw(side);
            assert(session.capture_rows(stage,rows,health));
            const int next=nf::fn<int(*)(void*,int)>(0x8417e0)(nullptr,24);
            const int routed=session.after_native_next(24,next,nf::at<int>(0xb3e82a4));
            if(stage==failed_stage){
                assert(rows[side*4+stage].cleared==0 && routed==26);
                assert(session.phase()==self_dan::NativeSession::Phase::result && session.suppress_grade_save());
                assert(session.health_raw()[side]==(recovered?250:-1000));
                assert(nf::at<int>(0xb3e82a8+side*4)==stage);
                assert(!session.music_id(stage+1,dp));
            }else assert(rows[side*4+stage].cleared==1 && routed==23);
        }
        session.leave();assert(!session.active() && !session.suppress_grade_save());
    }
    std::puts("PASS: real native Dan rows/next-scene -> custom course; all four failure stages, SP/DP both sides, negative and recovered failure, exact carry, save guard and exit reset");
    // Execute real native digit layout; substitute its graphics backend only.
    stub(0x32d1a0,reinterpret_cast<void*>(&node));stub(0x329da0,reinterpret_cast<void*>(&texture));stub(0x32a1c0,reinterpret_cast<void*>(&visible));
    stub(0x32ade0,reinterpret_cast<void*>(&node_x));stub(0x32ae20,reinterpret_cast<void*>(&node_y));stub(0x32a2c0,reinterpret_cast<void*>(&position));
    for(auto& row:nodes){row[0].x=110;row[1].x=120;row[2].x=140;}
    // Substitute only the final tint backend; execute native digit layout and
    // production tint adapter together, including both players and Dan.
    nf::tint_hook->reset();stub(0x32a790,reinterpret_cast<void*>(&color));
    *nf::tint_hook=safetyhook::create_inline(nf::fn<void*>(0x32a790),reinterpret_cast<void*>(&nf::tint));assert(*nf::tint_hook);
    for(int side=0;side<2;++side)for(int play_mode:{6,3}){
        setup(0,side,false,play_mode==3?0:4,play_mode);
        nf::publish_value(side,5000);nf::delta(side,-6000);
        auto names=nf::fn<const char**>(0x103e960+24*side);auto textures=nf::fn<const char**>(0x103e9a0);
        auto draw=[&](bool show=true){nf::number(-20,3,reinterpret_cast<void*>(1),names,textures,show,false,false,37);};
        draw();
        assert(!nodes[side][0].visible && nodes[side][1].visible && nodes[side][2].visible);
        assert(nodes[side][1].texture==textures[2] && nodes[side][2].texture==textures[0]);
        for(const auto& n:nodes[side])assert(n.r==1 && n.g==0.2f && n.b==0.2f);
        assert(nodes[side][0].alpha==37 && nodes[side][1].alpha==100);
        draw(false);for(const auto& n:nodes[side])assert(!n.visible);
        nf::delta(side,1001);draw(); // Recovered positive gauge restores original tint.
        assert(nodes[side][2].texture==textures[1] && nodes[side][2].visible);
        for(const auto& n:nodes[side])assert(n.r==1 && n.g==1 && n.b==1);
        assert(nf::blocked(side));
        nf::delta(side,-10001);draw(); // 200%, highest digit needs full spacing.
        assert(nodes[side][0].x==100 && nodes[side][1].x-nodes[side][0].x==nodes[side][2].x-nodes[side][1].x);
        nf::delta(side,5000);draw(); // 100% restores the native narrow '1'.
        assert(nodes[side][0].x==110 && nodes[side][0].texture==textures[1]);
    }
    std::puts("PASS: production hooks; SP/DP both sides, signed native judgement, permanent failure, automatic fallback, ordinary passthrough, external save veto, requests, graph coordinates, red numeric draw");
    for(auto it=nf::hooks.rbegin();it!=nf::hooks.rend();++it)it->reset();
    second.reset();first.reset();
    (void)native_counts;
}
