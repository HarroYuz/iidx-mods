#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <safetyhook.hpp>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include "policy.hpp"
#include "../../2dx-practice/src/addresses.hpp"
#include "../../2dx-practice/src/request_policy.hpp"
#include "../../2dx-practice/src/native_hooks.hpp"

namespace nf {
uintptr_t base{};
template<class T>T& at(uintptr_t r){return *reinterpret_cast<T*>(base+r);}
template<class T>T& field(void* p,size_t n){return *reinterpret_cast<T*>(static_cast<char*>(p)+n);}
template<class F>F fn(uintptr_t r){return reinterpret_cast<F>(base+r);}
template<class F>F proc(HMODULE module,const char* name){auto p=GetProcAddress(module,name);F f{};std::memcpy(&f,&p,sizeof f);return f;}
void log(const char* s){if(auto f=std::fopen("no-fail-mod/runtime.log","a")){std::fprintf(f,"%lu %s\n",GetTickCount(),s);std::fclose(f);}}
using no_fail::Gauge;
struct Death {bool recorded=false;int measure=0,notes=0;};
struct Side {
    no_fail::Player policy;
    std::array<std::array<int64_t,384>,5> history{};
    std::array<std::array<int32_t,4>,5> judgements{};
    bool active=false,terminated=false;
    bool dan=false,dan_started=false;
    no_fail::Track dan_track;
    std::array<Death,5> deaths{};
    Death dan_death;
    unsigned view=0;
};
Side sides[2];
bool initialized=false, in_simulation=false, in_result=false;
thread_local bool signed_plot=false;
std::atomic<bool> installed{false},attempted{false};
std::atomic<unsigned> active_mask{0},blocked_mask{0};
std::atomic<bool> dan_failed{false};
bool last_graph_input=false;
std::vector<SafetyHookInline> hooks;
SafetyHookInline *chart_hook{},*delta_hook{},*hold_hook{},*graph_hook{},*result_hook{},*plot_hook{},*coord_hook{},*guard_hook{},*clear_hook{},*request_hook{},*number_hook{};
SafetyHookInline* background_hook{};
SafetyHookInline *history_hook{},*graph_value_hook{};
SafetyHookInline* dan_row_hook{};
SafetyHookInline *dead_init_hook{},*dead_render_hook{},*dead_notes_hook{};
SafetyHookInline* death_hook{};
SafetyHookInline* set_gauge_hook{};
thread_local bool showing_dead=false;
void(*native_update)(int,int){};
void* native_update_code{};
std::vector<practice_network::Entry> requests;
unsigned type(int side){return fn<unsigned(*)(void*,int,int)>(0x8829b0)(at<void*>(0xb3e9f28),at<int>(0xb60b3a4)?0:side,at<int>(0xb60b3a4));}
void set_type(int side,unsigned g){fn<void*(*)(void*,int,unsigned,int)>(0x885ff0)(at<void*>(0xb3e9f28),at<int>(0xb60b3a4)?0:side,g,at<int>(0xb60b3a4));}
int32_t& raw(int side){return at<int32_t>(0xbcbb2c0+20*side);}
int native_value(int64_t v){return static_cast<int>(std::clamp<int64_t>(v,INT32_MIN,5000));}
bool mode_ok(){
    const int m=at<int>(0xb60b3a0);auto options=at<void*>(0xb3e9f28);
    if(!options || m==4 || m==100)return false;
    if(m==3)return true;
    for(int s=0;s<2;++s)if(fn<bool(*)(void*,int,int)>(0x884a00)(options,s,0))return false;
    return !fn<bool(*)(void*,int,int)>(0x884a00)(options,0,1);
}
bool enabled_provider(const wchar_t* name){
    auto m=GetModuleHandleW(name);if(!m)return false;
    auto q=proc<bool(*)()>(m,"is_enabled");return q && q();
}
unsigned bottom(bool fire){
    const bool dp=at<int>(0xb60b3a4)!=0;
    if(fire){auto m=reinterpret_cast<uintptr_t>(GetModuleHandleW(L"firenet.dll"));auto v=*reinterpret_cast<uint8_t*>(m+0x13dec8+(dp?1:0));if(v<3)return v;}
    char value[64]{};GetPrivateProfileStringA("Gauge",dp?"Bottom Shiftable Gauge (DP)":"Bottom Shiftable Gauge","ASSISTED EASY",value,sizeof(value),".\\2dx-gsm.ini");
    return !std::strcmp(value,"NORMAL")?0:!std::strcmp(value,"EASY")?2:1;
}
void publish_value(int side,int value){
    raw(side)=value;at<int32_t>(0xbcbd3f0+4*side)=value;
    if(auto object=at<void*>(0xbcb66c8+16*side))field<int32_t>(object,0)=value;
}
Death& death(int side,unsigned gauge){return sides[side].dan?sides[side].dan_death:sides[side].deaths[gauge];}
void record_death(int side,unsigned gauge,bool failed){
    auto& d=death(side,gauge);if(!failed || d.recorded)return;
    d.recorded=true;
    d.measure=field<int>(fn<void*(*)()>(0x815d40)(),0x64);
    d.notes=fn<int(*)(void*,int)>(0x8e8f70)(fn<void*(*)()>(0x8e94f0)(),side);
    char message[192]{};
    const auto value=sides[side].dan?sides[side].dan_track.value:sides[side].policy.tracks[gauge].value;
    std::snprintf(message,sizeof message,"gauge depleted: side=%d dan=%d gauge=%u raw=%lld measure=%d notes=%d",
        side,int(sides[side].dan),gauge,static_cast<long long>(value),d.measure,d.notes);
    log(message);
}
void publish_death(int side){
    if(!initialized || side<0 || side>1 || !sides[side].active)return;
    const auto g=no_fail::index(sides[side].policy.display());
    if(!sides[side].dan && g<3)return;
    auto& d=death(side,g);at<int>(0xb4dd734+side*4)=d.recorded?d.measure:0;
}
void native_death(void* manager,int side,bool failed){
    death_hook->call<void>(manager,side,failed);publish_death(side);
}
bool blocked(int side){
    if(side<0 || side>1 || !initialized || !sides[side].active)return false;
    if(sides[side].terminated)return true;
    if(sides[side].dan)return sides[side].dan_track.failed;
    const auto& p=sides[side].policy;
    if(!no_fail::hard(p.starting))return false;
    return p.automatic?!p.best_clear():p.tracks[no_fail::index(p.starting)].failed;
}
// A failed lamp does not invalidate a fixed-gauge music score. Keep course
// and automatic-gauge save policy separate from the permanent clear latch.
bool save_blocked(int side){return blocked(side) && (sides[side].terminated || sides[side].dan || sides[side].policy.automatic);}
void publish_save_state(){unsigned active=0,invalid=0;bool course_failed=false;for(int s=0;s<2;++s)if(sides[s].active){active|=1u<<s;if(save_blocked(s)){invalid|=1u<<s;course_failed|=sides[s].dan;}}blocked_mask=invalid;active_mask=active;dan_failed=course_failed;}
bool all_blocked(){const unsigned active=active_mask.load();return dan_failed.load() || (active && (blocked_mask.load()&active)==active);}
int set_gauge(int side,int value){
    const int result=set_gauge_hook->call<int>(side,value);
    // 0917's two early-exit paths call this absolute setter with zero for
    // both sides. It is not the judgement delta or chart initialization.
    // The independent simulation must not turn that forced failure into a
    // surviving H/EXH (or a full ordinary gauge) at result/save time.
    if(initialized && side>=0 && side<2 && sides[side].active && value==0){
        sides[side].terminated=true;
        publish_value(side,0);publish_save_state();
        log("native stage termination: clear and save blocked until chart reset");
    }
    return result;
}
void choose(int side){
    if(sides[side].dan)return;
    auto& p=sides[side].policy;auto g=p.display();set_type(side,no_fail::index(g));
    at<unsigned>(side?0xb11faec:0xb11fab4)=no_fail::index(g);
    std::memcpy(&at<int32_t>(0xbcbb2c4+20*side),sides[side].judgements[no_fail::index(g)].data(),16);
    publish_value(side,sides[side].terminated?0:native_value(p.tracks[no_fail::index(g)].value));
    publish_death(side);
}
void delta(int side,int amount){
    if(side<0 || side>1 || !initialized || !sides[side].active || !mode_ok())return delta_hook->call<void>(side,amount);
    auto& s=sides[side];
    if(s.terminated)return;
    if(s.dan){
        // Native stage preparation applies the preceding song's carry after
        // chart initialization. Read it at the first actual gauge update.
        if(!s.dan_started){s.dan_track.value=raw(side);s.dan_started=true;}
        int maximum=5000;
        auto course=fn<void*(*)()>(0x86fab0)();
        if(field<unsigned char>(course,9))maximum=50*fn<int(*)(void*,int,bool)>(0x863340)(course,fn<int(*)(void*)>(0x863ee0)(course),true);
        if(auto object=at<void*>(0xbcb66c8+16*side))if(field<int>(object,0x10)>0)maximum=std::min(maximum,field<int>(object,0x10));
        s.dan_track.apply(Gauge::hard,amount);
        s.dan_track.value=std::min<int64_t>(maximum,s.dan_track.value);
        record_death(side,0,s.dan_track.failed);
        publish_value(side,native_value(s.dan_track.value));publish_death(side);publish_save_state();return;
    }
    const unsigned g=type(side);
    if(g!=3 && g!=4)return delta_hook->call<void>(side,amount);
    auto& track=sides[side].policy.tracks[g];track.apply(static_cast<Gauge>(g),amount);
    record_death(side,g,track.failed);
    publish_value(side,native_value(track.value));
    if(!in_simulation)publish_death(side);
}
template<class Call>void calculate(int side,Call&& call){
    auto& s=sides[side];const unsigned g=type(side);
    if(raw(side)>0 || (!s.dan && g!=3)){call();return;}
    if(s.dan && !s.dan_started){s.dan_track.value=raw(side);s.dan_started=true;}
    // The native gauge-object callback may treat nonpositive/unsigned health
    // as outside its live low-health range. Compute at its lowest positive
    // input, then apply the native delta to our independent signed track.
    // This preserves native rounding and avoids applying compensation twice.
    publish_value(side,1);call();
    publish_value(side,native_value(s.dan?s.dan_track.value:s.policy.tracks[g].value));
}
template<class Call>void simulate(int side,Call&& call){
    if(side<0 || side>1 || !initialized || !sides[side].active || !mode_ok() || in_simulation){call();return;}
    auto& s=sides[side];auto& p=s.policy;
    if(s.terminated){call();publish_value(side,0);return;}
    if(s.dan || !p.automatic){calculate(side,call);if(!s.dan && !no_fail::hard(p.starting))p.tracks[no_fail::index(p.starting)].value=raw(side);publish_save_state();return;}
    in_simulation=true;
    for(unsigned g=0;g<5;++g){
        set_type(side,g);publish_value(side,native_value(p.tracks[g].value));
        std::memcpy(&at<int32_t>(0xbcbb2c4+20*side),s.judgements[g].data(),16);
        calculate(side,call);if(g<3)p.tracks[g].value=raw(side);
    }
    in_simulation=false;choose(side);publish_save_state();
}
void update(int side,int judge){simulate(side,[&]{native_update(side,judge);});}
void hold(int side,double amount,bool reduce){simulate(side,[&]{hold_hook->call<void>(side,amount,reduce);});}
void* chart(int64_t left,int64_t right){
    initialized=false;in_result=false;last_graph_input=false;
    auto result=chart_hook->call<void*>(left,right);
    const bool allowed=mode_ok();
    const bool dan=allowed && at<int>(0xb60b3a0)==3;
    const bool fire=allowed && !dan && enabled_provider(L"firenet.dll");
    const bool auto_gauge=fire || (allowed && !dan && enabled_provider(L"2dx-gsm.dll"));
    for(int side=0;side<2;++side){
        auto& s=sides[side];s={};
        const unsigned selected=allowed?type(side):0;s.active=allowed && selected<5 && at<int>(0xb60b3b0+side*4)==1;
        if(!s.active)continue;
        s.policy.reset(static_cast<Gauge>(selected),static_cast<Gauge>(bottom(fire)),auto_gauge);
        s.view=selected;
        s.dan=dan;
        if(dan){s.dan_track.value=raw(side);for(auto& h:s.history)h.fill(raw(side));continue;}
        for(unsigned g=0;g<5;++g){
            set_type(side,g);
            for(int j=0;j<4;++j)s.judgements[g][j]=static_cast<int32_t>(fn<int64_t(*)(int,int,int)>(0x95b4a0)(side,j,static_cast<int>(side?right:left)));
            s.history[g].fill(g<3?1100:5000);
        }
        set_type(side,selected);s.policy.tracks[selected].value=raw(side);
        std::memcpy(&at<int32_t>(0xbcbb2c4+20*side),s.judgements[selected].data(),16);
    }
    initialized=true;publish_save_state();log(auto_gauge?"stage reset: automatic gauges":"stage reset: fixed gauge");return result;
}
void* graph(void* object,int16_t index,int16_t value){
    auto r=graph_hook->call<void*>(object,index,static_cast<int16_t>(std::max(0,int(value))));
    if(!initialized)return r;
    const int sample=at<int16_t>(0xb123870+874);
    if(sample<0 || sample>=384)return r;
    for(int s=0;s<2;++s)if(sides[s].active){
        publish_death(s);
        for(unsigned g=0;g<5;++g)sides[s].history[g][sample]=sides[s].dan?(sides[s].dan_started?sides[s].dan_track.value:raw(s)):sides[s].policy.tracks[g].value;
        // Providers' legacy graph exports remain unsigned-safe. Only the
        // dedicated display buffer receives signed values below.
        if(!sides[s].dan)for(const auto name:{L"2dx-gsm.dll",L"firenet.dll"})if(auto m=GetModuleHandleW(name)){
            auto get=proc<void*(*)(uint8_t,uint8_t)>(m,"get_graph_points");
            if(get)for(unsigned g=0;g<5;++g)if(auto p=static_cast<int16_t*>(get(static_cast<uint8_t>(g),static_cast<uint8_t>(s+1))))p[sample]=static_cast<int16_t>(std::clamp<int64_t>(sides[s].history[g][sample],0,5000));
        }
    }
    return r;
}
void prepare_result_view(){
    if(!in_result)for(auto& s:sides)if(s.active && !s.dan)s.view=no_fail::index(s.policy.display());
    in_result=true;
    const unsigned input=at<unsigned>(0xbcce288);
    const bool pressed=(input&(1u<<19))!=0;
    if(pressed && !last_graph_input)for(auto& side:sides)if(side.active && side.policy.automatic)side.view=(side.view+1)%5;
    last_graph_input=pressed;
}
void dead_init(void* object,void* root){
    if(!initialized)return dead_init_hook->call<void>(object,root);
    std::array<unsigned char,2> saved{};
    for(int s=0;s<2;++s){saved[s]=at<unsigned char>(0xb4dd620+s);if(sides[s].active)at<unsigned char>(0xb4dd620+s)=1;}
    // Allocate both joined players' native widgets, including a failed gauge
    // hidden behind an automatic clear. Never leave gameplay death flags set.
    dead_init_hook->call<void>(object,root);
    for(int s=0;s<2;++s)at<unsigned char>(0xb4dd620+s)=saved[s];
}
int dead_notes(void* manager,int side){
    if(showing_dead && side>=0 && side<2 && sides[side].active){auto& d=death(side,sides[side].view);if(d.recorded)return d.notes;}
    return dead_notes_hook->call<int>(manager,side);
}
void dead_render(void* object){
    if(!initialized)return dead_render_hook->call<void>(object);
    prepare_result_view();
    std::array<void*,2> roots{};std::array<int,2> measures{};
    for(int s=0;s<2;++s){
        roots[s]=field<void*>(object,8+s*8);measures[s]=at<int>(0xb4dd734+s*4);
        if(!sides[s].active)continue;
        auto& d=death(s,sides[s].view);
        const bool survival=sides[s].dan || sides[s].view>=3;
        const bool visible=survival?d.recorded:fn<bool(*)(void*,int)>(0x8e9220)(fn<void*(*)()>(0x8e94f0)(),s);
        if(roots[s])reinterpret_cast<void(*)(void*,bool)>((*static_cast<void***>(roots[s]))[5])(roots[s],visible);
        if(!visible)field<void*>(object,8+s*8)=nullptr;
        if(d.recorded)at<int>(0xb4dd734+s*4)=d.measure;
    }
    showing_dead=true;dead_render_hook->call<void>(object);showing_dead=false;
    for(int s=0;s<2;++s){field<void*>(object,8+s*8)=roots[s];at<int>(0xb4dd734+s*4)=measures[s];}
}
void background(void* object,void* root,int side){
    if(!initialized || side<0 || side>1 || !sides[side].active)return background_hook->call<void>(object,root,side);
    prepare_result_view();
    const auto saved=type(side);set_type(side,sides[side].view);
    // Native graph_bg texture selection is a separate callback from the line
    // renderer. Both must use the same view for this frame.
    background_hook->call<void>(object,root,side);
    set_type(side,saved);
}
void* result(void* object){
    if(!initialized)return result_hook->call<void*>(object);
    prepare_result_view();
    const bool pressed=(at<unsigned>(0xbcce288)&(1u<<19))!=0;
    // Own graph browsing once even when both GSM providers are loaded.
    at<unsigned>(0xbcce288)&=~(1u<<19);
    for(int s=0;s<2;++s)if(sides[s].active)set_type(s,sides[s].view);
    const int original_state=field<int>(object,0x1818);field<int>(object,0x1818)=1;
    auto r=result_hook->call<void*>(object);
    field<int>(object,0x1818)=original_state;
    if(pressed)at<unsigned>(0xbcce288)|=1u<<19;
    for(int s=0;s<2;++s)if(sides[s].active)choose(s);
    return r;
}
void plot(void* object,int side,bool alternate,void* ghost){
    const int kind=field<int>(object,0xb8);
    if(kind==1 || kind==2){
        // Subscreen history belongs to the selected song, not necessarily the
        // live stage. Its signed samples are captured when the record is saved.
        const bool previous=signed_plot;signed_plot=true;
        plot_hook->call<void>(object,side,alternate,ghost);signed_plot=previous;return;
    }
    if(!initialized || !in_result || side<0 || side>1 || !sides[side].active)return plot_hook->call<void>(object,side,alternate,ghost);
    const auto g=std::min(type(side),4u);
    auto* points=reinterpret_cast<int32_t*>(static_cast<char*>(object)+0xc8+side*384*4);
    std::array<int32_t,384> saved{};std::memcpy(saved.data(),points,sizeof(saved));
    for(int i=0;i<384;++i)points[i]=native_value(sides[side].history[g][i]);
    signed_plot=g>=3 || sides[side].dan;plot_hook->call<void>(object,side,alternate,ghost);signed_plot=false;
    std::memcpy(points,saved.data(),sizeof(saved));
}
int coordinate(void* object,int value,int maximum){
    if(!signed_plot || value>=0 || maximum!=5000)return coord_hook->call<int>(object,value,maximum);
    const int kind=field<int>(object,0xb8);
    if(kind<0 || kind>2)return coord_hook->call<int>(object,value,maximum);
    const int top=at<int>(0xd8ab28+kind*4),height=at<int>(0xd8ab48+kind*4);
    // Subscreen coordinates start below 1080 (native tops: 1305 / 1310).
    // Applying the main-screen limit moves every negative sample ABOVE it.
    const int limit=kind==0?1080:INT32_MAX;
    return static_cast<int>(std::clamp<int64_t>(top+(int64_t(maximum)-value)*height/maximum,0,limit));
}
void history(void* state,const void* points,int bytes){
    if(initialized && bytes==384*int(sizeof(int32_t))){
        auto manager=fn<void*(*)()>(0x871930)();
        for(int s=0;s<2;++s)if(sides[s].active && state==fn<void*(*)(void*,uint64_t)>(0x9e5950)(manager,(uint64_t(1)<<32)|unsigned(s))){
            const auto g=no_fail::index(sides[s].policy.display());
            std::array<int32_t,384> signed_points{};
            for(int i=0;i<384;++i)signed_points[i]=native_value(sides[s].history[g][i]);
            // This is local display history, separate from the unsigned ghost
            // sent to the server. Store the final best lamp, not a browsed view.
            history_hook->call<void>(state,signed_points.data(),bytes);return;
        }
    }
    history_hook->call<void>(state,points,bytes);
}
int graph_value(void* object,int side,int x){
    const int kind=field<int>(object,0xb8);
    if((kind==1 || kind==2) && side>=0 && side<2){
        const int count=field<int>(object,0xc0+side*4),width=at<int>(0xd8ab38+kind*4);
        if(count>0 && count<=384 && width>0){
            const int sample=std::min(count-1,int(int64_t(count)*std::clamp(x,0,width)/width));
            const int value=field<int>(object,0xc8+(side*384+sample)*4);
            // The native tooltip already formats a signed decimal with %%.
            if(value<0)return -int((-(int64_t(value))+49)/50);
            // Native lookup indexes count at the right edge; use the last
            // actual sample there, keeping its ordinary percentage rounding.
            if(x>=width)return fn<int(*)(int)>(0x95b840)(std::clamp(value,0,5000));
        }
    }
    return graph_value_hook->call<int>(object,side,x);
}
bool invalid_record(int side,bool assist){return save_blocked(side)||guard_hook->call<bool>(side,assist);}
int clear(int side){
    if(side==0 || side==1){
        if(blocked(side))return 0;
        if(initialized && sides[side].active && sides[side].policy.automatic)return sides[side].policy.best_clear()?1:0;
        return clear_hook->call<int>(side);
    }
    // Native callers use -1 (including Dan's next-scene decision), not just 2.
    if(initialized)for(int s=0;s<2;++s)if(sides[s].dan && blocked(s))return 0;
    return clear(0)+clear(1);
}
void dan_row(int side,int stage,const void* row){
    if(side<0 || side>1 || !initialized || !sides[side].dan || !blocked(side))return dan_row_hook->call<void>(side,stage,row);
    // Native Dan builds this flag from the *final rounded percentage*, not
    // clear(). A recovered failed gauge must not become a cleared course row.
    std::array<unsigned char,32> failed{};std::memcpy(failed.data(),row,failed.size());
    if(failed[28] && at<int>(0xb3e82a8+side*4)>0)--at<int>(0xb3e82a8+side*4);
    failed[28]=0;dan_row_hook->call<void>(side,stage,failed.data());
}
int request(void* manager,int* handle,int id,void* input,void* output){
    auto e=practice_network::find(requests,id);
    if(all_blocked() && e && practice_network::is_write(*e)){log("failed no-fail score request blocked");return 3;}
    return request_hook->call<int>(manager,handle,id,input,output);
}
thread_local bool red_digits=false;
struct DigitLayout {void* root{};int original_x=0,last_x=0;};
DigitLayout digit_layout[2];
SafetyHookInline* tint_hook{};
void tint(void* node,int alpha,float red,float green,float blue){
    if(red_digits){red=1.0f;green=blue=0.2f;}
    tint_hook->call<void>(node,alpha,red,green,blue);
}
void number(int value,int digits,void* root,const char** names,const char** textures,bool show,bool zeros,bool align,int alpha){
    const int side=names==reinterpret_cast<const char**>(base+0x103e960)?0:names==reinterpret_cast<const char**>(base+0x103e978)?1:-1;
    const bool managed=side>=0 && initialized && sides[side].active;
    const bool negative=managed && raw(side)<0;
    if(negative){
        const auto current=sides[side].dan?sides[side].dan_track.value:sides[side].policy.tracks[std::min(type(side),4u)].value;
        const auto magnitude=uint64_t(-(current+1))+1;
        value=static_cast<int>(std::min<uint64_t>(INT_MAX,magnitude/50+(magnitude%50!=0)));
    }else if(managed && (type(side)>=3 || sides[side].dan)){
        // Match native gameplay/result rounding, including positive remainders
        // below 2% that display as zero and already count as depleted.
        value=fn<int(*)(int)>(0x95b840)(raw(side));
    }
    // Keep native digit textures, alignment, leading-zero visibility and alpha.
    // Negative gauge is distinguished only by red tint, with no minus glyph.
    const bool previous=red_digits;red_digits=negative;
    number_hook->call<void>(value,digits,root,names,textures,show,zeros,align,alpha);
    red_digits=previous;
    if(managed && root && digits==3){
        auto find=fn<void*(*)(void*,const char*)>(0x32d1a0);
        auto x=fn<int(*)(void*)>(0x32ade0);
        auto middle=find(root,names[1]);if(!middle)return;const int middle_x=x(middle);
        auto last=find(root,names[2]);if(!last)return;const int last_x=x(last);
        auto high=find(root,names[0]);if(!high)return;const int high_x=x(high);
        auto& layout=digit_layout[side];
        if(layout.root!=root || high_x!=layout.last_x){layout.root=root;layout.original_x=high_x;}
        layout.last_x=negative && value>=200?2*middle_x-last_x:layout.original_x;
        if(high_x!=layout.last_x)fn<void(*)(void*,int,int)>(0x32a2c0)(high,layout.last_x,fn<int(*)(void*)>(0x32ae20)(high));
    }
}
SafetyHookInline* add(uintptr_t r,void* replacement){
    hooks.push_back(safetyhook::create_inline(reinterpret_cast<void*>(base+r),replacement));
    if(!hooks.back())throw std::runtime_error("hook installation failed");
    return &hooks.back();
}
void install(){
    hooks.reserve(32);
    // Both GSM implementations replace only the first seven native bytes.
    // Construct the unchanged single-gauge entry so automatic simulation is
    // performed once, rather than recursively simulating 5 x 5 gauges.
    const unsigned char tail[]={0x83,0x3d,0xca,0x17,0x36,0x0b,0x00,0x48,0x63,0xda,0x48,0x63,0xe9};
    if(std::memcmp(reinterpret_cast<void*>(base+0x95bc27),tail,sizeof tail))throw std::runtime_error("native judgement body changed");
    native_update_code=VirtualAlloc(nullptr,64,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!native_update_code)throw std::runtime_error("trampoline allocation failed");
    unsigned char code[21]={0x40,0x53,0x55,0x48,0x83,0xec,0x28,0xff,0x25,0,0,0,0};
    const uintptr_t continuation=base+0x95bc27;std::memcpy(code+13,&continuation,8);std::memcpy(native_update_code,code,sizeof code);
    DWORD old{};if(!VirtualProtect(native_update_code,64,PAGE_EXECUTE_READ,&old))throw std::runtime_error("trampoline protection failed");
    FlushInstructionCache(GetCurrentProcess(),native_update_code,sizeof code);native_update=reinterpret_cast<void(*)(int,int)>(native_update_code);
    const auto& layout=addresses::layout();requests.resize(layout.request_count);
    std::memcpy(requests.data(),reinterpret_cast<void*>(base+layout.request_table),requests.size()*sizeof(requests[0]));
    if(!practice_network::valid(requests))throw std::runtime_error("request table changed");
    chart_hook=add(0x95d060,reinterpret_cast<void*>(&chart));
    set_gauge_hook=add(0x95d020,reinterpret_cast<void*>(&set_gauge));
    delta_hook=add(0x95cee0,reinterpret_cast<void*>(&delta));
    hold_hook=add(0x95be70,reinterpret_cast<void*>(&hold));
    graph_hook=add(0x80a300,reinterpret_cast<void*>(&graph));
    result_hook=add(0x90b3a0,reinterpret_cast<void*>(&result));
    background_hook=add(0x909360,reinterpret_cast<void*>(&background));
    dead_init_hook=add(0x907660,reinterpret_cast<void*>(&dead_init));
    dead_render_hook=add(0x907710,reinterpret_cast<void*>(&dead_render));
    dead_notes_hook=add(0x8e8f70,reinterpret_cast<void*>(&dead_notes));
    death_hook=add(0x8e9400,reinterpret_cast<void*>(&native_death));
    plot_hook=add(0x9b65a0,reinterpret_cast<void*>(&plot));
    coord_hook=add(0x9b6b40,reinterpret_cast<void*>(&coordinate));
    history_hook=add(0x9e60f0,reinterpret_cast<void*>(&history));
    graph_value_hook=add(0x9b6ab0,reinterpret_cast<void*>(&graph_value));
    guard_hook=add(addresses::rva(0x82d250),reinterpret_cast<void*>(&invalid_record));
    clear_hook=add(0x95c2e0,reinterpret_cast<void*>(&clear));
    dan_row_hook=add(0x842370,reinterpret_cast<void*>(&dan_row));
    request_hook=add(addresses::rva(0xab7b80),reinterpret_cast<void*>(&request));
    tint_hook=add(0x32a790,reinterpret_cast<void*>(&tint));
    number_hook=add(0x81ff60,reinterpret_cast<void*>(&number));
    add(0x95bc20,reinterpret_cast<void*>(&update));
    installed=true;log("no-fail runtime installed (0917)");
}
} // namespace nf

