#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../src/native_calls.hpp"
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* message) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static int replacement(int a,int b,int c,int d,int e,int f) {return a+b*2+c*3+d*4+e*5+f*6;}
int main() {
    using namespace self_dan;
    auto* page=static_cast<uint8_t*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    check(page!=nullptr,"allocate private test code");
    // Function frame with Win64 shadow space and two stack arguments. This
    // exercises the complete relay ABI rather than merely comparing bytes.
    const uint8_t code[]={0x48,0x83,0xec,0x38, // sub rsp,38
        0x48,0x8b,0x44,0x24,0x60,0x48,0x89,0x44,0x24,0x20, // argument 5
        0x48,0x8b,0x44,0x24,0x68,0x48,0x89,0x44,0x24,0x28, // argument 6
        0xe8,0,0,0,0,0x48,0x83,0xc4,0x38,0xc3};
    std::memcpy(page,code,sizeof(code));
    const uint8_t original[]={0xb8,7,0,0,0,0xc3};std::memcpy(page+128,original,sizeof(original));
    uint8_t call[5]{};check(relative_call(page+24,page+128,call),"relative encoding");std::memcpy(page+24,call,5);
    DWORD old{};check(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old),"test code RX");
    check(FlushInstructionCache(GetCurrentProcess(),page,4096),"flush test code");
    using Fn=int(*)(int,int,int,int,int,int);auto invoke=reinterpret_cast<Fn>(page);
    check(invoke(1,2,3,4,5,6)==7,"baseline dispatch");
    CallHook hook{};hook.site=page+24;hook.expected_target=page+128;hook.replacement=reinterpret_cast<void*>(&replacement);
    check(prepare_call(hook),"prepare nearby relay");check(install_calls(&hook,1),"publish call hook");
    check(invoke(1,2,3,4,5,6)==91,"register and stack arguments survive relay");
    MEMORY_BASIC_INFORMATION region{};check(VirtualQuery(page,&region,sizeof(region)) && region.Protect==PAGE_EXECUTE_READ,"restore RX protection");
    check(!release_call(hook),"published relay cannot be released");
    check(restore_calls(&hook,1) && invoke(1,2,3,4,5,6)==7,"restore original call");
    check(release_call(hook),"release unpublished relay");
    CallHook bad{};bad.site=page+24;bad.expected_target=page+129;bad.replacement=reinterpret_cast<void*>(&replacement);
    check(!prepare_call(bad) && invoke(0,0,0,0,0,0)==7,"unexpected target preserved");
    check(prepare_call(hook),"prepare again");
    CallHook duplicate=hook;CallHook batch[]={hook,duplicate};
    check(!install_calls(batch,2) && invoke(0,0,0,0,0,0)==7,"overlapping calls rejected before mutation");
    check(install_calls(&hook,1),"reinstall");
    check(VirtualProtect(page,4096,PAGE_EXECUTE_READWRITE,&old),"simulate foreign patch");
    page[25]^=1;
    check(VirtualProtect(page,4096,old,&old),"restore simulated foreign protection");
    check(!restore_calls(&hook,1) && hook.installed,"foreign call replacement preserved");
    check(VirtualProtect(page,4096,PAGE_EXECUTE_READWRITE,&old),"repair private fixture");
    std::memcpy(page+24,hook.after,5);
    check(VirtualProtect(page,4096,old,&old),"fixture RX");
    check(restore_calls(&hook,1) && release_call(hook),"clean fixture");
    check(VirtualProtect(page,4096,PAGE_EXECUTE_READWRITE,&old),"prepare tail-jump fixture");
    check(relative_call(page+160,page+128,call),"encode tail jump");call[0]=0xe9;
    std::memcpy(page+160,call,5);
    check(VirtualProtect(page,4096,old,&old),"tail fixture RX");
    check(FlushInstructionCache(GetCurrentProcess(),page+160,5),"flush tail fixture");
    CallHook tail{};tail.site=page+160;tail.expected_target=page+128;tail.replacement=reinterpret_cast<void*>(&replacement);tail.opcode=0xe9;
    auto tail_invoke=reinterpret_cast<Fn>(page+160);
    check(tail_invoke(1,2,3,4,5,6)==7 && prepare_call(tail) && install_calls(&tail,1),"publish verified E9 relay");
    check(tail_invoke(1,2,3,4,5,6)==91,"tail relay preserves six-argument ABI");
    check(restore_calls(&tail,1) && release_call(tail) && tail_invoke(0,0,0,0,0,0)==7,"restore original tail jump");
    VirtualFree(page,0,MEM_RELEASE);
    // Model a large game image: searching inside its reservation must skip
    // regions rather than issue thousands of queries per relay.
    constexpr size_t reserved_size=512*1024*1024;
    auto* image=static_cast<uint8_t*>(VirtualAlloc(nullptr,reserved_size,MEM_RESERVE,PAGE_NOACCESS));
    check(image!=nullptr,"reserve large image fixture");
    auto* sites=image+reserved_size/2;
    check(VirtualAlloc(sites,4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit call sites");
    CallHook many[132]{};
    for(size_t i=0;i<132;++i){
        auto& h=many[i];h.site=sites+i*8;h.expected_target=sites+2048;h.replacement=reinterpret_cast<void*>(&replacement);
        check(relative_call(h.site,h.expected_target,call),"encode batch fixture");std::memcpy(h.site,call,5);
    }
    RelaySearchStats stats{};const auto began=GetTickCount64();
    check(prepare_calls(many,132,&stats),"prepare all 132 relays together");
    check(stats.queries<32 && stats.allocation_attempts==1,"skip large reservation and allocate one block");
    const auto block=many[0].relay_allocation.get();
    for(const auto& h:many){
        check(h.relay_allocation.get()==block,"all relays share one allocation");
        check(VirtualQuery(h.relay,&region,sizeof(region)) && region.Protect==PAGE_EXECUTE_READ,"pooled relay RX");
    }
    for(size_t i=0;i<131;++i)check(release_call(many[i]),"release shared relay reference");
    check(VirtualQuery(block,&region,sizeof(region)) && region.State==MEM_COMMIT,"remaining relay keeps allocation alive");
    check(release_call(many[131]),"release last shared relay");
    check(VirtualQuery(block,&region,sizeof(region)) && region.State==MEM_FREE,"last reference frees block");
    VirtualFree(image,0,MEM_RELEASE);
    std::printf("PASS: 132 pooled relays around 512 MiB reservation: %zu queries, %zu allocation, %llu ms\n",
        stats.queries,stats.allocation_attempts,static_cast<unsigned long long>(GetTickCount64()-began));
    std::puts("PASS: native call relay ABI, rollback, validation and foreign hook conflicts");
}
