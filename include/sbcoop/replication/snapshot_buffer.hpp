#pragma once

#include "sbcoop/core/types.hpp"

#include <cstddef>
#include <deque>
#include <optional>

namespace sbcoop {

enum class SampleMode { held, interpolated, extrapolated, extrapolation_clamped };
struct SampledTransform { Transform transform; SampleMode mode; };

// Timestamps must already be in one monotonic clock domain. Clock sync is a
// separate, not-yet-implemented session concern. Clear on epoch/entity changes.
class SnapshotBuffer {
public:
    explicit SnapshotBuffer(std::size_t capacity = 32, std::uint64_t max_extrapolation_us = 100'000);
    bool push(const TransformSample& sample);
    std::optional<SampledTransform> sample_at(std::uint64_t timestamp_us) const;
    void clear() { samples_.clear(); }
    [[nodiscard]] std::size_t size() const { return samples_.size(); }

private:
    std::size_t capacity_;
    std::uint64_t max_extrapolation_us_;
    std::deque<TransformSample> samples_;
};

} // namespace sbcoop
