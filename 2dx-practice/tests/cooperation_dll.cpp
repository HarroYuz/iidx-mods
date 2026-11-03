// Build only into another repository's build/cooperation/2dx-practice.dll.
#define DllMain UnusedProductionDllMain
#include "../src/runtime.cpp"
#undef DllMain
extern "C" __declspec(dllexport) bool TestPrepare(uintptr_t game){
    if(!addresses::select(game))return false;
    char temp[MAX_PATH]{};GetTempPathA(MAX_PATH,temp);
    ingame_rate::config_path=std::string(temp)+"2dx-cooperation-"+std::to_string(GetCurrentProcessId())+".ini";
    ingame_rate::prepare(game);return install_bootstrap();
}
extern "C" __declspec(dllexport) bool TestPublish(){
    if(!prepare_lifecycle())return false;
    ingame_rate::publish();
    return self_dan::install_hooks(lifecycle,4)==self_dan::HookStatus::installed;
}
extern "C" __declspec(dllexport) void TestFreeze(int rate,bool scratch){
    ingame_rate::selected=rate;ingame_rate::scratch_selected=scratch;
    ingame_rate::freeze_rate();
}
extern "C" __declspec(dllexport) bool TestBlocked(){
    return ingame_rate::request(nullptr,nullptr,5,nullptr,nullptr)==3 &&
           ingame_rate::request(nullptr,nullptr,41,nullptr,nullptr)==3 &&
           ingame_rate::request(nullptr,nullptr,29,nullptr,nullptr)!=3 &&
           ingame_rate::invalid_record(0,true);
}
extern "C" __declspec(dllexport) bool TestEditable(){return ingame_rate::editable();}
extern "C" __declspec(dllexport) bool TestBranches(){
    using namespace ingame_rate;
    for(const auto& hook:calls)if(!self_dan::call_matches(hook,hook.after))return false;
    for(const auto& hook:menu_calls)if(!self_dan::call_matches(hook,hook.after))return false;
    for(const auto& hook:record_calls)if(!self_dan::call_matches(hook,hook.after))return false;
    return self_dan::call_matches(result_call,result_call.after);
}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}

extern "C" __declspec(dllexport) bool TestChange(int rate,bool scratch){
    using namespace ingame_rate;
    set_option(&menu,0,rate_id(),(150-rate)/5);set_option(&menu,0,scratch_id(),scratch?0:1);
    return selected==rate && scratch_selected==scratch;
}
extern "C" __declspec(dllexport) void TestCleanup(){DeleteFileA(ingame_rate::config_path.c_str());}
