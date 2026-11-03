#pragma once
#include <array>
#include <string>
#include <cstdio>
#include <vector>

namespace self_dan {
// Native basic option IDs: RANDOM=0/5, GAUGE=1, ASSIST=2, FLIP=3.
// Leave gauge and all non-Dan eligibility decisions with the original menu.
inline int chart_menu_mode(int mode,int option) {
    return mode==3 && option!=1 ? 0 : mode;
}
inline int random_cycle_mode(int mode) {return mode==3?0:mode;}
struct DanOptions {
    int random[2]{},mirror[2]{};
    int autoscratch{},legacy{},flip{},battle{};
    int rate=100;
    bool all_scratch=false;
    bool extended(int style) const {
        if(random[0] || (style==1 && random[1]) || autoscratch || legacy ||
           battle || rate!=100 || all_scratch)return true;
        // The native DP MIRROR shortcut applies MIRROR to both lanes + FLIP.
        return style==1 && !(mirror[0]==mirror[1] && flip==mirror[0]);
    }
    std::string label(int style) const {
        auto lane=[&](int i) {
            const char* names[]{"","RANDOM","R-RANDOM","S-RANDOM"};
            std::string s=random[i]>=0 && random[i]<4?names[random[i]]:"UNKNOWN";
            if(mirror[i])s=random[i]?s+"+MIRROR":"MIRROR";
            return s;
        };
        std::string s=lane(0);
        if(style==1){
            const auto right=lane(1);
            if(!s.empty() && !right.empty())s+="/"+right;
            else if(!s.empty())s="1P:"+s;
            else if(!right.empty())s="2P:"+right;
        }
        const auto append=[&](const std::string& text){if(!s.empty())s+=' ';s+=text;};
        if(flip && style==1)append("FLIP");
        if(battle)append("BATTLE");
        if(autoscratch)append("A-SCR");
        if(legacy)append("LEGACY");
        if(all_scratch)append("ALL-SCR");
        if(rate!=100){
            char b[24];std::snprintf(b,sizeof(b),"%d.%02dx",rate/100,rate%100);append(b);
        }
        return s;
    }
};
// Keep the exact song-result font. Substituting a title font changes the
// letterforms and can hide missing glyphs; incomplete native fonts are logged.
template<class Measure> int result_option_font(int native_font,const std::string& label,Measure measure) {
    for(unsigned char c:label) {
        if(c==' ')continue;
        char glyph[]{char(c),0};
        if(!(measure(native_font,glyph)>0.f))return -1;
    }
    return native_font;
}
struct OptionSpan {std::string text;bool red=false;};
inline std::vector<OptionSpan> result_option_spans(const DanOptions& options,int style,bool no_save){
    const auto label=options.label(style);
    if(label.empty())return no_save?std::vector<OptionSpan>{{"[NO SAVE]",true}}:std::vector<OptionSpan>{};
    std::vector<OptionSpan> spans{{"OPTION: ",false}};
    const bool native_mirror=style!=1 ||
        (options.mirror[0]==options.mirror[1] && options.flip==options.mirror[0]);
    const auto append=[&](const std::string& text,bool red){
        if(spans.back().red==red)spans.back().text+=text;
        else spans.push_back({text,red});
    };
    for(size_t start=0;start<label.size();){
        if(label[start]==' ' || label[start]=='/'){
            append(label.substr(start++,1),false);continue;
        }
        auto end=label.find_first_of(" /",start);
        if(end==std::string::npos)end=label.size();
        const auto token=label.substr(start,end-start);
        const auto option=token.rfind("1P:",0)==0 || token.rfind("2P:",0)==0?token.substr(3):token;
        const bool safe=(option=="MIRROR" || option=="FLIP") && native_mirror;
        append(token,!safe);start=end;
    }
    if(no_save)append(" [NO SAVE]",true);
    return spans;
}
class DanOptionHistory {
public:
    struct Stage {bool known=false;int style{};std::array<bool,2> joined{};std::array<DanOptions,2> side{};};
    void begin(bool custom){*this=DanOptionHistory{};active_=true;blocked_=custom;}
    void capture(int index,const Stage& stage) {
        if(!active_ || index<0 || index>=4)return;
        stages_[index]=stage;stages_[index].known=true;
        for(int side=0;side<2;++side)if(stage.joined[side])
            blocked_|=stage.side[side].extended(stage.style);
    }
    void practice(int index,int rate,bool scratch) {
        if(!active_ || index<0 || index>=4 || !stages_[index].known)return;
        auto& stage=stages_[index];
        for(auto& options:stage.side){options.rate=rate;options.all_scratch=scratch;}
        blocked_|=rate!=100 || scratch;
    }
    bool active()const{return active_;}
    bool blocked()const{return active_ && blocked_;}
    const std::array<Stage,4>& stages()const{return stages_;}
    void leave(){*this=DanOptionHistory{};}
private:
    bool active_=false,blocked_=false;
    std::array<Stage,4> stages_{};
};
}
