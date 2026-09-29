#include "sbcoop/protocol/codec.hpp"
#include "sbcoop/replication/entity_registry.hpp"
#include "sbcoop/replication/snapshot_buffer.hpp"
#include "sbcoop/testing/fake_game_adapter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace sbcoop;
namespace wire = sbcoop::protocol;

void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

SessionId test_session() { SessionId result{}; result[0] = 1; return result; }

Transform trajectory(EntityId id, std::uint64_t time_us) {
    const double t = static_cast<double>(time_us) / 1'000'000.0;
    const double speed = id == 1 ? 2.0 : -1.25;
    const double half_angle = t * 0.1;
    return {{static_cast<float>(speed * t), static_cast<float>(id), 0.0F},
            {0.0F, 0.0F, static_cast<float>(std::sin(half_angle)), static_cast<float>(std::cos(half_angle))}};
}

class SimulatedPeer {
public:
    explicit SimulatedPeer(EntityId expected_remote) : expected_remote_(expected_remote) {}

    bool receive(const std::vector<std::byte>& bytes) {
        auto decoded = wire::decode(bytes);
        if (!decoded) { ++rejected_; return false; }
        const auto& packet = *decoded.packet;
        if (packet.envelope.session != test_session() || packet.envelope.epoch != registry_.epoch()) {
            ++rejected_; return false;
        }
        if (const auto* spawn = std::get_if<wire::Spawn>(&packet.message)) {
            if (spawn->entity != expected_remote_ || registry_.find(registry_.epoch(), spawn->entity)) {
                ++rejected_; return false;
            }
            const auto handle = adapter_.create_visual_proxy(trajectory(spawn->entity, 0));
            if (!handle) { ++rejected_; return false; }
            if (registry_.bind(registry_.epoch(), spawn->entity, *handle) != BindResult::bound) {
                require(adapter_.destroy_proxy(*handle), "failed spawn rollback");
                ++rejected_; return false;
            }
            return true;
        }
        if (const auto* despawn = std::get_if<wire::Despawn>(&packet.message)) {
            const auto handle = registry_.find(registry_.epoch(), despawn->entity);
            if (!handle) { ++rejected_; return false; }
            require(adapter_.destroy_proxy(*handle), "failed proxy destruction");
            require(registry_.unbind(registry_.epoch(), despawn->entity), "failed registry cleanup");
            buffer_.clear();
            return true;
        }
        const auto& state = std::get<wire::TransformMessage>(packet.message);
        if (state.entity != expected_remote_ || !registry_.find(registry_.epoch(), state.entity)) {
            ++rejected_; return false;
        }
        const bool accepted = buffer_.push({packet.envelope.sequence, packet.envelope.timestamp_us,
                                            state.discontinuity, state.transform});
        if (accepted) { ++accepted_; } else { ++rejected_; }
        return accepted;
    }

    void render(std::uint64_t now_us) {
        constexpr std::uint64_t interpolation_delay_us = 100'000;
        const auto target = now_us > interpolation_delay_us ? now_us - interpolation_delay_us : 0;
        const auto sample = buffer_.sample_at(target);
        if (!sample) { return; }
        const auto handle = registry_.find(registry_.epoch(), expected_remote_);
        require(handle.has_value(), "sample without registered entity");
        require(adapter_.apply_proxy_transform(*handle, sample->transform), "invalid proxy transform");
        require(buffer_.size() <= 32, "unbounded snapshot buffer");
        ++rendered_;
        if (now_us >= 500'000) {
            const auto actual = adapter_.read_transform(*handle);
            require(actual.has_value(), "proxy missing after update");
            const auto expected = trajectory(expected_remote_, target);
            max_error_ = std::max(max_error_, std::abs(static_cast<double>(actual->position.x) - expected.position.x));
        }
    }

    void change_world() {
        adapter_.invalidate_world();
        require(registry_.advance_epoch(registry_.epoch() + 1), "epoch did not advance");
        buffer_.clear();
    }

    void verify(bool impaired) const {
        require(accepted_ > 100, "too few received transforms");
        require(rendered_ > 900, "too few render updates");
        require(max_error_ < (impaired ? 0.5 : 0.01), "movement exceeded error tolerance");
    }

