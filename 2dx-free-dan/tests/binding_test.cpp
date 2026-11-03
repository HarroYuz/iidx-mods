#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../src/native_dan.hpp"
#include <cstdio>
#include <cstdlib>
namespace n=self_dan::native;
static uintptr_t base;
static self_dan::NativeDan* bridge;
static n::GradeManagerPrefix grades;
static int saves,cleanups,prepared_music;
static void check(bool ok,const char* message) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
template<class T> static T& at(uintptr_t offset){return *reinterpret_cast<T*>(base+offset);}
static uint64_t init(void*,void*) {at<int>(n::dan_state)=-1;at<int>(n::dan_state+4)=0;return 0;}
static int first(void*){return n::dan_select;}
static int official_song(void*,int,int,int){return 999;}
static int official_difficulty(void*,int,int,int){return 1;}
static void prepare(void*,int scene) {
    const int index=at<int>(n::dan_state+4);
    if(scene==n::dan_stage) {
        prepared_music=bridge->course_music(nullptr,0,index,1,official_song);
        check(bridge->course_difficulty(nullptr,0,index,1,0,official_difficulty)==3,"custom P1 chart reaches stage preparation");
        for(int side=0;side<2;++side)at<n::Gauge>(n::gauges+side*20).raw=at<int>(0xb287480+side*4);
    }
    if(scene==n::stage_result) {
        auto& row=at<n::DanRow>(n::dan_records+index*32);row.displayed_health=2*(at<n::Gauge>(n::gauges).raw/100);
        row.notes=900+index;row.judgment[0]=400;row.cleared=1;
    }
}
static int next(void*,int scene) {
    if(scene==n::dan_select)return n::dan_stage;
    if(scene==n::dan_stage)return n::stage_result;
    if(scene==n::stage_result) {
        auto& index=at<int>(n::dan_state+4);
        if(index==3 || at<n::Gauge>(n::gauges).raw==0)return n::dan_result;
        ++index;return n::dan_stage;
    }
    if(scene==n::dan_result){++cleanups;return 0x7fffffff;}
    return n::dan_select;
}
static int account(int) {++saves;return 1;}
static bool joined(int){return true;}
static uint64_t result_init(void*,void*) {
    check(grades.special_course==0,"special course disabled during result init");
    grades.result_grade[0]=0;
    check(bridge->result_account(0,account)==0,"no-save decision active during init");
    return 1;
}
static void result_update(void*,bool,void*) {
    grades.result_grade[1]=0;
    check(bridge->result_account(1,account)==0,"no-save decision active during update");
}
static void result_render(void*) {
    bridge->result_joined(1,joined);
    check(bridge->course_difficulty(nullptr,0,0,1,-1,official_difficulty)==2,"result rendering uses side captured before course getter");
    check(std::strcmp(bridge->result_title("ti_as_7",0),"ti_self_dan_zi")==0,"title redirect scoped to custom result");
}
int main() {
    auto* memory=static_cast<uint8_t*>(VirtualAlloc(nullptr,n::image_size,MEM_RESERVE,PAGE_NOACCESS));
    check(memory!=nullptr,"reserve synthetic game address space");base=reinterpret_cast<uintptr_t>(memory);
    for(uintptr_t rva:{n::settings,n::dan_state,n::gauges,uintptr_t(0xb287480),uintptr_t(0x882200)}) {
        const auto page=rva&~uintptr_t(4095);
        check(VirtualAlloc(memory+page,8192,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit synthetic data pages");
    }
    // Stub only the getter: mov rax,&grades; ret. No game binary is executed.
    auto* stub=memory+0x882200;stub[0]=0x48;stub[1]=0xb8;auto* prefix=&grades;
    std::memcpy(stub+2,&prefix,8);stub[10]=0xc3;DWORD old{};
    check(VirtualProtect(memory+(0x882200&~4095),4096,PAGE_EXECUTE_READ,&old),"getter stub RX");
    check(FlushInstructionCache(GetCurrentProcess(),stub,11),"flush getter stub");
    self_dan::NativeDan integration(base);bridge=&integration;
    check(integration.course_music(nullptr,13,0,1,official_song)==999,"inactive course provider calls original");
    n::Settings settings{};settings.mode=5;settings.style=1;settings.joined[0]=1;
    n::Selection folder;folder.category_valid=true;folder.opened=true;folder.style=1;folder.category=0x1111;
    check(integration.session().request_entry(settings,folder,0x1111),"entry armed");
    alignas(16) uint8_t flow[128]{};settings.mode=3;at<n::Settings>(n::settings)=settings;
    check(integration.initialize(flow,nullptr,init)==0 && integration.first(flow,first)==n::music_select,"initialize preserves return and redirects first child");
    const int health[4]={5000,2923,2479,1773};
    for(int i=0;i<4;++i) {
        check(integration.session().select({uintptr_t(0x2000+i),14014+i,{3,2},{10,8}}),"confirm valid chart");
        check(integration.next(flow,n::music_select,next)==n::dan_stage,"native confirmation handling");
        integration.prepare(flow,n::dan_stage,prepare);
        check(prepared_music==14014+i && at<n::Gauge>(n::gauges).raw==(i?health[i-1]:5000),"native stage gets custom song and exact carried gauge");
        at<n::Gauge>(n::gauges).raw=health[i];
        check(integration.next(flow,n::dan_stage,next)==n::stage_result,"native single-song result retained");
        integration.prepare(flow,n::stage_result,prepare);
        check(integration.session().rows()[i].notes==900+i,"capture after native row creation");
        check(integration.next(flow,n::stage_result,next)==(i<3?n::music_select:n::dan_result),"native stage index advances exactly once");
    }
    grades.special_course=1;grades.result_grade[0]=13;grades.result_grade[1]=10;
    int scene;
    check(integration.result_initialize(&scene,nullptr,result_init)==1 && grades.result_grade[0]==13,"result init cache restored");
    integration.result_update(&scene,false,nullptr,result_update);
    integration.result_render(&scene,result_render);
    check(grades.result_grade[1]==10 && grades.special_course==1 && saves==0,"both native save paths isolated in callback scope");
    check(std::strcmp(integration.result_title("original",0),"original")==0,"title redirect ends at callback boundary");
    check(integration.next(flow,n::dan_result,next)==0x7fffffff && cleanups==1,"original final cleanup runs once");
    integration.leave();check(integration.course_music(nullptr,13,0,1,official_song)==999,"normal provider restored after exit");
    VirtualFree(memory,0,MEM_RELEASE);
    std::puts("PASS: native callback plumbing against synthetic game memory (no live-game execution)");
}
