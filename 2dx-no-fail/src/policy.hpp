#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>

namespace no_fail {
// Same option IDs as GSM. Native units: 50 = 1%.
enum class Gauge : unsigned { normal, assisted, easy, hard, exhard };
constexpr unsigned index(Gauge g) { return static_cast<unsigned>(g); }
constexpr bool hard(Gauge g) { return g == Gauge::hard || g == Gauge::exhard; }
// The native display is 2 * (raw / 100). Survival gauges deplete below
// 100 raw units, including the positive remainder that displays as 0%.
constexpr std::int64_t survival_minimum = 100;
constexpr std::array<int, 5> rank{2, 0, 1, 3, 4};
constexpr std::array<Gauge, 5> descending{
    Gauge::exhard, Gauge::hard, Gauge::normal, Gauge::easy, Gauge::assisted};

struct Track {
    std::int64_t value{};
    bool failed{};
    void reset(Gauge type) { value = hard(type) ? 5000 : 1100; failed = false; }
    // delta must come from the native judgement calculation before clamping.
    // Never infer delta from the clamped gauge or run scoring callbacks twice.
    void apply(Gauge type, std::int32_t delta) {
        if (hard(type)) failed = failed || value < survival_minimum;
        constexpr auto floor = std::numeric_limits<std::int64_t>::min();
        value = delta < 0 && value < floor - delta ? floor : value + delta;
        value = std::min<std::int64_t>(5000, value);
        if (hard(type)) failed = failed || value < survival_minimum;
        // 0917 ordinary gauges retain the original 2% (100-unit) floor.
        else value = std::max<std::int64_t>(100, value);
    }
    bool clear(Gauge type) const {
        if (hard(type)) return !failed && value >= survival_minimum;
        return value >= (type == Gauge::assisted ? 3000 : 4000);
    }
    bool red() const { return value < 0; }
};

struct Player {
    std::array<Track, 5> tracks{};
    Gauge starting = Gauge::exhard;
    Gauge bottom = Gauge::assisted;
    bool automatic = false;
    bool externally_blocked = false;

    void reset(Gauge selected, Gauge lowest, bool shift) {
        starting = selected; bottom = lowest; automatic = shift;
        externally_blocked = false;
        for (auto g : descending) tracks[index(g)].reset(g);
    }
    bool allowed(Gauge g) const {
        if (!automatic) return g == starting;
        const int minimum = std::min(rank[index(starting)], rank[index(bottom)]);
        return rank[index(g)] <= rank[index(starting)] && rank[index(g)] >= minimum;
    }
    std::optional<Gauge> best_clear() const {
        for (auto g : descending)
            if (allowed(g) && tracks[index(g)].clear(g)) return g;
        return std::nullopt;
    }
    // A failed automatic attempt still displays its bottom ordinary gauge.
    // Upload eligibility is separate: a fallback display is not a clear.
    Gauge display() const {
        if (!automatic) return starting;
        if (auto best = best_clear()) return *best;
        return rank[index(starting)] < rank[index(bottom)] ? starting : bottom;
    }
    std::optional<Gauge> upload_clear() const {
        if (externally_blocked) return std::nullopt;
        return best_clear();
    }
};

// Screen coordinates grow downwards. Preserve the original 0..100% scale;
// negative values intentionally extend below bottom. No graph-bound clipping.
inline double graph_y(std::int64_t value, double bottom, double height) {
    return bottom - static_cast<double>(value) * height / 5000.0;
}
} // namespace no_fail
