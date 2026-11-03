// Used only by cooperation_test.exe. No FireNet initialization, network or GPU.
#include "../src/native_compat.hpp"
static uintptr_t fire_base{};
static int fire_finishes{},fire_native_exits{},fire_locks{},fire_unlocks{};
static int fire_lock(void*){++fire_locks;return 0;}
static void fire_unlock(void*){++fire_unlocks;}
static void fire_finish(){++fire_finishes;}
static void fire_native_exit(void* scene){assert(scene==reinterpret_cast<void*>(0x1234));++fire_native_exits;}
template<class T> static void fire_store(uintptr_t at,const T& value){
    DWORD old{},ignored{};assert(VirtualProtect(reinterpret_cast<void*>(at),sizeof(T),PAGE_READWRITE,&old));
    std::memcpy(reinterpret_cast<void*>(at),&value,sizeof(T));
    assert(VirtualProtect(reinterpret_cast<void*>(at),sizeof(T),old,&ignored));
}
struct FireReader {
    template<class T>bool operator()(uintptr_t at,T& value)const{
        SIZE_T got{};return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),&value,sizeof(T),&got) && got==sizeof(T);
    }
};
static void prepare_firenet(uintptr_t game){
    const auto module=LoadLibraryExW(L"firenet.dll",nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(module);
    fire_base=reinterpret_cast<uintptr_t>(module);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(fire_base+reinterpret_cast<IMAGE_DOS_HEADER*>(fire_base)->e_lfanew);
    assert(nt->FileHeader.TimeDateStamp==0x6ad36e3d && nt->OptionalHeader.SizeOfImage==0x1ab000 && nt->OptionalHeader.AddressOfEntryPoint==0x8c89c);
    struct Scene {uintptr_t table,callback,saved,native;int slot;};
    const Scene scenes[]={
        {0xd73238,0x727b0,0x144290,0x91a2c0,16},
        {0xd7b670,0x72a40,0x144298,0x91a2c0,16},
        {0xd4c260,0x72cd0,0x144260,0x8b5360,17},
        {0xd45ad8,0x72d70,0x144268,0x896e40,17},
        {0xd7ba70,0x72e10,0x144270,0x842250,17},
        {0xd7ce08,0x72eb0,0x144278,0x8416f0,17}};
    // Replay exactly the six vtable publications in the reviewed installer.
    for(const auto& scene:scenes){
        const auto slot=game+scene.table+scene.slot*8;
        assert(*reinterpret_cast<uintptr_t*>(slot)==game+scene.native);
        fire_store(fire_base+scene.saved,game+scene.native);
        fire_store(slot,fire_base+scene.callback);
    }
    addresses::use_current(true);
    assert(self_dan::compatible_firenet_exit(game,fire_base,fire_base+0x72e10,FireReader{}));
    // Execute real callbacks; substitute only external locking, MP4 finalizer
    // and game teardown. Active/inactive paths are both checked below.
    stub(fire_base+0x8a92c,&fire_lock);stub(fire_base+0x8a958,&fire_unlock);
    stub(fire_base+0x70c10,&fire_finish);
    for(const auto& scene:scenes)if(scene.slot==17)stub(game+scene.native,&fire_native_exit);
}
static void verify_firenet_exit(uintptr_t game){
    assert(fire_base);
    const auto exit=*reinterpret_cast<void(**)(void*)>(game+0xd7ba70+17*8);
    // Free-dan must sit above FireNet and retain its exact original callback.
    assert(reinterpret_cast<uintptr_t>(exit)!=fire_base+0x72e10);
    for(unsigned char enabled:{0,1}){
        fire_store(fire_base+0x143c9f,enabled);
        fire_finishes=fire_native_exits=fire_locks=fire_unlocks=0;
        exit(reinterpret_cast<void*>(0x1234));
        assert(fire_finishes==enabled && fire_native_exits==1 && fire_locks==1 && fire_unlocks==1);
    }
    // The result leave is owned only by FireNet; free-dan's rendering/init
    // wrappers must not replace it or prevent recording completion.
    const auto result_exit=*reinterpret_cast<void(**)(void*)>(game+0xd4c260+17*8);
    assert(reinterpret_cast<uintptr_t>(result_exit)==fire_base+0x72cd0);
    fire_finishes=fire_native_exits=fire_locks=fire_unlocks=0;result_exit(reinterpret_cast<void*>(0x1234));
    assert(fire_finishes==1 && fire_native_exits==1 && fire_locks==1 && fire_unlocks==1);
    std::puts("PASS: actual FireNet recorder callbacks; active/inactive finalization and native teardown once; result leave retained");
}
