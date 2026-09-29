#pragma once

#include "sbcoop/core/types.hpp"

#include <cstddef>
#include <optional>
#include <unordered_map>

namespace sbcoop {

enum class BindResult { bound, wrong_epoch, invalid, duplicate_id, duplicate_handle, stale_id, full };

// Game-thread owned. Construct a new registry for each session. Network IDs
// are host-allocated, monotonic and never reused, including after an epoch change.
// Spawns must arrive in increasing ID order on the future reliable lifecycle lane.
class EntityRegistry {
public:
    explicit EntityRegistry(WorldEpoch epoch, std::size_t capacity = 256);
    std::optional<EntityId> allocate_id();
    BindResult bind(WorldEpoch epoch, EntityId id, GameObjectHandle handle);
    bool unbind(WorldEpoch epoch, EntityId id);
    std::optional<GameObjectHandle> find(WorldEpoch epoch, EntityId id) const;
    std::optional<EntityId> find_id(WorldEpoch epoch, GameObjectHandle handle) const;
    bool advance_epoch(WorldEpoch next);
    [[nodiscard]] std::size_t size() const { return by_id_.size(); }
    [[nodiscard]] WorldEpoch epoch() const { return epoch_; }

private:
    static std::uint64_t handle_key(GameObjectHandle handle);
    WorldEpoch epoch_;
    std::size_t capacity_;
    EntityId next_id_{1};
    EntityId highest_bound_id_{};
    std::unordered_map<EntityId, GameObjectHandle> by_id_;
    std::unordered_map<std::uint64_t, EntityId> by_handle_;
};

} // namespace sbcoop
