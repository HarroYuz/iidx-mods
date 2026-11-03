// Map executable sections for validation without running game initialization.
#define SELF_DAN_RUNTIME_TEST
#include "../src/native_runtime.cpp"
#include <cassert>
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    auto game=LoadLibraryExW(L"bm2dx.dll",nullptr,DONT_RESOLVE_DLL_REFERENCES);
    assert(game);base=reinterpret_cast<uintptr_t>(game);
    assert(addresses::select(base));
    assert(prepare());
    for(const auto& hook:slots)assert(*hook.slot==hook.expected);
    assert(install_bootstrap());
    for(const auto* hook:{&bootstrap,&standard_bootstrap,&mode_bootstrap})
        assert(*hook->slot==hook->replacement);
    owner_thread=GetCurrentThreadId();assert(publish(0));
    for(const auto& hook:calls)assert(self_dan::call_matches(hook,hook.after));
    for(const auto& hook:slots)assert(*hook.slot==hook.replacement);
    std::printf("PASS: %s; %zu direct branches, %zu lifecycle slots, 3 bootstrap slots\n",
        addresses::version(),calls.size(),slots.size());
}
