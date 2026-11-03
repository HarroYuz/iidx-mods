#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "ingame_rate.cpp"
#include "native_chain.hpp"
#include <array>

namespace {
HMODULE instance{};
std::atomic<bool> stopping{false},ready{false};
self_dan::SlotHook bootstrap[3]{},lifecycle[4]{};
using Update=void(*)(void*,bool,void*);
using Init=uint64_t(*)(void*,void*);
using Exit=void(*)(void*);
Update next_update[3]{};
Init next_init[2]{};
Exit next_exit[2]{};
std::atomic_flag publishing=ATOMIC_FLAG_INIT;

template<int I> uint64_t enter(void* scene,void* parameter) {
    ingame_rate::enter_selection();
    return next_init[I](scene,parameter);
}
template<int I> void leave(void* scene) {
    ingame_rate::stop_preview();
    next_exit[I](scene);
}
bool prepare_lifecycle() {
    const uintptr_t tables[]={0xd82768,0xd7f898};
    void* wrappers[]={reinterpret_cast<void*>(&enter<0>),reinterpret_cast<void*>(&leave<0>),
                      reinterpret_cast<void*>(&enter<1>),reinterpret_cast<void*>(&leave<1>)};
    for(int i=0;i<4;++i) {
        auto slot=reinterpret_cast<void**>(ingame_rate::base+addresses::rva(tables[i/2])+
                                           addresses::scene_slot(i%2?14:13)*8);
        auto current=reinterpret_cast<uintptr_t>(*slot);
        if(!self_dan::executable_target(current))return false;
        lifecycle[i]={slot,reinterpret_cast<void*>(current),wrappers[i]};
        if(i%2)next_exit[i/2]=reinterpret_cast<Exit>(current);
        else next_init[i/2]=reinterpret_cast<Init>(current);
    }
    return true;
}
template<int I> void update(void* scene,bool resumed,void* parameter) {
    if(ready.load() && !ingame_rate::published.load() && !publishing.test_and_set()){
        // Both modules capture predecessors on the owning scene thread.
        // Their slot chains therefore work in either DLL loading order.
        if(!prepare_lifecycle())ingame_rate::fatal("Selection callback conflict.");
        ingame_rate::publish();
        if(self_dan::install_hooks(lifecycle,4)!=self_dan::HookStatus::installed)
            ingame_rate::fatal("Selection callback publication failed.");
        ingame_rate::enter_selection();
        ingame_rate::log("standalone practice integration published");
    }
    next_update[I](scene,resumed,parameter);
}
bool install_bootstrap() {
    const uintptr_t tables[]={0xd818d8,0xd82768,0xd7f898};
    void* wrappers[]={reinterpret_cast<void*>(&update<0>),reinterpret_cast<void*>(&update<1>),
                      reinterpret_cast<void*>(&update<2>)};
    for(int i=0;i<3;++i) {
        auto slot=reinterpret_cast<void**>(ingame_rate::base+addresses::rva(tables[i])+
                                           addresses::scene_slot(15)*8);
        bool installed=false;
        for(int attempt=0;attempt<8 && !installed;++attempt){
            auto current=reinterpret_cast<uintptr_t>(*slot);
            if(!self_dan::executable_target(current))return false;
            next_update[i]=reinterpret_cast<Update>(current);
            bootstrap[i]={slot,reinterpret_cast<void*>(current),wrappers[i]};
            installed=self_dan::install_hooks(&bootstrap[i],1)==self_dan::HookStatus::installed;
            if(!installed)Sleep(1);
        }
        if(!installed)return false;
    }
    return true;
}
DWORD WINAPI worker(void*) {
    try{
        HMODULE pinned{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                              reinterpret_cast<LPCWSTR>(&worker),&pinned))return 1;
        CreateDirectoryW(L"practice-mod",nullptr);
        ingame_rate::log("practice standalone v2 starting");
        const auto start=GetTickCount64();
        HMODULE game{};
        while(!stopping && GetTickCount64()-start<180000) {
            game=GetModuleHandleW(L"bm2dx.dll");
            if(game)break;
            Sleep(50);
        }
        const auto base=reinterpret_cast<uintptr_t>(game);
        if(!game || !addresses::select(base)){
            ingame_rate::log("unsupported game; no hooks installed");return 2;
        }
        if(!GetModuleHandleW(L"ifs_hook.dll")){
            ingame_rate::log("ifs_hook.dll missing; no hooks installed");return 3;
        }
        ingame_rate::log(addresses::version());
        ingame_rate::prepare(base);
        if(!install_bootstrap()){
            ingame_rate::log("bootstrap conflict; practice remains disabled");return 4;
        }
        ready.store(true);
        ingame_rate::log("standalone bootstrap ready");
        return 0;
    }catch(const std::exception& e){
        ingame_rate::log(e.what());return 5;
    }catch(...){
        ingame_rate::log("preparation failed; practice remains disabled");return 6;
    }
}
}
extern "C" __declspec(dllexport) BOOL PracticeReadyV1(){return ingame_rate::published.load()?TRUE:FALSE;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){
        instance=module;DisableThreadLibraryCalls(module);
        auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);
        if(thread)CloseHandle(thread);
    }else if(reason==DLL_PROCESS_DETACH)stopping.store(true);
    return TRUE;
}
