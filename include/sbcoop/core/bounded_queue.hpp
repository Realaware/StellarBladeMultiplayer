#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace sbcoop {

// Multiple producers/consumers. Overflow is explicit; callers choose whether
// to coalesce state or fail a session. close() rejects writes, but permits drain.
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) { throw std::invalid_argument("queue capacity is zero"); }
    }

    bool try_push(T value) {
        const std::lock_guard lock(mutex_);
        if (closed_ || queue_.size() >= capacity_) { return false; }
        queue_.push_back(std::move(value));
        return true;
    }

    std::optional<T> try_pop() {
        const std::lock_guard lock(mutex_);
        if (queue_.empty()) { return std::nullopt; }
        T value = std::move(queue_.front());
        queue_.pop_front();
        return value;
    }

    void close() { const std::lock_guard lock(mutex_); closed_ = true; }
    [[nodiscard]] std::size_t size() const {
        const std::lock_guard lock(mutex_);
        return queue_.size();
    }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<T> queue_;
    bool closed_{};
};

} // namespace sbcoop
