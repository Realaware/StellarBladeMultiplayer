#include "sbcoop/core/bounded_queue.hpp"
#include "sbcoop/protocol/codec.hpp"
#include "sbcoop/replication/entity_registry.hpp"
#include "sbcoop/replication/snapshot_buffer.hpp"
#include "sbcoop/testing/fake_game_adapter.hpp"

#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace sbcoop;
namespace wire = sbcoop::protocol;

void check(bool value, const char* expression, int line) {
    if (!value) { throw std::runtime_error("line " + std::to_string(line) + ": " + expression); }
}
#define CHECK(...) check(static_cast<bool>((__VA_ARGS__)), #__VA_ARGS__, __LINE__)

void near(double actual, double expected, double epsilon = 0.00001) {
    if (std::abs(actual - expected) > epsilon) {
        throw std::runtime_error("expected " + std::to_string(expected) + ", got " + std::to_string(actual));
    }
}

template <typename F>
void invalid_argument(F operation) {
    bool threw = false;
    try { operation(); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

wire::Envelope envelope() {
    SessionId session{};
    session[0] = 1;
    return {session, 1, 1, 0};
}

std::vector<std::byte> hex_bytes(const char* text) {
    std::istringstream input(text);
    unsigned int value;
    std::vector<std::byte> result;
    while (input >> std::hex >> value) { result.push_back(static_cast<std::byte>(value)); }
    return result;
}

void put_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes.at(offset + i) = static_cast<std::byte>((value >> (8U * i)) & 0xFFU);
    }
}

void protocol_golden_vector() {
    auto context = envelope();
    context.timestamp_us = 0x1122334455667788ULL;
    const wire::Packet packet{context, wire::Spawn{0x0102030405060708ULL}};
    const auto expected = hex_bytes(
        "53 42 4D 50 00 00 01 00 01 00 00 00 08 00 00 00 "
        "01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 "
        "01 00 00 00 01 00 00 00 00 00 00 00 "
        "88 77 66 55 44 33 22 11 08 07 06 05 04 03 02 01");
    CHECK(expected.size() == 60);
    CHECK(wire::encode(packet) == expected);
    const auto decoded = wire::decode(expected);
    CHECK(decoded);
    CHECK(std::get<wire::Spawn>(decoded.packet->message).entity == 0x0102030405060708ULL);
    CHECK(decoded.packet->envelope.timestamp_us == context.timestamp_us);
}

void protocol_roundtrips_and_truncation() {
    const Transform transform{{1.25F, -2.5F, 3.75F}, {0, 0, 1, 0}};
    const std::vector<wire::Message> messages{
        wire::Spawn{7}, wire::Despawn{7}, wire::TransformMessage{7, 3, transform}
    };
    for (const auto& message : messages) {
        const auto encoded = wire::encode({envelope(), message});
        const auto decoded = wire::decode(encoded);
        CHECK(decoded);
        CHECK(wire::encode(*decoded.packet) == encoded);
        for (std::size_t length = 0; length < encoded.size(); ++length) {
            CHECK(!wire::decode(std::span(encoded).first(length)));
        }
        auto extra = encoded;
        extra.push_back(std::byte{0});
        CHECK(wire::decode(extra).error == wire::DecodeError::bad_length);
    }
    const auto encoded = wire::encode({envelope(), wire::TransformMessage{7, 3, transform}});
    CHECK(encoded.size() == 92);
    const auto decoded = wire::decode(encoded);
    CHECK(std::get<wire::TransformMessage>(decoded.packet->message).transform == transform);
}

void protocol_rejects_malformed() {
    const auto original = wire::encode({envelope(), wire::TransformMessage{1, 0, {}}});
    auto reject_byte = [&](std::size_t offset, unsigned char value, wire::DecodeError error) {
        auto bytes = original;
        bytes.at(offset) = static_cast<std::byte>(value);
        CHECK(wire::decode(bytes).error == error);
    };
    reject_byte(0, 0, wire::DecodeError::bad_magic);
    reject_byte(4, 1, wire::DecodeError::incompatible_version);
    reject_byte(6, 2, wire::DecodeError::incompatible_version);
    reject_byte(8, 99, wire::DecodeError::unknown_type);
    reject_byte(10, 1, wire::DecodeError::invalid_flags);
    reject_byte(12, 0, wire::DecodeError::bad_length);
    reject_byte(16, 0, wire::DecodeError::invalid_context);
    reject_byte(32, 0, wire::DecodeError::invalid_context);
    reject_byte(36, 0, wire::DecodeError::invalid_context);
    reject_byte(52, 0, wire::DecodeError::invalid_entity);
    auto bytes = original;
    put_u32(bytes, 64, 0x7F800000U); // +infinity in position.x
    CHECK(wire::decode(bytes).error == wire::DecodeError::invalid_transform);
    bytes = original;
    put_u32(bytes, 88, 0); // zero quaternion
    CHECK(wire::decode(bytes).error == wire::DecodeError::invalid_transform);
    bytes = original;
    put_u32(bytes, 64, 0x7FC00000U); // quiet NaN
    CHECK(wire::decode(bytes).error == wire::DecodeError::invalid_transform);
    bytes.resize(wire::max_packet_size + 1);
    CHECK(wire::decode(bytes).error == wire::DecodeError::too_large);
    invalid_argument([] { wire::encode({envelope(), wire::Spawn{0}}); });
    invalid_argument([] { wire::encode({{}, wire::Spawn{1}}); });
    invalid_argument([] {
        Transform invalid{};
        invalid.rotation.w = 2;
        wire::encode({envelope(), wire::TransformMessage{1, 0, invalid}});
    });
}

