// Executes the real 0917 accumulator in a private mapping. No game entrypoint,
// network, account or renderer is executed. External game helpers are stubs.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <safetyhook.hpp>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../src/policy.hpp"

static uintptr_t game;
static int mode_value=6, death_calls=0, cancel_calls=0;
static bool survival=true;
static int mode() { return mode_value; }
static int is_survival(int) { return survival; }
static void cancel(int) { ++cancel_calls; }
static void* manager() { return nullptr; }
static void death(void*, int, bool) { ++death_calls; }
static no_fail::Track track;
static SafetyHookInline accumulator;
static int32_t& gauge(int side) { return *reinterpret_cast<int32_t*>(game+0xbcbb2c0+20*side); }
static void experimental_accumulate(int side,int delta) {
    if (!survival) { accumulator.call<void>(side,delta); return; }
    track.apply(no_fail::Gauge::hard,delta);
    // Test-range values fit in native int32. Production requires a separately
    // reviewed wide-value display path and per-player/per-gauge state.
    gauge(side)=static_cast<int32_t>(track.value);
    *reinterpret_cast<int32_t*>(game+0xbcbd3f0+4*side)=gauge(side);
}
static void stub(uintptr_t rva, void* target) {
    auto at=reinterpret_cast<unsigned char*>(game+rva);
    DWORD old{},ignored{};
    assert(VirtualProtect(at,14,PAGE_EXECUTE_READWRITE,&old));
    unsigned char jump[14]={0xff,0x25,0,0,0,0};
    std::memcpy(jump+6,&target,8);std::memcpy(at,jump,14);
    assert(VirtualProtect(at,14,old,&ignored));
    FlushInstructionCache(GetCurrentProcess(),at,14);
}
int main(int argc,char**argv) {
    assert(argc==2);
    auto module=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    assert(module);game=reinterpret_cast<uintptr_t>(module);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(game+reinterpret_cast<IMAGE_DOS_HEADER*>(game)->e_lfanew);
    assert(nt->FileHeader.TimeDateStamp==0x6aa23d57);
    const unsigned char expected[]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10};
    assert(std::memcmp(reinterpret_cast<void*>(game+0x95cee0),expected,sizeof expected)==0);
    stub(0x92fca0,reinterpret_cast<void*>(&mode));
    stub(0x95c420,reinterpret_cast<void*>(&is_survival));
    stub(0x95c9e0,reinterpret_cast<void*>(&cancel));
    stub(0x8e94f0,reinterpret_cast<void*>(&manager));
    stub(0x8e9400,reinterpret_cast<void*>(&death));
    auto apply=reinterpret_cast<void(*)(int,int)>(game+0x95cee0);
    for(int side=0;side<2;++side) {
        *reinterpret_cast<uintptr_t*>(game+0xbcb66c8+16*side)=0;
        for(int low:{-1,0,1,49,50,99,100}) {
            survival=true;gauge(side)=5000;apply(side,low-5000);
            assert(gauge(side)==(low<100?0:low));
            apply(side,1000);assert(gauge(side)==(low<100?0:1100));
        }
        survival=false;gauge(side)=1000;apply(side,-5000);
        assert(gauge(side)==100); // Ordinary gauge retains native 2% floor.
    }
    const int baseline_deaths=death_calls;
    accumulator=safetyhook::create_inline(apply,&experimental_accumulate);
    assert(accumulator);
    for(int side=0;side<2;++side) {
        survival=true;track.reset(no_fail::Gauge::hard);
        gauge(side)=5000;apply(side,-4950);
        assert(gauge(side)==50 && track.failed);
        apply(side,-50);assert(gauge(side)==0 && track.failed);
        apply(side,-5000);assert(gauge(side)==-5000);
        apply(side,6000);assert(gauge(side)==1000 && track.failed);
        assert(*reinterpret_cast<int32_t*>(game+0xbcbd3f0+4*side)==1000);
        assert(death_calls==baseline_deaths);
        survival=false;gauge(side)=1000;apply(side,-5000);
        assert(gauge(side)==100);
    }
    accumulator.reset();
    assert(std::memcmp(reinterpret_cast<void*>(game+0x95cee0),expected,sizeof expected)==0);
    std::puts("PASS: actual accumulator depletion/recovery, signed hook experiment, ordinary passthrough, two sides, hook restoration");
}
