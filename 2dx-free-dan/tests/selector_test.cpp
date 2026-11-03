#include "../src/native_selector.hpp"
#include <cstdlib>
#include <cstring>
using self_dan::NativeSession;using self_dan::NativeSelector;
namespace n=self_dan::native;
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(){
    NativeSession session;NativeSelector selector;n::Settings settings{};
    settings.mode=5;settings.style=1;settings.joined[0]=1;
    n::Selection cursor;cursor.category=0x800;cursor.category_valid=true;cursor.opened=true;cursor.style=1;
    check(session.request_entry(settings,cursor,0x800) && session.begin(0x1000),"begin custom course");
    check(selector.enter(0x2000,session),"own scene");
    using D=NativeSelector::Decision;
    check(selector.confirm(0x2001,cursor,session)==D::passthrough,"official selector untouched");
    check(selector.confirm(0x2000,cursor,session)==D::reject,"folder timeout rejected");
    cursor.entry_type=2;check(selector.confirm(0x2000,cursor,session)==D::reject,"random timeout rejected");
    cursor.entry_type=1;cursor.preview_valid=true;cursor.music=0x9999;cursor.music_id=25007;
    cursor.difficulty[0]=cursor.difficulty[1]=3;cursor.level[0]=cursor.level[1]=11;
    check(selector.confirm(0x2000,cursor,session)==D::accept && session.phase()==NativeSession::Phase::selecting,"preliminary confirmation does not freeze");
    cursor.level[0]=12;check(selector.commit(0x2000,cursor,session)==D::accept,"final popup choice committed");
    check(selector.commit(0x2000,cursor,session)==D::accept,"duplicate final callback idempotent");
    cursor.difficulty[0]=2;check(selector.confirm(0x2000,cursor,session)==D::reject,"changed chart after freeze rejected");
    selector.reject(0x2000,session);
    check(session.phase()==NativeSession::Phase::selecting && !selector.chosen() && !session.music_id(0,1),"rejected frozen choice can be selected again");
    cursor.difficulty[0]=3;
    check(selector.commit(0x2000,cursor,session)==D::accept,"retry after rejected frozen choice");
    check(session.begin_stage(0),"native stage begins");
    std::array<n::DanRow,8> rows{};check(session.capture_rows(0,rows,{2923,0}),"native results");
    check(session.after_native_next(n::stage_result,n::dan_stage,1)==n::music_select,"next choice");
    selector.leave(0x2001);check(selector.owns(0x2000),"foreign teardown cannot drop ownership");
    selector.leave(0x2000);check(selector.enter(0x3000,session),"new scene after stage");
    cursor.level[0]=11;check(selector.confirm(0x3000,cursor,session)==D::reject,"lower grade rejected on input and timer path");
    cursor.level[0]=12;check(selector.confirm(0x3000,cursor,session)==D::accept,"equal grade allowed");
    cursor.style=0;check(selector.confirm(0x3000,cursor,session)==D::reject,"style mismatch rejected");
    char text[160];selector.help(session,text,true);check(std::strstr(text,"2/4") && std::strstr(text,"1P LV 12+"),"help contains current song and numeric limit");
    std::puts("PASS: native selector input/timeout gate, popup changes, frozen chart and scene ownership");
}
