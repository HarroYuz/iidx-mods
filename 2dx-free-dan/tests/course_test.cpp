#include "../src/course.hpp"
#include <cstdio>
#include <cstdlib>
using namespace self_dan;
static int checks;
static void check(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
    Course c;
    check(!c.start({1,2,10}), "cannot start outside folder");
    check(c.enter() && c.health_raw()==5000, "fixed initial 100 percent");
    check(!c.enter(), "repeated entry cannot reset active course");
    check(!c.start({1,0,0}) && !c.start({1,0,13}), "reject unknown or invalid numeric level");
    check(c.start({1,4,10}), "first song level 10");
    check(!c.finish(-1,80) && !c.finish(5001,80), "invalid gauge cannot advance course");
    check(c.finish(2923,85), "record first song");
    check(!c.finish(1000,50) && c.completed()==1, "duplicate result cannot advance stage");
    check(c.health_raw()==2923 && c.displayed_health()==58, "carry health instead of refilling");
    check(!c.can_select({2,9,9}), "higher chart classification does not bypass numeric limit");
    check(c.start({2,0,10}) && c.finish(2479,90), "same numeric level allowed with lower classification");
    check(c.start({3,1,11}) && c.finish(3901,93), "increased numeric level and health recovery allowed");
    check(c.start({4,1,12}) && c.finish(1773,64), "fourth stage accepted");
    check(c.phase()==Phase::result && c.completed()==4, "four songs go to total result");
    check(!c.start({5,0,12}), "fifth song forbidden");
    check(c.results()[0].end_health_raw==2923 && c.results()[3].attainment==64, "retain per-song result rows");
    c.leave();
    check(c.enter() && c.health_raw()==5000 && c.minimum_level()==1 && c.completed()==0, "new course resets rules and health");
    check(c.start({8,2,12}) && c.finish(0,31), "zero gauge records failed song");
    check(c.phase()==Phase::result && c.completed()==1 && !c.can_select({9,2,12}), "zero health ends course early");
    check(c.results()[1].chart.music_id==0, "unplayed rows remain empty");
    std::printf("PASS: %d course checks\n", checks);
}
