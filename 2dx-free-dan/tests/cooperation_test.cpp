#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
template<class F> F proc(HMODULE module,const char* name){
    const auto raw=GetProcAddress(module,name);assert(raw);
    F f{};static_assert(sizeof(f)==sizeof(raw));std::memcpy(&f,&raw,sizeof(f));return f;
}
static int initializations=0;
static uint64_t init(void*,void*){++initializations;return 0x123456789abcdef0ull;}
static void noop(){}
static void* null_manager(){return nullptr;}
static bool valid(int,bool){return false;}
static int request(void*,int*,int,void*,void*){return 77;}
template<class F> void stub(uintptr_t address,F f){
    auto* code=reinterpret_cast<uint8_t*>(address);DWORD old{},ignored{};
    assert(VirtualProtect(code,14,PAGE_EXECUTE_READWRITE,&old));
    const uint8_t j[]{0xff,0x25,0,0,0,0};std::memcpy(code,j,6);
    const auto target=reinterpret_cast<uintptr_t>(f);std::memcpy(code+6,&target,8);
    assert(FlushInstructionCache(GetCurrentProcess(),code,14));
    assert(VirtualProtect(code,14,old,&ignored));
}
#include "firenet_fixture.hpp"
int main(int argc,char** argv){
    assert(argc==3 || (argc==4 && std::strcmp(argv[3],"firenet")==0));SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    const bool reverse=std::strcmp(argv[2],"reverse")==0;
    auto game=LoadLibraryExW(L"bm2dx.dll",nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(game);
    const auto base=reinterpret_cast<uintptr_t>(game);
    const std::string dir=argv[1];HMODULE free{},practice{};
    auto load_free=[&](){free=LoadLibraryA((dir+"/2dx-free-dan.dll").c_str());assert(free);};
    auto load_practice=[&](){practice=LoadLibraryA((dir+"/2dx-practice.dll").c_str());assert(practice);};
    if(reverse){load_practice();load_free();}else{load_free();load_practice();}
    if(argc==4)prepare_firenet(base);
    HMODULE first=reverse?practice:free,second=reverse?free:practice;
    assert(proc<bool(*)(uintptr_t)>(first,"TestPrepare")(base));
    assert(proc<bool(*)(uintptr_t)>(second,"TestPrepare")(base));
    assert(proc<bool(*)()>(first,"TestPublish")());
    assert(proc<bool(*)()>(second,"TestPublish")());
    assert(proc<bool(*)()>(free,"TestBranches")() && proc<bool(*)()>(practice,"TestBranches")());
    const auto rva=proc<uintptr_t(*)(uintptr_t)>(free,"TestRva");
    stub(base+rva(0x8ea560),&init);
    stub(base+rva(0x5c4db0),&noop);
    // These are practice-only singleton and dispatcher symbols in 0917.
    const auto lookup=[&](uintptr_t current){return base+current;};
    // Values supplied by the runner script from the practice address table.
    // Test-only environment arguments avoid duplicating another repo's table.
    char addresses[160]{};assert(GetEnvironmentVariableA("IIDX_MODS_TEST_RVAS",addresses,sizeof(addresses)));
    unsigned long long bank{},manager{},dispatch{},invalid{};
    assert(std::sscanf(addresses,"%llx,%llx,%llx,%llx",&bank,&manager,&dispatch,&invalid)==4);
    stub(lookup(bank),&noop);stub(lookup(manager),&null_manager);
    stub(lookup(dispatch),&request);stub(lookup(invalid),&valid);
    auto settings=reinterpret_cast<int*>(base+rva(0xacd59a0));settings[0]=0;settings[1]=0;settings[4]=1;
    auto slot=reinterpret_cast<uintptr_t*>(base+rva(0xd82768)+16*8);
    alignas(16) unsigned char scene[0x2200]{};
    assert(reinterpret_cast<uint64_t(*)(void*,void*)>(*slot)(scene,nullptr)==0x123456789abcdef0ull);
    assert(initializations==1);
    *reinterpret_cast<int*>(base+rva(0x107ecf0))=25;
    auto begin=proc<void(*)(int,bool)>(free,"TestBegin");
    auto freeze=proc<void(*)(int,bool)>(practice,"TestFreeze");
    auto capture=proc<bool(*)(int,bool)>(free,"TestCapture");
    auto blocked=proc<BOOL(*)()>(free,"FreeDanBlocksGradeSaveV1");
    auto leave=proc<void(*)()>(free,"TestLeave");
    begin(0,false);freeze(85,false);assert(capture(85,false) && blocked());
    freeze(80,true);assert(capture(80,true) && blocked());
    freeze(100,false);assert(proc<bool(*)()>(practice,"TestBlocked")()); // Course latch survives.
    leave();assert(!blocked() && !proc<bool(*)()>(practice,"TestBlocked")());
    begin(1,false);assert(blocked() && proc<bool(*)()>(practice,"TestBlocked")());
    leave();begin(0,false);assert(!blocked() && proc<bool(*)()>(practice,"TestEditable")());
    leave();begin(0,true);assert(blocked() && !proc<bool(*)()>(practice,"TestEditable")());
    leave();assert(proc<bool(*)()>(practice,"TestEditable")());
    assert(proc<bool(*)()>(free,"TestCustomStart")());
    *reinterpret_cast<int*>(base+rva(0x107ecf0))=23;
    auto advance=proc<bool(*)(bool)>(free,"TestCustomAdvance");
    auto editable=proc<bool(*)()>(practice,"TestEditable");
    auto change=proc<bool(*)(int,bool)>(practice,"TestChange");
    for(int stage=0;stage<4;++stage){
        const int rate=85+stage*5;const bool scratch=stage%2==1;
        assert(editable() && change(rate,scratch));
        assert(advance(false) && !editable());
        assert(!change(150,!scratch));
        freeze(rate,scratch);assert(capture(rate,scratch) && blocked());
        assert(proc<bool(*)()>(practice,"TestBlocked")());
        assert(advance(true));
    }
    assert(!editable() && blocked());leave();
    proc<void(*)()>(practice,"TestCleanup")();
    std::puts("PASS: all four custom selection phases edit rate/ALL-SCR; frozen after confirmation; course save protection retained");
    if(argc==4)verify_firenet_exit(base);
    std::printf("PASS: actual 0917 hook publication, single native init, optional exports, labels' practice snapshot and latched save guards (%s order)\n",argv[2]);
}