void protocol_deterministic_mutation_corpus() {
    // Bounded regression corpus, not a substitute for a coverage-guided fuzzer.
    std::mt19937 random(0x53424D50U);
    const auto seed = wire::encode({envelope(), wire::TransformMessage{7, 0, {}}});
    for (int iteration = 0; iteration < 10'000; ++iteration) {
        auto bytes = seed;
        if (iteration % 3 == 0) { bytes.resize(static_cast<std::size_t>(random() % 1100U)); }
        if (!bytes.empty()) {
            const auto offset = static_cast<std::size_t>(random()) % bytes.size();
            bytes[offset] = static_cast<std::byte>(random() & 0xFFU);
        }
        const auto decoded = wire::decode(bytes);
        if (decoded) { CHECK(wire::encode(*decoded.packet) == bytes); }
        else { CHECK(decoded.error != wire::DecodeError::none); }
    }
}

void registry_lifecycle() {
    EntityRegistry registry(1, 2);
    CHECK(registry.allocate_id() == 1);
    const GameObjectHandle first{1, 1};
    const GameObjectHandle second{2, 1};
    CHECK(registry.bind(1, 10, first) == BindResult::bound);
    CHECK(registry.bind(1, 10, second) == BindResult::duplicate_id);
    CHECK(registry.bind(1, 11, first) == BindResult::duplicate_handle);
    CHECK(registry.bind(1, 11, second) == BindResult::bound);
    CHECK(registry.bind(1, 12, {3, 1}) == BindResult::full);
    CHECK(registry.bind(2, 12, {3, 1}) == BindResult::wrong_epoch);
    CHECK(registry.find(1, 10) == first);
    CHECK(registry.find_id(1, second) == 11);
    CHECK(!registry.find(2, 10));
    CHECK(registry.unbind(1, 10));
    CHECK(!registry.unbind(1, 10));
    CHECK(registry.bind(1, 10, {1, 2}) == BindResult::stale_id);
    CHECK(!registry.find_id(1, first));
    CHECK(!registry.advance_epoch(1));
    CHECK(registry.advance_epoch(2));
    CHECK(registry.size() == 0);
    CHECK(!registry.find(1, 11));
    CHECK(!registry.find_id(2, second));
    CHECK(registry.allocate_id() == 12);
    CHECK(registry.bind(2, 12, {1, 2}) == BindResult::bound);
    CHECK(registry.bind(2, 11, second) == BindResult::stale_id);
    CHECK(registry.bind(2, 13, {}) == BindResult::invalid);
    invalid_argument([] { EntityRegistry invalid(0); });
}

