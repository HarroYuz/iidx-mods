#pragma once
#include "native_selection.hpp"
#include <array>
#include <algorithm>
#include <optional>

namespace self_dan {
// State shared by the native scene wrappers. Scene progression, row creation,
// failure detection and attainment remain decisions of the original Dan code.
// No callback in this file writes to the game or submits a score.
class NativeSession {
public:
    enum class Phase { idle, entry_pending, selecting, selected, playing, stage_result, result, leaving, fault };
    struct Song {
        uintptr_t music{};
        int music_id{};
        int difficulty[2]{};
        int level[2]{};
    };
    bool request_entry(const native::Settings& settings,const native::Selection& cursor,uintptr_t own_category) {
        if(phase_!=Phase::idle || (settings.mode!=5 && settings.mode!=0 && settings.mode!=6) || settings.style<0 || settings.style>1 ||
           (!settings.joined[0] && !settings.joined[1]) || !own_category ||
           !cursor.category_valid || cursor.category!=own_category || !cursor.opened ||
           cursor.style!=settings.style) return false;
        style_=settings.style;
        for(int side=0;side<2;++side) joined_[side]=settings.joined[side]!=0;
        phase_=Phase::entry_pending;
        return true;
    }
    bool begin(uintptr_t flow) {
        if(phase_!=Phase::entry_pending || !flow) return false;
        flow_=flow;phase_=Phase::selecting;
        for(int side=0;side<2;++side) health_[side]=joined_[side]?5000:0;
        return true;
    }
    bool owns(uintptr_t flow) const {return flow && flow_==flow && phase_!=Phase::idle;}
    bool active() const {return flow_!=0 && phase_!=Phase::idle;}
    bool can_select(const Song& song) const {
        if(phase_!=Phase::selecting || !song.music || song.music_id<=0) return false;
        for(int side=0;side<2;++side) {
            // Native prepare56 dereferences BOTH chart slots, even for single
            // player. They must both refer to existing charts.
            if(song.difficulty[side]<0 || song.difficulty[side]>4 || song.level[side]<1 || song.level[side]>12) return false;
            if(joined_[side] && health_[side]>0 && song.level[side]<minimum_[side]) return false;
        }
        return true;
    }
    bool select(const Song& song) {
        if(!can_select(song)) return false;
        songs_[stage_]=song;known_[stage_]=true;phase_=Phase::selected;return true;
    }
    bool begin_stage(int native_index) {
        if(phase_!=Phase::selected || native_index!=stage_ || !known_[stage_]) return false;
        phase_=Phase::playing;return true;
    }
    bool cancel_selection() {
        if(phase_!=Phase::selected)return false;
        songs_[stage_]={};known_[stage_]=false;phase_=Phase::selecting;return true;
    }
    bool capture_rows(int native_index,const std::array<native::DanRow,8>& rows,
                      const std::array<int,2>& raw_health) {
        if(phase_!=Phase::playing || native_index!=stage_) return false;
        for(int side=0;side<2;++side)
            if(raw_health[side]>5000 || (raw_health[side]<0 && rows[side*4+stage_].cleared)) return false;
        // The complete native array is retained unchanged: including zero rows
        // for not-yet-played songs and native clear flags/judgments.
        rows_=rows;health_=raw_health;phase_=Phase::stage_result;return true;
    }
    int after_native_next(int previous,int native_next,int native_index) {
        if(!active()) return native_next;
        if(previous==native::dan_stage && phase_==Phase::playing && native_next==native::stage_result && native_index==stage_)
            return native_next;
        if(previous==native::stage_result && phase_==Phase::stage_result) {
            if(native_next==native::dan_result && native_index==stage_) {
                phase_=Phase::result;return native_next;
            }
            if(native_next==native::dan_stage && stage_<3 && native_index==stage_+1) {
                for(int side=0;side<2;++side) minimum_[side]=songs_[stage_].level[side];
                ++stage_;phase_=Phase::selecting;return native::music_select;
            }
        }
        if(previous==native::dan_result && phase_==Phase::result && native_next==0x7fffffff) {
            phase_=Phase::leaving;return native_next;
        }
        // Preserve save suppression until the custom parent exits. Do not
        // silently drop back to an official course on a state mismatch.
        phase_=Phase::fault;return 0x7fffffff;
    }
    std::optional<int> music_id(int index,int style) const {
        if(!active() || style!=style_ || index<0 || index>=4 || !known_[index]) return {};
        return songs_[index].music_id;
    }
    std::optional<int> difficulty(int index,int style,int side) const {
        if(!music_id(index,style) || side<0 || side>=2) return {};
        return songs_[index].difficulty[side];
    }
    bool suppress_grade_save() const {return active();}
    int stage() const {return stage_;}
    int style() const {return style_;}
    int minimum_level(int side) const {return side>=0 && side<2?minimum_[side]:-1;}
    int browsing_minimum() const {
        // Shared song lists retain charts usable by either surviving side.
        // Final confirmation still enforces each player's own threshold.
        int level=13;
        for(int side=0;side<2;++side)if(joined_[side] && health_[side]>0)level=std::min(level,minimum_[side]);
        return level==13?1:level;
    }
    int loading_stage()const{return stage_==3?4:stage_;} // Native FINAL texture is index 4.
    const std::array<int,2>& health_raw() const {return health_;}
    const std::array<native::DanRow,8>& rows() const {return rows_;}
    Phase phase() const {return phase_;}
    void leave() {*this=NativeSession{};}
private:
    Phase phase_=Phase::idle;
    uintptr_t flow_{};
    int style_{},stage_{};
    std::array<bool,2> joined_{};
    std::array<int,2> health_{};
    std::array<int,2> minimum_{1,1};
    std::array<Song,4> songs_{};
    std::array<bool,4> known_{};
    std::array<native::DanRow,8> rows_{};
};
} // namespace self_dan
