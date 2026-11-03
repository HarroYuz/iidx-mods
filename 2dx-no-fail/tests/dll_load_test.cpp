#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../2dx-practice/src/addresses.hpp"
static int updates=0;
static void update(void*,bool,void*){++updates;}
int main(int argc,char**argv){
    assert(argc==3);setbuf(stdout,nullptr);SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    auto game=reinterpret_cast<uintptr_t>(LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES));
    assert(game && addresses::select(game));
    uintptr_t tables[]={0xd818d8,0xd82768,0xd7f898};
    for(auto table:tables){auto slot=reinterpret_cast<void**>(game+addresses::rva(table)+addresses::scene_slot(15)*8);DWORD old{},ignored{};
        assert(VirtualProtect(slot,8,PAGE_READWRITE,&old));*slot=reinterpret_cast<void*>(&update);assert(VirtualProtect(slot,8,old,&ignored));}
    auto dll=LoadLibraryA(argv[2]);if(!dll){std::printf("LoadLibrary failed: %lu\n",GetLastError());return 2;}
    auto slot=reinterpret_cast<void(**)(void*,bool,void*)>(game+addresses::rva(tables[0])+addresses::scene_slot(15)*8);
    for(int i=0;i<200 && *slot==&update;++i)Sleep(10);
    assert(*slot!=&update);(*slot)(nullptr,false,nullptr);assert(updates==1);
    unsigned(*status)(){};auto p=GetProcAddress(dll,"NoFailStatusV1");std::memcpy(&status,&p,sizeof status);
    assert(status && status()==1);
    assert(*reinterpret_cast<unsigned char*>(game+0x95bc20)==0xe9);
    std::puts("PASS: actual DLL load, worker bootstrap, installation on scene thread, original scene callback retained");
    // Keep modules mapped until process termination: the game owns hook lifetime.
    ExitProcess(0);
}
