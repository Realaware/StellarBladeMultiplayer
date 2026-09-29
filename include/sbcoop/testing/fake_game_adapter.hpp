#pragma once

#include "sbcoop/game/game_adapter.hpp"

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace sbcoop::testing {

// An in-memory test double. It neither launches nor inspects Stellar Blade.
class FakeGameAdapter final : public IGameAdapter {
public:
    explicit FakeGameAdapter(std::size_t capacity = 256)
        : owner_(std::this_thread::get_id()), capacity_(capacity) {
        if (capacity == 0 || capacity > 65'536) { throw std::invalid_argument("invalid fake capacity"); }
    }

    std::optional<GameObjectHandle> create_visual_proxy(const Transform& initial) override {
        require_owner();
        if (!valid_transform(initial)) { return std::nullopt; }
        for (std::size_t i = 0; i < slots_.size(); ++i) {
            auto& slot = slots_[i];
            if (!slot.alive && slot.generation != std::numeric_limits<std::uint32_t>::max()) {
                slot.alive = true;
                slot.transform = initial;
                return GameObjectHandle{static_cast<std::uint32_t>(i + 1), slot.generation};
            }
        }
        if (slots_.size() >= capacity_) { return std::nullopt; }
        slots_.push_back({1, true, initial});
        return GameObjectHandle{static_cast<std::uint32_t>(slots_.size()), 1};
    }

    bool apply_proxy_transform(GameObjectHandle handle, const Transform& transform) override {
        require_owner();
        if (!is_alive(handle) || !valid_transform(transform)) { return false; }
        slots_[handle.slot - 1].transform = transform;
        return true;
    }

    bool destroy_proxy(GameObjectHandle handle) override {
        require_owner();
        if (!is_alive(handle)) { return false; }
        auto& slot = slots_[handle.slot - 1];
        slot.alive = false;
        ++slot.generation; // max generation is retired, never allocated again.
        return true;
    }

    bool is_alive(GameObjectHandle handle) const override {
        require_owner();
        return handle.valid() && handle.slot <= slots_.size() && slots_[handle.slot - 1].alive &&
               slots_[handle.slot - 1].generation == handle.generation;
    }

    std::optional<Transform> read_transform(GameObjectHandle handle) const {
        require_owner();
        if (!is_alive(handle)) { return std::nullopt; }
        return slots_[handle.slot - 1].transform;
    }

    void invalidate_world() {
        require_owner();
        for (auto& slot : slots_) {
            if (slot.alive) { slot.alive = false; ++slot.generation; }
        }
    }

    [[nodiscard]] std::size_t live_count() const {
        require_owner();
        std::size_t count = 0;
        for (const auto& slot : slots_) { if (slot.alive) { ++count; } }
        return count;
    }

private:
    struct Slot { std::uint32_t generation; bool alive; Transform transform; };
    void require_owner() const {
        if (std::this_thread::get_id() != owner_) { throw std::logic_error("game adapter called off owning thread"); }
    }
    std::thread::id owner_;
    std::size_t capacity_;
    std::vector<Slot> slots_;
};

} // namespace sbcoop::testing
