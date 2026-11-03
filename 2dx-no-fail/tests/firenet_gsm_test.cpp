// Executes FireNet's actual GSM update callback, with native option/judgement
// and logging services replaced. Never runs FireNet initialization or network.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
static uintptr_t fire;
static int state[14]{}, gauge[2]{}, chart[2][4]{}, options[2]{};
static int types[2][2]{}, calls[5]{};
static void* option_object=reinterpret_cast<void*>(1);
static unsigned get(void*,int side,int style) { return types[style][side]; }
static void* set(void*,int side,unsigned type,int style) { types[style][side]=type;return nullptr; }
static bool yes() { return true; }
static bool eligible(int type,const int16_t* values,uint8_t starting) {
    // The fixture does not run CRT constructors for FireNet's std::vectors.
    // Supply the legacy select-to-under predicate instead of those vectors.
    constexpr int rank[]={2,0,1,3,4};
    return rank[type]<=rank[starting] && values[type]<=5000 &&
        values[type]>=(type>=3?100:4000);
}
static LONG WINAPI fault(EXCEPTION_POINTERS* e) {
    std::printf("FAULT %lx at FireNet+%llx\n",e->ExceptionRecord->ExceptionCode,
        static_cast<unsigned long long>(e->ContextRecord->Rip-fire));
    std::fflush(stdout);ExitProcess(4);
}
static void cookie(uintptr_t) {}
static void* format(void* s,void*,void*) {
    std::memset(s,0,32);*reinterpret_cast<size_t*>(static_cast<char*>(s)+24)=15;return s;
}
static void native(void*,int side,int judgement) {
    int type=types[state[1]][state[1]?0:side];++calls[type];
    gauge[side]+=judgement==0?1000:type==4?-6000:type==3?-3000:-100;
    if(gauge[side]>5000)gauge[side]=5000;
}
static void stub(uintptr_t rva,void* target) {
    auto at=reinterpret_cast<unsigned char*>(fire+rva);DWORD old{},ignored{};
    assert(VirtualProtect(at,14,PAGE_EXECUTE_READWRITE,&old));
    unsigned char jump[14]={0xff,0x25};std::memcpy(jump+6,&target,8);std::memcpy(at,jump,14);
    assert(VirtualProtect(at,14,old,&ignored));FlushInstructionCache(GetCurrentProcess(),at,14);
}
template<class T> void store(uintptr_t rva,T value) { std::memcpy(reinterpret_cast<void*>(fire+rva),&value,sizeof value); }
int main(int argc,char**argv) {
    setbuf(stdout,nullptr);SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1,&fault);
    assert(argc==2);auto module=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    assert(module);fire=reinterpret_cast<uintptr_t>(module);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(fire+reinterpret_cast<IMAGE_DOS_HEADER*>(fire)->e_lfanew);
    assert(nt->FileHeader.TimeDateStamp==0x6ad36e3d && nt->OptionalHeader.SizeOfImage==0x1ab000);
    stub(0x650c0,reinterpret_cast<void*>(&yes));
    stub(0x651a0,reinterpret_cast<void*>(&eligible));
    stub(0x67070,reinterpret_cast<void*>(&native));
    stub(0x82260,reinterpret_cast<void*>(&format));
    stub(0x278f0,reinterpret_cast<void*>(&yes));
    stub(0x8bdb0,reinterpret_cast<void*>(&cookie));
    store(0x141dc0,&state);store(0x141d80,&option_object);
    store(0x141d58,&get);store(0x141db0,&set);
    store(0x141d60,&gauge[0]);store(0x141d90,&gauge[1]);
    store(0x141da0,&chart[0]);store(0x141dc8,&chart[1]);
    store(0x141da8,&options[0]);store(0x141dd0,&options[1]);
    store<size_t>(0x13dec0,4);
    store<uint8_t>(0x13dec8,1);store<uint8_t>(0x13dec9,1);
    store<uint8_t>(0x141d54,1);
    auto update=reinterpret_cast<void(*)(int,int)>(fire+0x659f0);
    for(int style=0;style<2;++style)for(int side=0;side<2;++side) {
        state[0]=6;state[1]=style;
        // gauge_type objects consist of side and style int32s.
        store<int>(0x143dc0,0);store<int>(0x143dc4,0);
        store<int>(0x13e0c8,1);store<int>(0x13e0cc,0);
        store<int>(0x13e068,0);store<int>(0x13e06c,1);
        store<uint8_t>(0x141d56,4);store<uint8_t>(0x141d52,4);store<uint8_t>(0x141d53,4);
        for(auto& row:types)for(auto& type:row)type=4;
        auto values=reinterpret_cast<int16_t*>(fire+(side==0?0x143c90:0x143c80));
        for(int i=0;i<5;++i)values[i]=i<3?1100:5000;
        std::memset(calls,0,sizeof calls);
        update(side,4);
        assert(values[4]==0 && values[3]==2000);
        assert(types[style][style?0:side]==3);
        assert(calls[4]==1 && calls[3]==1);
        update(side,4);
        assert(values[4]==0 && values[3]==0);
        assert(calls[4]==1 && calls[3]==2); // EXH is no longer simulated.
        update(side,0);
        assert(values[4]==0 && values[3]==0);
        assert(calls[4]==1 && calls[3]==2); // Neither hard gauge can recover.
    }
    std::puts("PASS: real FireNet GSM SP/DP both sides; reproduced zero clamp and stopped simulation after depletion");
}