void snapshot_interpolation_and_extrapolation() {
    SnapshotBuffer buffer;
    CHECK(!buffer.sample_at(0));
    CHECK(buffer.push({1, 0, 0, {{0, 0, 0}, {}}}));
    CHECK(buffer.push({2, 100'000, 0, {{1, 0, 0}, {0, 0, 1, 0}}}));
    const auto middle = buffer.sample_at(50'000);
    CHECK(middle->mode == SampleMode::interpolated);
    near(middle->transform.position.x, 0.5);
    near(middle->transform.rotation.z, std::sqrt(0.5));
    near(middle->transform.rotation.w, std::sqrt(0.5));
    const auto projected = buffer.sample_at(150'000);
    CHECK(projected->mode == SampleMode::extrapolated);
    near(projected->transform.position.x, 1.5);
    CHECK(projected->transform.rotation == Quaternion{0, 0, 1, 0});
    const auto clamped = buffer.sample_at(1'000'000);
    CHECK(clamped->mode == SampleMode::extrapolation_clamped);
    near(clamped->transform.position.x, 2.0);
    CHECK(valid_transform(clamped->transform));
    buffer.clear();
    CHECK(buffer.size() == 0);
    CHECK(!buffer.sample_at(500));
}

void snapshot_sequence_teleport_and_bounds() {
    SnapshotBuffer buffer(2);
    CHECK(buffer.push({1, 10, 0, {}}));
    CHECK(!buffer.push({1, 20, 0, {}}));
    CHECK(!buffer.push({2, 10, 0, {}}));
    CHECK(buffer.push({2, 20, 0, {{1, 0, 0}, {}}}));
    CHECK(buffer.push({3, 30, 0, {{2, 0, 0}, {}}}));
    CHECK(buffer.size() == 2);
    near(buffer.sample_at(0)->transform.position.x, 1);
    CHECK(buffer.push({4, 40, 1, {{100, 0, 0}, {}}}));
    CHECK(buffer.size() == 1);
    near(buffer.sample_at(35)->transform.position.x, 100);
    CHECK(!buffer.push({5, 50, 0, {}}));
    CHECK(!buffer.push({3, 50, 1, {}}));
    CHECK(!buffer.push({5, 50, 1, {{}, {0, 0, 0, 0}}}));
    CHECK(buffer.push({5, 50, 1, {{101, 0, 0}, {}}}));
    invalid_argument([] { SnapshotBuffer invalid(1); });
    invalid_argument([] { SnapshotBuffer invalid(32, 100'001); });
}

void snapshot_quaternion_and_numeric_edges() {
    SnapshotBuffer buffer;
    CHECK(buffer.push({1, 0, 0, {{}, {0, 0, 0, 1}}}));
    CHECK(buffer.push({2, 100'000, 0, {{}, {0, 0, 0, -1}}}));
    const auto middle = buffer.sample_at(50'000);
    near(std::abs(middle->transform.rotation.w), 1.0);
    CHECK(valid_transform(middle->transform));
    buffer.clear();
    CHECK(buffer.push({1, 0, 0, {{999'999, 0, 0}, {}}}));
    CHECK(buffer.push({2, 1, 0, {{1'000'000, 0, 0}, {}}}));
    CHECK(valid_transform(buffer.sample_at(std::numeric_limits<std::uint64_t>::max())->transform));
    Transform invalid{};
    invalid.position.x = std::numeric_limits<float>::infinity();
    CHECK(!buffer.push({3, 2, 0, invalid}));
    invalid.position.x = 0;
    invalid.rotation.w = std::numeric_limits<float>::quiet_NaN();
    CHECK(!valid_transform(invalid));
}

void adapter_handles_and_thread_ownership() {
    testing::FakeGameAdapter adapter(1);
    const auto first = adapter.create_visual_proxy({});
    CHECK(first);
    CHECK(!adapter.create_visual_proxy({}));
    CHECK(adapter.destroy_proxy(*first));
    CHECK(!adapter.destroy_proxy(*first));
    const auto second = adapter.create_visual_proxy({});
    CHECK(second);
    CHECK(second->slot == first->slot);
    CHECK(second->generation != first->generation);
    CHECK(!adapter.apply_proxy_transform(*first, {}));
    bool rejected = false;
    std::thread other([&] {
        try { adapter.is_alive(*second); } catch (const std::logic_error&) { rejected = true; }
    });
    other.join();
    CHECK(rejected);
    adapter.invalidate_world();
    CHECK(!adapter.is_alive(*second));
    CHECK(adapter.live_count() == 0);
}

void queue_capacity_and_close() {
    BoundedQueue<int> queue(2);
    CHECK(!queue.try_pop());
    CHECK(queue.try_push(1));
    CHECK(queue.try_push(2));
    CHECK(!queue.try_push(3));
    CHECK(queue.size() == 2);
    queue.close();
    CHECK(!queue.try_push(4));
    CHECK(queue.try_pop() == 1);
    CHECK(queue.try_pop() == 2);
    CHECK(!queue.try_pop());
    invalid_argument([] { BoundedQueue<int> invalid(0); });
}

void queue_concurrent_delivery() {
    constexpr int producers = 4;
    constexpr int count = 2000;
    BoundedQueue<int> queue(32);
    std::atomic<int> finished{};
    std::vector<std::thread> threads;
    for (int producer = 0; producer < producers; ++producer) {
        threads.emplace_back([&, producer] {
            for (int i = 0; i < count; ++i) {
                while (!queue.try_push(producer * count + i)) { std::this_thread::yield(); }
            }
            ++finished;
        });
    }
    std::vector<int> seen(producers * count, 0);
    bool invalid = false;
    while (finished.load() != producers || queue.size() != 0) {
        if (const auto value = queue.try_pop()) {
            if (*value < 0 || *value >= producers * count) { invalid = true; }
            else { ++seen[static_cast<std::size_t>(*value)]; }
        } else { std::this_thread::yield(); }
    }
    for (auto& thread : threads) { thread.join(); }
    CHECK(!invalid);
    for (const auto times : seen) { CHECK(times == 1); }
}

} // namespace

int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests{
        {"protocol golden vector", protocol_golden_vector},
        {"protocol round trips and every truncation", protocol_roundtrips_and_truncation},
        {"protocol malformed input", protocol_rejects_malformed},
        {"protocol deterministic mutation corpus", protocol_deterministic_mutation_corpus},
        {"registry lifecycle and epoch isolation", registry_lifecycle},
        {"snapshot interpolation and bounded extrapolation", snapshot_interpolation_and_extrapolation},
        {"snapshot sequence, teleport and capacity", snapshot_sequence_teleport_and_bounds},
        {"snapshot quaternion and numeric edge cases", snapshot_quaternion_and_numeric_edges},
        {"adapter lifetime and thread ownership", adapter_handles_and_thread_ownership},
        {"queue capacity and close", queue_capacity_and_close},
        {"queue concurrent delivery", queue_concurrent_delivery}
    };
    std::size_t failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS: " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL: " << name << ": " << error.what() << '\n'; }
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " test groups passed\n";
    return failures == 0 ? 0 : 1;
}
