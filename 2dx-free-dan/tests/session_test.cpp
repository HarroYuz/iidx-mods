#include "../src/native_session.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using self_dan::NativeSession;
namespace n=self_dan::native;
static void check(bool ok,const char* message) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static NativeSession enter(bool p2=false) {
    NativeSession s;n::Settings settings{};settings.mode=5;settings.style=1;
    settings.joined[0]=1;settings.joined[1]=p2;
    n::Selection folder;folder.category_valid=true;folder.category=0x1234;folder.opened=true;folder.style=1;
    check(!s.request_entry(settings,folder,0x5678),"foreign folder cannot enter");
    check(s.request_entry(settings,folder,0x1234) && !s.active(),"entry request before Dan flow");
    check(s.begin(0x9876) && s.owns(0x9876) && !s.owns(0x1111),"scope to custom flow instance");
    check(s.health_raw()[0]==5000 && s.health_raw()[1]==(p2?5000:0),"100 percent for joined sides");
    return s;
}
int main() {
    auto s=enter();
    const int raw[4]={5000,2923,2479,1773};
    const int levels[4]={8,11,11,12};
    std::array<n::DanRow,8> rows{};
    for(int stage=0;stage<4;++stage) {
        check(s.loading_stage()==(stage==3?4:stage),"native loading indices 1st/2nd/3rd/FINAL");
        NativeSession::Song song{uintptr_t(0x2000+stage),14014+stage,{3,3},{levels[stage],levels[stage]}};
        check(s.select(song) && !s.select(song),"one chart per stage");
        check(s.music_id(stage,1)==song.music_id && !s.music_id(stage,0),"provider isolates style");
        check(!s.music_id(4,1) && !s.music_id(-1,1),"provider bounds");
        check(!s.begin_stage(stage+1) && s.begin_stage(stage),"match native stage index");
        check(s.after_native_next(n::dan_stage,n::stage_result,stage)==n::stage_result,"native next runs before prepare captures result");
        rows[stage].displayed_health=2*(raw[stage]/100);rows[stage].notes=891+stage;
        rows[stage].judgment[0]=466;rows[stage].cleared=1;
        check(s.capture_rows(stage,rows,{raw[stage],0}),"capture native result rows");
        check(s.health_raw()[0]==raw[stage] && std::memcmp(s.rows().data(),rows.data(),sizeof(rows))==0,
              "native raw health and judgments retained exactly");
        check(!s.capture_rows(stage,rows,{raw[stage],0}),"duplicate result cannot advance");
        if(stage<3) {
            check(s.after_native_next(n::stage_result,n::dan_stage,stage+1)==n::music_select,"insert selection after native advancement");
            check(s.browsing_minimum()==levels[stage],"browsing filter follows previous chart level");
            song.level[0]=levels[stage]-1;
            check(!s.can_select(song),"reject lower numeric level");
            song.level[0]=levels[stage];check(s.can_select(song),"allow same numeric level");
            check(!s.music_id(stage+1,1),"unselected song is absent, never an official fallback");
        } else check(s.after_native_next(n::stage_result,n::dan_result,3)==n::dan_result,"fourth native result preserved");
    }
    check(s.suppress_grade_save(),"save isolation remains active in final result");
    check(s.after_native_next(n::dan_result,0x7fffffff,3)==0x7fffffff && s.suppress_grade_save(),"save isolation until parent exit");
    s.leave();check(!s.active() && !s.suppress_grade_save() && !s.music_id(0,1),"exit clears only custom session");
    s=enter();NativeSession::Song song{0x2000,25007,{3,3},{12,12}};
    check(s.select(song) && s.begin_stage(0) && s.capture_rows(0,rows,{0,0}),"failed first stage");
    check(s.after_native_next(n::stage_result,n::dan_result,0)==n::dan_result,"native early failure goes directly to formal result");
    check(!s.music_id(1,1) && s.music_id(0,1)==25007,"early failure keeps only chosen song");
    s=enter();rows[0].cleared=0;rows[0].displayed_health=-250;
    check(s.select(song) && s.begin_stage(0) && s.capture_rows(0,rows,{-12500,0}),"no-fail negative Dan result is valid");
    check(s.health_raw()[0]==-12500 && s.rows()[0].displayed_health==-250,"retain negative result without clamping");
    check(s.after_native_next(n::stage_result,n::dan_result,0)==n::dan_result && s.suppress_grade_save(),"negative Dan ends at formal result with save isolation");
    s=enter(true);song.level[0]=8;song.level[1]=11;
    check(s.select(song) && s.begin_stage(0) && s.capture_rows(0,rows,{2923,0}),"two-player state with one failed side");
    check(s.after_native_next(n::stage_result,n::dan_stage,1)==n::music_select,"native survivor continues");
    song.level[0]=8;song.level[1]=8;check(s.can_select(song),"failed side cannot block survivor's selection");
    check(s.after_native_next(n::stage_result,n::dan_stage,2)==0x7fffffff && s.suppress_grade_save(),"mismatched sequence retains isolation and exits");
    std::puts("PASS: four native stages, raw carry, progressive levels, early failure and result scope");
}
