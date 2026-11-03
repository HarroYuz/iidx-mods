#pragma once
#include <cstdint>
#include <cstring>
#include <cstddef>

namespace self_dan::native {
// Prefix returned by 882200. Only fields touched by DAN_RESULT are named.
struct GradeManagerPrefix {
    uint8_t reserved0[9];
    uint8_t special_course;
    uint8_t reserved1[0x16];
    int32_t result_grade[2];
    int32_t special_result[2];
};
static_assert(offsetof(GradeManagerPrefix,special_course)==9);
static_assert(offsetof(GradeManagerPrefix,result_grade)==0x20);
static_assert(offsetof(GradeManagerPrefix,special_result)==0x28);
static_assert(sizeof(GradeManagerPrefix)==0x30);

// Scope the original result callback on its game thread. The single
// 8c81c3->948110 call must also be wrapped with account_for_result_save below;
// restoring manager fields alone cannot undo a submitted network score.
class GradeResultScope {
public:
    GradeResultScope(GradeManagerPrefix* manager,bool custom):manager_(custom?manager:nullptr) {
        if(!manager_)return;
        special_=manager_->special_course;
        std::memcpy(grades_,manager_->result_grade,sizeof(grades_));
        std::memcpy(special_grades_,manager_->special_result,sizeof(special_grades_));
        // Select normal Dan UI and bypass the separate special-course save path.
        manager_->special_course=0;
    }
    ~GradeResultScope() {
        if(!manager_)return;
        manager_->special_course=special_;
        std::memcpy(manager_->result_grade,grades_,sizeof(grades_));
        std::memcpy(manager_->special_result,special_grades_,sizeof(special_grades_));
    }
    GradeResultScope(const GradeResultScope&)=delete;
    GradeResultScope& operator=(const GradeResultScope&)=delete;
private:
    GradeManagerPrefix* manager_{};
    uint8_t special_{};
    int32_t grades_[2]{},special_grades_[2]{};
};

template<class Original> int account_for_result_save(bool custom,int side,Original&& original) {
    // Returning zero here uses native save-status 3, advancing the native result
    // state machine without submitting/polling/committing an official grade.
    return custom?0:original(side);
}
} // namespace self_dan::native
