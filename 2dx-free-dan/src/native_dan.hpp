#pragma once
#include "native_session.hpp"
#include "native_result.hpp"

namespace self_dan {
// Native callback implementations, connected by native_runtime.cpp in the
// experimental DLL. The separate trace launcher remains observation-only.
class NativeDan {
public:
    explicit NativeDan(uintptr_t verified_base):base_(verified_base) {}
    NativeSession& session(){return session_;}
    uint64_t initialize(void* flow,void* parameter,native::Init original);
    int first(void* flow,int (*original)(void*));
    void prepare(void* flow,int next,native::Prepare original);
    int next(void* flow,int previous,native::Next original);
    uint64_t result_initialize(void* scene,void* parameter,native::Init original);
    void result_update(void* scene,bool resumed,void* parameter,native::Update original);
    void result_render(void* scene,void (*original)(void*));
    int result_account(int side,int (*original)(int));
    bool result_joined(int side,bool (*original)(int));
    int course_music(void* manager,int grade,int stage,int style,int (*original)(void*,int,int,int));
    int course_difficulty(void* manager,int grade,int stage,int style,int side,int (*original)(void*,int,int,int));
    const char* result_title(const char* native_name,int part) const;
    void leave();
private:
    native::GradeManagerPrefix* grade_manager() const;
    bool custom_result() const;
    uintptr_t base_{};
    NativeSession session_;
    void* result_scene_{};
    unsigned result_depth_{};
    int result_side_{};
    bool fault_{};
};
} // namespace self_dan
