#pragma once

#include "sbcoop/core/types.hpp"

#include <optional>

namespace sbcoop {

// Project-owned interface, NOT names or signatures from the game.
// All methods are game-thread only. Networking receives values, never handles.
class IGameAdapter {
public:
    virtual ~IGameAdapter() = default;
    virtual std::optional<GameObjectHandle> create_visual_proxy(const Transform& initial) = 0;
    virtual bool apply_proxy_transform(GameObjectHandle handle, const Transform& transform) = 0;
    virtual bool destroy_proxy(GameObjectHandle handle) = 0;
    virtual bool is_alive(GameObjectHandle handle) const = 0;
};

} // namespace sbcoop
