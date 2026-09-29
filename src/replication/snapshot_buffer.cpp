#include "sbcoop/replication/snapshot_buffer.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sbcoop {
namespace {

Vec3 lerp(Vec3 a, Vec3 b, double t) {
    return {
        static_cast<float>(a.x + (static_cast<double>(b.x) - a.x) * t),
        static_cast<float>(a.y + (static_cast<double>(b.y) - a.y) * t),
        static_cast<float>(a.z + (static_cast<double>(b.z) - a.z) * t)
    };
}

Quaternion slerp(Quaternion a, Quaternion b, double t) {
    auto normalize = [](Quaternion q) {
        const double n = std::sqrt(static_cast<double>(q.x) * q.x + static_cast<double>(q.y) * q.y +
                                   static_cast<double>(q.z) * q.z + static_cast<double>(q.w) * q.w);
        return Quaternion{static_cast<float>(q.x / n), static_cast<float>(q.y / n),
                          static_cast<float>(q.z / n), static_cast<float>(q.w / n)};
    };
    a = normalize(a);
    b = normalize(b);
    double dot = static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y +
                 static_cast<double>(a.z) * b.z + static_cast<double>(a.w) * b.w;
    if (dot < 0.0) {
        b = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }
    dot = std::clamp(dot, 0.0, 1.0);
    double wa = 1.0 - t;
    double wb = t;
    if (dot < 0.9995) {
        const double angle = std::acos(dot);
        const double divisor = std::sin(angle);
        wa = std::sin((1.0 - t) * angle) / divisor;
        wb = std::sin(t * angle) / divisor;
    }
    return normalize({static_cast<float>(wa * a.x + wb * b.x),
                      static_cast<float>(wa * a.y + wb * b.y),
                      static_cast<float>(wa * a.z + wb * b.z),
                      static_cast<float>(wa * a.w + wb * b.w)});
}

} // namespace

SnapshotBuffer::SnapshotBuffer(std::size_t capacity, std::uint64_t max_extrapolation_us)
    : capacity_(capacity), max_extrapolation_us_(max_extrapolation_us) {
    if (capacity < 2 || capacity > 1024 || max_extrapolation_us > 100'000) {
        throw std::invalid_argument("invalid snapshot buffer bounds");
    }
}

bool SnapshotBuffer::push(const TransformSample& sample) {
    if (sample.sequence == 0 || !valid_transform(sample.transform)) { return false; }
    if (!samples_.empty()) {
        const auto& previous = samples_.back();
        if (sample.sequence <= previous.sequence || sample.timestamp_us <= previous.timestamp_us ||
            sample.discontinuity < previous.discontinuity) {
            return false;
        }
        if (sample.discontinuity != previous.discontinuity) { samples_.clear(); }
    }
    samples_.push_back(sample);
    if (samples_.size() > capacity_) { samples_.pop_front(); }
    return true;
}

std::optional<SampledTransform> SnapshotBuffer::sample_at(std::uint64_t timestamp_us) const {
    if (samples_.empty()) { return std::nullopt; }
    if (timestamp_us <= samples_.front().timestamp_us) {
        return SampledTransform{samples_.front().transform, SampleMode::held};
    }
    for (std::size_t i = 1; i < samples_.size(); ++i) {
        const auto& right = samples_[i];
        if (timestamp_us <= right.timestamp_us) {
            const auto& left = samples_[i - 1];
            const double t = static_cast<double>(timestamp_us - left.timestamp_us) /
                             static_cast<double>(right.timestamp_us - left.timestamp_us);
            return SampledTransform{{lerp(left.transform.position, right.transform.position, t),
                                     slerp(left.transform.rotation, right.transform.rotation, t)},
                                    SampleMode::interpolated};
        }
    }
    const auto& last = samples_.back();
    if (samples_.size() < 2 || max_extrapolation_us_ == 0) {
        return SampledTransform{last.transform, SampleMode::held};
    }
    const auto& previous = samples_[samples_.size() - 2];
    const auto requested = timestamp_us - last.timestamp_us;
    const auto duration = std::min(requested, max_extrapolation_us_);
    const double t = 1.0 + static_cast<double>(duration) /
                          static_cast<double>(last.timestamp_us - previous.timestamp_us);
    Transform projected{lerp(previous.transform.position, last.transform.position, t), last.transform.rotation};
    // Extrapolation may go beyond the accepted coordinate envelope. Never feed
    // invalid results to an adapter, even if all input snapshots were valid.
    if (!valid_transform(projected)) { return SampledTransform{last.transform, SampleMode::held}; }
    return SampledTransform{projected, requested > max_extrapolation_us_
                                         ? SampleMode::extrapolation_clamped
                                         : SampleMode::extrapolated};
}

} // namespace sbcoop
