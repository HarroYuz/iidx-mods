#pragma once
#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace self_dan {
// The game thread never waits for the disk or for the consumer's mutex.
class TraceQueue {
public:
    static constexpr size_t capacity_bytes = 1024 * 1024;
    void push(std::string line) noexcept {
        try {
            std::unique_lock<std::mutex> guard(mutex_, std::try_to_lock);
            if (!guard.owns_lock() || line.size() > capacity_bytes - bytes_) {
                ++dropped_;
                return;
            }
            const auto length = line.size();
            lines_.push_back(std::move(line));
            bytes_ += length;
        } catch (...) { ++dropped_; }
    }
    std::vector<std::string> take() {
        std::vector<std::string> result;
        std::lock_guard<std::mutex> guard(mutex_);
        result.swap(lines_);
        bytes_ = 0;
        return result;
    }
    size_t take_dropped() { return dropped_.exchange(0); }
private:
    std::mutex mutex_;
    std::vector<std::string> lines_;
    size_t bytes_ = 0;
    std::atomic<size_t> dropped_{0};
};
} // namespace self_dan
