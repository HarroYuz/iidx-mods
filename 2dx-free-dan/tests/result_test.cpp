#include "../src/native_result.hpp"
#include <cstdio>
#include <cstdlib>
namespace n=self_dan::native;
static void check(bool ok,const char* message) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int main() {
    n::GradeManagerPrefix manager{};manager.special_course=1;
    manager.result_grade[0]=13;manager.result_grade[1]=10;
    manager.special_result[0]=4;manager.special_result[1]=6;
    const auto before=manager;
    int account_calls=0;
    auto account=[&](int side){++account_calls;return side+1;};
    {
        n::GradeResultScope guard(&manager,true);
        check(!manager.special_course,"special submission branch disabled within result callback");
        check(n::account_for_result_save(true,0,account)==0 && n::account_for_result_save(true,1,account)==0 && account_calls==0,
              "custom result enters native no-save branch without account lookup");
        manager.result_grade[0]=0;manager.result_grade[1]=19;
        manager.special_result[0]=9;manager.special_result[1]=2;
    }
    check(std::memcmp(&before,&manager,sizeof(manager))==0,"both official caches restored");
    {
        n::GradeResultScope guard(&manager,false);
        check(manager.special_course==1 && n::account_for_result_save(false,1,account)==2,"official result remains native");
        manager.result_grade[0]=15;
    }
    check(manager.result_grade[0]==15 && account_calls==1,"official grade updates preserved");
    try {
        n::GradeResultScope guard(&manager,true);manager.result_grade[0]=0;throw 1;
    } catch(int) {}
    check(manager.result_grade[0]==15 && manager.special_course==1,"scope restored on C++ unwind");
    std::puts("PASS: scoped native result caches and custom-only no-save dispatch");
}
