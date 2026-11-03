#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace self_dan {
// Independent course rules. Native game integration is not implemented yet.
enum class Phase { idle, selecting, playing, result };
struct Chart {
    uint32_t music_id{};
    int chart_index{};
    int level{}; // Numeric chart level, independent of N/H/A/L classification.
};
struct StageResult {
    Chart chart{};
    int end_health_raw{}; // Native 0..5000 units, retained without display rounding.
    int attainment{}; // Supplied by the native Dan calculation, not EX-score rate.
};
class Course {
public:
    static constexpr int stage_count = 4;
    static constexpr int initial_health = 5000; // 100 percent; 50 native units per percent.
    bool enter() {
        if (phase_ != Phase::idle) return false;
        completed_ = 0;
        health_ = initial_health;
        minimum_level_ = 1;
        rows_ = {};
        selected_.reset();
        phase_ = Phase::selecting;
        return true;
    }
    bool can_select(const Chart& c) const {
        return phase_ == Phase::selecting && c.music_id != 0 &&
            c.chart_index >= 0 && c.level >= minimum_level_ && c.level <= 12;
    }
    bool start(const Chart& c) {
        if (!can_select(c)) return false;
        selected_ = c;
        phase_ = Phase::playing;
        return true;
    }
    bool finish(int end_health_raw, int attainment) {
        if (phase_ != Phase::playing || !selected_ ||
            end_health_raw < 0 || end_health_raw > initial_health || attainment < 0 || attainment > 100) return false;
        rows_[completed_++] = {*selected_, end_health_raw, attainment};
        minimum_level_ = selected_->level;
        health_ = end_health_raw;
        selected_.reset();
        phase_ = (health_ == 0 || completed_ == stage_count) ? Phase::result : Phase::selecting;
        return true;
    }
    void leave() { *this = Course{}; }
    Phase phase() const { return phase_; }
    int completed() const { return completed_; }
    int health_raw() const { return health_; }
    int displayed_health() const { return 2 * (health_ / 100); }
    int minimum_level() const { return minimum_level_; }
    const std::array<StageResult, stage_count>& results() const { return rows_; }
private:
    Phase phase_ = Phase::idle;
    int completed_ = 0;
    int health_ = initial_health;
    int minimum_level_ = 1;
    std::optional<Chart> selected_;
    std::array<StageResult, stage_count> rows_{};
};
} // namespace self_dan
