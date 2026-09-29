#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace sbcoop {

using EntityId = std::uint64_t;
using WorldEpoch = std::uint32_t;
using SessionId = std::array<std::uint8_t, 16>;

struct Vec3 {
    float x{}, y{}, z{};
    bool operator==(const Vec3&) const = default;
};

struct Quaternion {
    float x{}, y{}, z{}, w{1.0F};
    bool operator==(const Quaternion&) const = default;
};

struct Transform {
    Vec3 position;
    Quaternion rotation;
    bool operator==(const Transform&) const = default;
};

// Project coordinates: metres; right-handed +X forward, +Y left, +Z up.
// Quaternion: x,y,z,w, Hamilton product, active rotation of column vectors.
// The native game conversion is intentionally not implemented yet.
inline bool valid_transform(const Transform& value) {
    constexpr float max_coordinate = 1'000'000.0F;
    const auto position_ok = [](float v) {
        return std::isfinite(v) && std::abs(v) <= max_coordinate;
    };
    if (!position_ok(value.position.x) || !position_ok(value.position.y) ||
        !position_ok(value.position.z)) {
        return false;
    }
    const auto& q = value.rotation;
    if (!std::isfinite(q.x) || !std::isfinite(q.y) ||
        !std::isfinite(q.z) || !std::isfinite(q.w)) {
        return false;
    }
    const double norm = static_cast<double>(q.x) * q.x + static_cast<double>(q.y) * q.y +
                        static_cast<double>(q.z) * q.z + static_cast<double>(q.w) * q.w;
    return std::abs(norm - 1.0) <= 0.001;
}

struct GameObjectHandle {
    std::uint32_t slot{};
    std::uint32_t generation{};
    bool operator==(const GameObjectHandle&) const = default;
    [[nodiscard]] bool valid() const { return slot != 0 && generation != 0; }
};

struct TransformSample {
    std::uint64_t sequence{};
    std::uint64_t timestamp_us{};
    std::uint32_t discontinuity{};
    Transform transform;
};

} // namespace sbcoop
