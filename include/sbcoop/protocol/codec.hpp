#pragma once

#include "sbcoop/core/types.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace sbcoop::protocol {

constexpr std::uint16_t version_major = 0;
constexpr std::uint16_t version_minor = 1;
constexpr std::size_t header_size = 52;
constexpr std::size_t max_packet_size = 1024;

enum class MessageType : std::uint16_t { spawn = 1, despawn = 2, transform = 3 };

struct Envelope {
    SessionId session;
    WorldEpoch epoch{};
    std::uint64_t sequence{};
    std::uint64_t timestamp_us{};
};

struct Spawn { EntityId entity{}; };
struct Despawn { EntityId entity{}; };
struct TransformMessage {
    EntityId entity{};
    std::uint32_t discontinuity{};
    Transform transform;
};
using Message = std::variant<Spawn, Despawn, TransformMessage>;
struct Packet { Envelope envelope; Message message; };

enum class DecodeError {
    none, too_short, too_large, bad_magic, incompatible_version, invalid_flags,
    unknown_type, bad_length, invalid_context, invalid_entity, invalid_transform
};
struct DecodeResult {
    std::optional<Packet> packet;
    DecodeError error{DecodeError::none};
    explicit operator bool() const { return packet.has_value(); }
};

// Local programming errors throw invalid_argument. Untrusted decoding returns
// an explicit error. Parsing is NOT admission, authentication or ownership validation.
std::vector<std::byte> encode(const Packet& packet);
DecodeResult decode(std::span<const std::byte> bytes);
std::string_view error_name(DecodeError error);

} // namespace sbcoop::protocol
