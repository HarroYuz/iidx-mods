// Executes the real hook publisher against synthetic game memory. Optional
// layers model ABI-preserving DLL hooks; this does not run third-party DLLMain.
#define SELF_DAN_RUNTIME_TEST
#include "../src/native_runtime.cpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

static void check(bool ok,const char* text){if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
static void commit(uintptr_t at){check(VirtualAlloc(ptr(at&~uintptr_t(4095)),4096,MEM_COMMIT,PAGE_EXECUTE_READWRITE)!=nullptr,"commit");}
static void jump(uintptr_t at,uintptr_t to){
    commit(at);const uint8_t code[]{0xff,0x25,0,0,0,0};
    std::memcpy(ptr(at),code,6);std::memcpy(ptr(at+6),&to,8);
    check(FlushInstructionCache(GetCurrentProcess(),ptr(at),14),"flush stub");
}
static std::vector<int> order_seen;
static n::Init init_next[3]{};
static uint64_t native_init(void* scene,void* parameter){
    check(scene==ptr(0x1234) && parameter==ptr(0x5678),"init arguments");
    order_seen.push_back(9);return UINT64_C(0x123456789abcdef0);
}
template<int I> static uint64_t layer(void* scene,void* parameter){
    order_seen.push_back(I);auto result=init_next[I](scene,parameter);
    order_seen.push_back(I+10);return result+I+1;
}
static n::Update update_next[3]{};
static void native_update(void* scene,bool resumed,void* parameter){
    check(scene==ptr(0x1234) && resumed && parameter==ptr(0x5678),"update arguments");
    order_seen.push_back(9);
}
template<int I> static void update_layer(void* scene,bool resumed,void* parameter){
    order_seen.push_back(I);update_next[I](scene,resumed,parameter);order_seen.push_back(I+10);
}
using Availability=bool(*)(void*,int,int,int,int,bool,bool);
static Availability availability_next[3]{};
static Availability nested_site{};
static bool nested_call=false;
static int native_count{},branch_count[3]{};
static bool native_availability(void* manager,int mid,int diff,int style,int category,bool a,bool b){
    check(manager==ptr(0x8765) && mid==101 && diff==3 && style==1 && category==42 && a && !b,"seven arguments including stack arguments");
    ++native_count;return true;
}
template<int I> static bool branch_layer(void* m,int mid,int diff,int style,int category,bool a,bool b){
    ++branch_count[I];
    if constexpr(I==0){
        if(nested_call){
            const auto* parent=self_dan::ChainScope::active;
            check(nested_site(m,mid,diff,style,category,a,b),"nested site return");
            check(self_dan::ChainScope::active==parent,"nested site restores parent context");
        }
    }
    return availability_next[I](m,mid,diff,style,category,a,b);
}
static size_t branch_index(uintptr_t rva){
    for(size_t i=0;i<calls.size();++i)if(reinterpret_cast<uintptr_t>(calls[i].site)==base+rva)return i;
    std::abort();
}
static void set_branch(self_dan::CallHook& h,uintptr_t target){
    uint8_t bytes[5]{};check(self_dan::relative_call(h.site,ptr(target),bytes),"branch encodable");
    bytes[0]=h.opcode;std::memcpy(h.site,bytes,5);
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    base=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,n::image_size,MEM_RESERVE,PAGE_NOACCESS));check(base!=0,"reserve");
    commit(base+n::settings);
    #define BIND(purpose,site_rva,target,opcode_value) { \
        commit(base+site_rva);uint8_t bytes[5]{}; \
        check(self_dan::relative_call(reinterpret_cast<uint8_t*>(base+site_rva),ptr(base+target),bytes),"encode"); \
        bytes[0]=opcode_value;std::memcpy(ptr(base+site_rva),bytes,5); }
    #include "../src/native_bindings.inc"
    #undef BIND
    check(prepare(),"prepare actual integration");
    for(auto& h:slots){commit(reinterpret_cast<uintptr_t>(h.slot));*h.slot=h.expected;}
    jump(base+0x8ea560,reinterpret_cast<uintptr_t>(&native_init));
    jump(base+0x8d56b0,reinterpret_cast<uintptr_t>(&native_init));
    jump(base+0x8d6a80,reinterpret_cast<uintptr_t>(&native_update));
    jump(base+0x892ce0,reinterpret_cast<uintptr_t>(&native_availability));
    const std::array<n::Init,3> layers{&layer<0>,&layer<1>,&layer<2>};
    const std::array<n::Update,3> update_layers{&update_layer<0>,&update_layer<1>,&update_layer<2>};
    const std::array<Availability,3> branch_layers{&branch_layer<0>,&branch_layer<1>,&branch_layer<2>};
    const auto i=branch_index(0x5f8373),j=branch_index(0x5f83c3);
    auto& hook=calls[i];auto& other=calls[j];
    auto& init_slot=field<uintptr_t>(base,0xd82768+13*8);
    auto& bootstrap_slot=field<uintptr_t>(base,0xd7f898+15*8);
    owner_thread=GetCurrentThreadId();int cases=0;
    for(int mask=0;mask<8;++mask){
        std::vector<int> order;for(int k=0;k<3;++k)if(mask&(1<<k))order.push_back(k);
        do{
            published=false;integration_ready=false;
            init_slot=base+0x8ea560;uintptr_t branch_head=base+0x892ce0;
            uint64_t result=UINT64_C(0x123456789abcdef0);
            for(int k:order){
                init_next[k]=reinterpret_cast<n::Init>(init_slot);init_slot=reinterpret_cast<uintptr_t>(layers[k]);
                availability_next[k]=reinterpret_cast<Availability>(branch_head);
                jump(base+0x10000+k*0x100,reinterpret_cast<uintptr_t>(branch_layers[k]));branch_head=base+0x10000+k*0x100;
                result+=k+1;
            }
            const auto head=init_slot;set_branch(hook,branch_head);
            // Changes happened AFTER prepare; publication must capture them.
            check(publish(),"publish with any optional subset/order");
            order_seen.clear();
            check(reinterpret_cast<n::Init>(init_slot)(ptr(0x1234),ptr(0x5678))==result,"preserve 64-bit return value");
            std::vector<int> expected(order.rbegin(),order.rend());expected.push_back(9);
            for(int k:order)expected.push_back(k+10);
            check(order_seen==expected,"every predecessor called exactly once in chain order");
            native_count=0;std::fill(std::begin(branch_count),std::end(branch_count),0);
            check(reinterpret_cast<Availability>(hook.relay)(ptr(0x8765),101,3,1,42,true,false),"branch return");
            check(native_count==1,"native branch called exactly once");
            for(int k=0;k<3;++k)check(branch_count[k]==int(bool(mask&(1<<k))),"optional branch count");
            check(reinterpret_cast<Availability>(other.relay)(ptr(0x8765),101,3,1,42,true,false),"second site retains its own target");
            check(native_count==2 && self_dan::ChainScope::active==nullptr,"scope restored after call");
            for(int k=0;k<3;++k)check(branch_count[k]==int(bool(mask&(1<<k))),"no cross-site predecessor leakage");
            if(mask&1){
                native_count=0;nested_site=reinterpret_cast<Availability>(other.relay);nested_call=true;
                check(reinterpret_cast<Availability>(hook.relay)(ptr(0x8765),101,3,1,42,true,false),"nested chain return");
                check(native_count==2 && self_dan::ChainScope::active==nullptr,"nested chain reaches both native calls");
                nested_call=false;
            }
            check(self_dan::remove_hooks(slots.data(),slots.size()) && self_dan::restore_calls(calls.data(),calls.size()),"rollback");
            check(init_slot==head && self_dan::call_matches(hook,hook.before),"rollback keeps foreign hooks");
            bootstrap_slot=base+0x8d6a80;
            commit(base+0xd818d8);
            field<uintptr_t>(base,0xd82768+15*8)=base+0x8d6a80;
            field<uintptr_t>(base,0xd818d8+15*8)=base+0x8d6a80;
            for(int k:order){update_next[k]=reinterpret_cast<n::Update>(bootstrap_slot);bootstrap_slot=reinterpret_cast<uintptr_t>(update_layers[k]);}
            const auto update_head=bootstrap_slot;
            check(install_bootstrap(),"bootstrap chains optional update hook");
            // Avoid scene publication in this specific ABI test.
            published=true;order_seen.clear();
            reinterpret_cast<n::Update>(bootstrap_slot)(ptr(0x1234),true,ptr(0x5678));
            check(order_seen==expected,"bootstrap invokes all predecessors once");
            check(self_dan::remove_hooks(&bootstrap,1) && self_dan::remove_hooks(&standard_bootstrap,1) && self_dan::remove_hooks(&mode_bootstrap,1) && bootstrap_slot==update_head,"bootstrap rollback");
            ++cases;
        }while(std::next_permutation(order.begin(),order.end()));
    }
    published=false;
    for(size_t k=0;k<slots.size();++k)*slots[k].slot=ptr(base+slot_originals[k]);
    set_branch(hook,base+0x892ce0);
    // Non-code targets and changed instruction shape must fail atomically.
    init_slot=base+0x30000;
    check(!publish(),"non-executable slot rejected");init_slot=base+0x8ea560;
    hook.site[0]=0x90;check(!publish(),"non-branch modification rejected");
    for(const auto& h:slots)check(!h.installed,"no slots published on branch conflict");
    set_branch(hook,base+0x892ce0);check(publish(),"clean retry after previous conflict");
    // Do not clobber a hook installed by another module after publication.
    init_slot=reinterpret_cast<uintptr_t>(&layer<0>);
    check(!self_dan::remove_hooks(slots.data(),slots.size()) && init_slot==reinterpret_cast<uintptr_t>(&layer<0>),"preserve later foreign hook");
    init_slot=reinterpret_cast<uintptr_t>(&select_init);
    check(self_dan::remove_hooks(slots.data(),slots.size()) && self_dan::restore_calls(calls.data(),calls.size()),"cleanup");
    for(auto& h:calls)check(self_dan::release_call(h),"release relays");
    VirtualFree(ptr(base),0,MEM_RELEASE);
    std::printf("PASS: %d optional-subset/order cases, bootstrap/slot/branch chaining, ABI, rollback, conflict retry (synthetic)\n",cases);
}