    [[nodiscard]] bool empty() const { return registry_.size() == 0 && adapter_.live_count() == 0; }
    void report(const char* label) const {
        std::cout << label << ": accepted=" << accepted_ << " rejected=" << rejected_
                  << " rendered=" << rendered_ << " max_position_error_m=" << max_error_ << '\n';
    }

private:
    EntityId expected_remote_;
    testing::FakeGameAdapter adapter_;
    EntityRegistry registry_{1};
    SnapshotBuffer buffer_;
    std::size_t accepted_{}, rejected_{}, rendered_{};
    double max_error_{};
};

// Deterministic in-process delivery simulation. No UDP sockets, real game,
// session authentication, clock synchronization or reliable transport implied.
class SimulatedLink {
public:
    explicit SimulatedLink(bool impaired) : impaired_(impaired) {}
    void send(std::uint64_t now, int destination, std::vector<std::byte> bytes) {
        ++sent_;
        if (impaired_ && sent_ % 20 == 0) { ++dropped_; return; }
        std::uint64_t delay = 20'000;
        if (impaired_) {
            delay += (sent_ % 5) * 10'000;
            if (sent_ % 13 == 0) { delay += 90'000; }
        }
        require(pending_.size() < 128, "simulated link queue exceeded bound");
        if (impaired_ && sent_ % 11 == 0) { pending_.push_back({now + delay + 10'000, destination, bytes}); }
        pending_.push_back({now + delay, destination, std::move(bytes)});
    }

    void deliver(std::uint64_t now, SimulatedPeer& host, SimulatedPeer& guest) {
        std::stable_sort(pending_.begin(), pending_.end(), [](const auto& a, const auto& b) { return a.at < b.at; });
        while (!pending_.empty() && pending_.front().at <= now) {
            auto message = std::move(pending_.front());
            pending_.erase(pending_.begin());
            auto& target = message.destination == 0 ? host : guest;
            target.receive(message.bytes);
        }
    }
    [[nodiscard]] std::size_t dropped() const { return dropped_; }

private:
    struct Pending { std::uint64_t at; int destination; std::vector<std::byte> bytes; };
    bool impaired_;
    std::size_t sent_{}, dropped_{};
    std::vector<Pending> pending_;
};

int run(bool impaired) {
    SimulatedPeer host(2);
    SimulatedPeer guest(1);
    const wire::Envelope initial{test_session(), 1, 1, 0};
    require(host.receive(wire::encode({initial, wire::Spawn{2}})), "host spawn failed");
    require(guest.receive(wire::encode({initial, wire::Spawn{1}})), "guest spawn failed");
    SimulatedLink link(impaired);
    std::uint64_t sequence = 2;
    std::vector<std::byte> last_host_state;
    for (std::uint64_t now = 0; now <= 10'000'000; now += 10'000) {
        if (now % 50'000 == 0) { // Capture 20 Hz, render 100 Hz, all in virtual time.
            const wire::Envelope envelope{test_session(), 1, sequence++, now};
            last_host_state = wire::encode({envelope, wire::TransformMessage{1, 0, trajectory(1, now)}});
            link.send(now, 1, last_host_state);
            link.send(now, 0, wire::encode({envelope, wire::TransformMessage{2, 0, trajectory(2, now)}}));
        }
        link.deliver(now, host, guest);
        host.render(now);
        guest.render(now);
    }
    host.verify(impaired);
    guest.verify(impaired);
    const wire::Envelope end{test_session(), 1, sequence, 10'000'001};
    require(host.receive(wire::encode({end, wire::Despawn{2}})), "host cleanup failed");
    require(guest.receive(wire::encode({end, wire::Despawn{1}})), "guest cleanup failed");
    require(!guest.receive(last_host_state), "late state resurrected despawned entity");
    link.deliver(20'000'000, host, guest);
    require(host.empty() && guest.empty(), "proxy leak after disconnect");
    guest.change_world();
    require(!guest.receive(last_host_state), "old epoch was accepted");
    host.report("host");
    guest.report("guest");
    std::cout << "PASS: simulated peers, " << (impaired ? "impaired" : "clean")
              << " delivery; dropped=" << link.dropped() << "; no live proxies.\n"
              << "This is an in-process core test, not a running game or LAN/internet test.\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--impaired")) {
            std::cerr << "Usage: sbcoop_harness [--impaired]\n";
            return 2;
        }
        return run(argc == 2);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
