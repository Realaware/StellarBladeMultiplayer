#include "sbcoop/replication/entity_registry.hpp"

#include <limits>
#include <stdexcept>

namespace sbcoop {

EntityRegistry::EntityRegistry(WorldEpoch epoch, std::size_t capacity)
    : epoch_(epoch), capacity_(capacity) {
    if (epoch == 0 || capacity == 0) { throw std::invalid_argument("invalid registry bounds"); }
}

std::uint64_t EntityRegistry::handle_key(GameObjectHandle handle) {
    return (static_cast<std::uint64_t>(handle.generation) << 32U) | handle.slot;
}

std::optional<EntityId> EntityRegistry::allocate_id() {
    if (next_id_ == std::numeric_limits<EntityId>::max()) { return std::nullopt; }
    return next_id_++;
}

BindResult EntityRegistry::bind(WorldEpoch epoch, EntityId id, GameObjectHandle handle) {
    if (epoch != epoch_) { return BindResult::wrong_epoch; }
    if (id == 0 || id == std::numeric_limits<EntityId>::max() || !handle.valid()) {
        return BindResult::invalid;
    }
    if (by_id_.contains(id)) { return BindResult::duplicate_id; }
    if (id <= highest_bound_id_) { return BindResult::stale_id; }
    const auto key = handle_key(handle);
    if (by_handle_.contains(key)) { return BindResult::duplicate_handle; }
    if (by_id_.size() >= capacity_) { return BindResult::full; }
    by_id_.emplace(id, handle);
    try {
        by_handle_.emplace(key, id);
    } catch (...) {
        by_id_.erase(id);
        throw;
    }
    // Binding IDs received from a host must not collide with later allocations.
    if (id >= next_id_) { next_id_ = id + 1; }
    highest_bound_id_ = id;
    return BindResult::bound;
}

bool EntityRegistry::unbind(WorldEpoch epoch, EntityId id) {
    if (epoch != epoch_) { return false; }
    const auto it = by_id_.find(id);
    if (it == by_id_.end()) { return false; }
    by_handle_.erase(handle_key(it->second));
    by_id_.erase(it);
    return true;
}

std::optional<GameObjectHandle> EntityRegistry::find(WorldEpoch epoch, EntityId id) const {
    if (epoch != epoch_) { return std::nullopt; }
    const auto it = by_id_.find(id);
    if (it == by_id_.end()) { return std::nullopt; }
    return it->second;
}

std::optional<EntityId> EntityRegistry::find_id(WorldEpoch epoch, GameObjectHandle handle) const {
    if (epoch != epoch_) { return std::nullopt; }
    const auto it = by_handle_.find(handle_key(handle));
    if (it == by_handle_.end()) { return std::nullopt; }
    return it->second;
}

bool EntityRegistry::advance_epoch(WorldEpoch next) {
    if (next <= epoch_) { return false; }
    by_id_.clear();
    by_handle_.clear();
    epoch_ = next;
    return true;
}

} // namespace sbcoop
