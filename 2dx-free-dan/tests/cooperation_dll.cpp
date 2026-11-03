// Build only into build/cooperation/2dx-free-dan.dll. No worker/device startup.
#define DllMain UnusedProductionDllMain
#include "../src/native_runtime.cpp"
#undef DllMain
#include <cassert>
extern "C" __declspec(dllexport) bool TestPrepare(uintptr_t game){
    base=game;return addresses::select(base) && prepare() && install_bootstrap();
}
extern "C" __declspec(dllexport) bool TestPublish(){
    owner_thread=GetCurrentThreadId();return publish();
}
extern "C" __declspec(dllexport) void TestBegin(int random,bool custom){
    option_history.begin(custom);custom_course_active=custom;
    self_dan::DanOptionHistory::Stage s;s.joined[0]=true;s.side[0].random[0]=random;
    option_history.capture(0,s);options_stage=0;
    course_save_blocked=option_history.blocked();
}
extern "C" __declspec(dllexport) bool TestCapture(int rate,bool scratch){
    capture_dan_practice();
    const auto& s=option_history.stages()[options_stage].side[0];
    return s.rate==rate && s.all_scratch==scratch;
}
extern "C" __declspec(dllexport) void TestLeave(){
    option_history.leave();custom_course_active=false;course_save_blocked=false;
}
extern "C" __declspec(dllexport) uintptr_t TestRva(uintptr_t symbol){return addresses::rva(symbol);}
extern "C" __declspec(dllexport) bool TestBranches(){
    for(const auto& hook:calls)if(!self_dan::call_matches(hook,hook.after))return false;
    return true;
}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}

extern "C" __declspec(dllexport) bool TestCustomStart(){
    auto& s=dan->session();s.leave();
    settings().mode=0;settings().style=0;settings().joined[0]=1;settings().joined[1]=0;
    n::Selection cursor;cursor.category=1;cursor.category_valid=true;cursor.opened=true;cursor.style=0;
    if(!s.request_entry(settings(),cursor,1) || !s.begin(0x4321))return false;
    settings().mode=3;option_history.begin(true);custom_course_active=true;course_save_blocked=true;return true;
}
extern "C" __declspec(dllexport) bool TestCustomAdvance(bool finish){
    auto& s=dan->session();const int stage=s.stage();
    if(!finish){
        self_dan::NativeSession::Song song{0x1234,123,{3,3},{10,10}};
        options_stage=stage;
        self_dan::DanOptionHistory::Stage opt;opt.joined[0]=true;option_history.capture(stage,opt);
        return s.select(song) && s.begin_stage(stage);
    }
    std::array<n::DanRow,8> rows{};
    if(!s.capture_rows(stage,rows,{4000,0}))return false;
    if(stage==3)return s.after_native_next(n::stage_result,n::dan_result,stage)==n::dan_result;
    return s.after_native_next(n::stage_result,n::dan_stage,stage+1)==n::music_select;
}
