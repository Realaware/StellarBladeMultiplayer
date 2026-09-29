#include "sbcoop/protocol/codec.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace sbcoop::protocol {
namespace {

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
constexpr std::uint32_t magic = 0x504D4253; // Wire bytes: S B M P

template <typename T>
void write_integer(std::vector<std::byte>& out, T value) {
    static_assert(std::is_unsigned_v<T>);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<std::byte>((value >> (i * 8U)) & 0xFFU));
    }
}

void write_float(std::vector<std::byte>& out, float value) {
    write_integer(out, std::bit_cast<std::uint32_t>(value));
}

struct Truncated {};
class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}
    template <typename T> T integer() {
        static_assert(std::is_unsigned_v<T>);
        if (bytes_.size() - position_ < sizeof(T)) { throw Truncated{}; }
        std::uint64_t result = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            result |= static_cast<std::uint64_t>(std::to_integer<unsigned char>(bytes_[position_++])) << (i * 8U);
        }
        return static_cast<T>(result);
    }
    float floating() { return std::bit_cast<float>(integer<std::uint32_t>()); }

private:
    std::span<const std::byte> bytes_;
    std::size_t position_{};
};

bool valid_context(const Envelope& envelope) {
    return envelope.epoch != 0 && envelope.sequence != 0 &&
           std::any_of(envelope.session.begin(), envelope.session.end(), [](auto v) { return v != 0; });
}

bool valid_entity(EntityId id) { return id != 0 && id != std::numeric_limits<EntityId>::max(); }
DecodeResult fail(DecodeError error) { return {std::nullopt, error}; }

} // namespace

std::vector<std::byte> encode(const Packet& packet) {
    if (!valid_context(packet.envelope)) { throw std::invalid_argument("invalid packet context"); }
    const EntityId id = std::visit([](const auto& message) { return message.entity; }, packet.message);
    if (!valid_entity(id)) { throw std::invalid_argument("invalid entity ID"); }
    MessageType type;
    std::uint32_t length = 8;
    if (std::holds_alternative<Spawn>(packet.message)) {
        type = MessageType::spawn;
    } else if (std::holds_alternative<Despawn>(packet.message)) {
        type = MessageType::despawn;
    } else {
        type = MessageType::transform;
        length = 40;
        if (!valid_transform(std::get<TransformMessage>(packet.message).transform)) {
            throw std::invalid_argument("invalid transform");
        }
    }
    std::vector<std::byte> bytes;
    bytes.reserve(header_size + length);
    write_integer(bytes, magic);
    write_integer(bytes, version_major);
    write_integer(bytes, version_minor);
    write_integer(bytes, static_cast<std::uint16_t>(type));
    write_integer(bytes, std::uint16_t{0});
    write_integer(bytes, length);
    for (const auto value : packet.envelope.session) { write_integer(bytes, value); }
    write_integer(bytes, packet.envelope.epoch);
    write_integer(bytes, packet.envelope.sequence);
    write_integer(bytes, packet.envelope.timestamp_us);
    write_integer(bytes, id);
    if (const auto* message = std::get_if<TransformMessage>(&packet.message)) {
        write_integer(bytes, message->discontinuity);
        const auto& p = message->transform.position;
        const auto& q = message->transform.rotation;
        write_float(bytes, p.x); write_float(bytes, p.y); write_float(bytes, p.z);
        write_float(bytes, q.x); write_float(bytes, q.y); write_float(bytes, q.z); write_float(bytes, q.w);
    }
    return bytes;
}

DecodeResult decode(std::span<const std::byte> bytes) {
    if (bytes.size() > max_packet_size) { return fail(DecodeError::too_large); }
    if (bytes.size() < header_size) { return fail(DecodeError::too_short); }
    try {
        Reader reader(bytes);
        if (reader.integer<std::uint32_t>() != magic) { return fail(DecodeError::bad_magic); }
        const auto major = reader.integer<std::uint16_t>();
        const auto minor = reader.integer<std::uint16_t>();
        if (major != version_major || minor != version_minor) { return fail(DecodeError::incompatible_version); }
        const auto raw_type = reader.integer<std::uint16_t>();
        if (reader.integer<std::uint16_t>() != 0) { return fail(DecodeError::invalid_flags); }
        const auto length = reader.integer<std::uint32_t>();
        if (length != bytes.size() - header_size) { return fail(DecodeError::bad_length); }
        const auto type = static_cast<MessageType>(raw_type);
        if (type != MessageType::spawn && type != MessageType::despawn && type != MessageType::transform) {
            return fail(DecodeError::unknown_type);
        }
        const std::size_t expected_length = type == MessageType::transform ? 40 : 8;
        if (length != expected_length) { return fail(DecodeError::bad_length); }
        Envelope envelope{};
        for (auto& value : envelope.session) { value = reader.integer<std::uint8_t>(); }
        envelope.epoch = reader.integer<std::uint32_t>();
        envelope.sequence = reader.integer<std::uint64_t>();
        envelope.timestamp_us = reader.integer<std::uint64_t>();
        if (!valid_context(envelope)) { return fail(DecodeError::invalid_context); }
        const EntityId id = reader.integer<EntityId>();
        if (!valid_entity(id)) { return fail(DecodeError::invalid_entity); }
        if (type == MessageType::spawn) { return {Packet{envelope, Spawn{id}}, DecodeError::none}; }
        if (type == MessageType::despawn) { return {Packet{envelope, Despawn{id}}, DecodeError::none}; }
        TransformMessage message{};
        message.entity = id;
        message.discontinuity = reader.integer<std::uint32_t>();
        message.transform.position = {reader.floating(), reader.floating(), reader.floating()};
        message.transform.rotation = {reader.floating(), reader.floating(), reader.floating(), reader.floating()};
        if (!valid_transform(message.transform)) { return fail(DecodeError::invalid_transform); }
        return {Packet{envelope, message}, DecodeError::none};
    } catch (const Truncated&) {
        return fail(DecodeError::too_short);
    }
}

std::string_view error_name(DecodeError error) {
    switch (error) {
    case DecodeError::none: return "none";
    case DecodeError::too_short: return "too_short";
    case DecodeError::too_large: return "too_large";
    case DecodeError::bad_magic: return "bad_magic";
    case DecodeError::incompatible_version: return "incompatible_version";
    case DecodeError::invalid_flags: return "invalid_flags";
    case DecodeError::unknown_type: return "unknown_type";
    case DecodeError::bad_length: return "bad_length";
    case DecodeError::invalid_context: return "invalid_context";
    case DecodeError::invalid_entity: return "invalid_entity";
    case DecodeError::invalid_transform: return "invalid_transform";
    }
    return "unknown_error";
}

} // namespace sbcoop::protocol
