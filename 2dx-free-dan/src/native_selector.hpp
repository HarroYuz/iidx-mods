#pragma once
#include "native_session.hpp"
#include <cstdio>

namespace self_dan {
// One selection scene at a time. The native callback installer supplies the
// cursor snapshot AFTER native input processing and BEFORE queuing confirmation.
class NativeSelector {
public:
    enum class Decision { passthrough, accept, reject };
    bool enter(uintptr_t scene,NativeSession& session) {
        if(scene_ || !scene || session.phase()!=NativeSession::Phase::selecting)return false;
        scene_=scene;stage_=session.stage();frozen_=false;return true;
    }
    bool owns(uintptr_t scene)const{return scene_ && scene_==scene;}
    static NativeSession::Song song(const native::Selection& s) {
        return {s.music,s.music_id,{s.difficulty[0],s.difficulty[1]},{s.level[0],s.level[1]}};
    }
    Decision confirm(uintptr_t scene,const native::Selection& cursor,NativeSession& session) {
        if(!owns(scene))return Decision::passthrough;
        if(session.stage()!=stage_ || cursor.style!=session.style() || !cursor.category_valid ||
           !cursor.preview_valid || cursor.entry_type!=1)return Decision::reject;
        const auto candidate=song(cursor);
        if(frozen_)return same(candidate,chosen_)?Decision::accept:Decision::reject;
        return session.can_select(candidate)?Decision::accept:Decision::reject;
    }
    // Freeze at the final outgoing callback, after any preliminary popup.
    // Repeated native transitions do not commit the song twice.
    Decision commit(uintptr_t scene,const native::Selection& cursor,NativeSession& session) {
        const auto decision=confirm(scene,cursor,session);
        if(decision!=Decision::accept || frozen_)return decision;
        chosen_=song(cursor);
        if(!session.select(chosen_))return Decision::reject;
        frozen_=true;return Decision::accept;
    }
    const NativeSession::Song* chosen()const{return frozen_?&chosen_:nullptr;}
    void reject(uintptr_t scene,NativeSession& session){
        if(owns(scene) && frozen_ && session.cancel_selection())frozen_=false;
    }
    void leave(uintptr_t scene){if(owns(scene)){scene_=0;frozen_=false;}}
    void help(const NativeSession& session,char (&text)[160],bool rejected)const {
        std::snprintf(text,sizeof(text),"CUSTOM DAN %d/4 | 1P LV %d+ / 2P LV %d+%s",
            session.stage()+1,session.minimum_level(0),session.minimum_level(1),
            rejected?" | SELECT A CHART AT OR ABOVE THE LIMIT":"");
    }
private:
    static bool same(const NativeSession::Song& a,const NativeSession::Song& b) {
        return a.music==b.music && a.music_id==b.music_id &&
            a.difficulty[0]==b.difficulty[0] && a.difficulty[1]==b.difficulty[1] &&
            a.level[0]==b.level[0] && a.level[1]==b.level[1];
    }
    uintptr_t scene_{};int stage_{};bool frozen_{};NativeSession::Song chosen_{};
};
} // namespace self_dan
