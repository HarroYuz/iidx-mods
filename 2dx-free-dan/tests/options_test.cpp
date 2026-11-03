#include "../src/dan_options.hpp"
#include <cassert>
#include <cstdio>
using namespace self_dan;
int main(){
    for(int mode:{0,1,2,4,5,6,7,8,100,101})
        for(int id=0;id<10;++id)assert(chart_menu_mode(mode,id)==mode);
    for(int id=0;id<10;++id)assert(chart_menu_mode(3,id)==(id==1?3:0));
    assert(random_cycle_mode(3)==0 && random_cycle_mode(6)==6);
    assert(result_option_font(39,"0.85x [NO SAVE]",[](int id,const char*){return id==39?10.f:0.f;})==39);
    assert(result_option_font(39,"0.85x [NO SAVE]",[](int,const char*){return 0.f;})==-1);
    assert(result_option_font(38,"MIRROR",[](int id,const char*){return id==38?10.f:0.f;})==38);
    assert(result_option_font(39,"MIRROR",[](int id,const char*){return id==8?10.f:0.f;})==-1);
    DanOptions color;
    assert(color.label(0).empty() && color.label(1).empty());
    assert(result_option_spans(color,0,false).empty());
    assert(result_option_spans(color,1,false).empty());
    color.random[0]=1;assert(color.label(1)=="1P:RANDOM");
    color={};color.mirror[1]=1;assert(color.label(1)=="2P:MIRROR");
    color={};color.mirror[0]=1;
    auto spans=result_option_spans(color,0,false);
    assert(spans.size()==1 && !spans[0].red && spans[0].text=="OPTION: MIRROR");
    color.rate=85;spans=result_option_spans(color,0,true);
    assert(spans.size()==2 && spans[0].text=="OPTION: MIRROR " && !spans[0].red);
    assert(spans[1].text=="0.85x [NO SAVE]" && spans[1].red);
    color={};color.mirror[0]=color.mirror[1]=color.flip=1;
    spans=result_option_spans(color,1,false);assert(spans.size()==1 && !spans[0].red);
    color.mirror[1]=0;spans=result_option_spans(color,1,true);
    bool red_mirror=false,red_flip=false;
    for(const auto& part:spans){
        if(part.text.find("MIRROR")!=std::string::npos)red_mirror=part.red;
        if(part.text.find("FLIP")!=std::string::npos)red_flip=part.red;
    }
    assert(red_mirror && red_flip);
    color={};spans=result_option_spans(color,0,true);
    assert(spans.size()==1 && spans[0].red && spans[0].text=="[NO SAVE]"); // Earlier song tainted course.
    color.random[0]=1;color.autoscratch=color.legacy=1;color.all_scratch=true;
    spans=result_option_spans(color,0,true);
    for(const auto& part:spans)if(part.text.find_first_not_of(" /")!=std::string::npos && part.text!="OPTION: ")assert(part.red);
    DanOptionHistory h;DanOptionHistory::Stage s;s.joined[0]=true;
    h.begin(false);h.capture(0,s);assert(!h.blocked());
    s.side[0].mirror[0]=1;h.capture(1,s);assert(!h.blocked());
    s.style=1;s.side[0].mirror[1]=1;s.side[0].flip=1;
    h.capture(2,s);assert(!h.blocked()); // Native DP MIRROR.
    s.side[0].mirror[1]=0;h.capture(3,s);assert(h.blocked());
    assert(h.stages()[0].side[0].label(0)=="");
    assert(h.stages()[2].side[0].label(1)=="MIRROR/MIRROR FLIP");
    h.leave();assert(!h.active() && !h.blocked());
    h.begin(false);s={};s.joined[1]=true;s.side[1].random[0]=3;
    s.side[1].autoscratch=1;s.side[1].legacy=1;h.capture(0,s);
    assert(h.blocked() && h.stages()[0].side[1].label(0)=="S-RANDOM A-SCR LEGACY");
    s.side[1]={};h.capture(1,s);assert(h.blocked()); // Never clear course taint.
    h.begin(false);h.capture(0,s);h.practice(0,80,true);
    assert(h.blocked() && h.stages()[0].side[1].label(0)=="ALL-SCR 0.80x");
    h.capture(1,s);h.practice(1,100,false);assert(h.blocked());
    assert(h.stages()[0].side[1].rate==80 && h.stages()[1].side[1].rate==100);
    h.capture(-1,s);h.capture(4,s);h.practice(3,150,true);
    assert(!h.stages()[3].known);
    h.begin(true);assert(h.blocked());
    h.begin(false);s.side[0].random[0]=1;h.capture(0,s);
    assert(!h.blocked()); // An unjoined player's stored options do not count.
    std::puts("PASS: Dan menu scope, native MIRROR, per-stage labels, course-wide taint and practice history");
}