extern "C" __declspec(dllexport) unsigned NoFailStatusV1(){return nf::installed?1u:nf::attempted?2u:0u;}
extern "C" __declspec(dllexport) int64_t NoFailGaugeV1(unsigned side,unsigned gauge){
    return side<2 && gauge<5 && nf::initialized?nf::sides[side].policy.tracks[gauge].value:INT64_MIN;
}

#ifndef NO_FAIL_TEST
namespace {
using Update=void(*)(void*,bool,void*);
self_dan::SlotHook slots[3];Update next[3];
template<int N>void bootstrap(void* scene,bool resumed,void* parameter){
    if(!nf::attempted.exchange(true)){try{nf::install();}catch(const std::exception& e){nf::log(e.what());for(auto it=nf::hooks.rbegin();it!=nf::hooks.rend();++it)it->reset();nf::log("no-fail disabled after installation failure");}}
    next[N](scene,resumed,parameter);
}
DWORD WINAPI worker(void*){
    CreateDirectoryW(L"no-fail-mod",nullptr);
    nf::base=reinterpret_cast<uintptr_t>(GetModuleHandleW(L"bm2dx.dll"));
    if(!nf::base || !addresses::select(nf::base) || !addresses::current){nf::log("unsupported game; no hooks installed");return 1;}
    const uintptr_t tables[]={0xd818d8,0xd82768,0xd7f898};
    void* wrappers[]={reinterpret_cast<void*>(&bootstrap<0>),reinterpret_cast<void*>(&bootstrap<1>),reinterpret_cast<void*>(&bootstrap<2>)};
    for(int i=0;i<3;++i){auto slot=reinterpret_cast<void**>(nf::base+addresses::rva(tables[i])+addresses::scene_slot(15)*8);next[i]=reinterpret_cast<Update>(*slot);slots[i]={slot,*slot,wrappers[i]};}
    if(self_dan::install_hooks(slots,3)!=self_dan::HookStatus::installed){nf::log("bootstrap conflict");return 2;}
    nf::log("bootstrap ready");return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE self,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(self);auto thread=CreateThread(nullptr,0,&worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;}
#endif
