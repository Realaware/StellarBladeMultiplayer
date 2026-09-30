#pragma once

#include "sbcoop/core/bounded_queue.hpp"
#include "sbcoop/save/save_system.hpp"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace sbcoop::save {

enum class RuntimeAction { refresh, create };
enum class SubmitResult { accepted, stale_epoch, stale_generation, closed, overflow, invalid_observation, sequence_exhausted };
enum class RuntimeFailure { none, closed, overflow, invalid_observation, sequence_exhausted, worker_exception };

struct RuntimeSnapshot {
    std::uint64_t epoch{};
    std::uint64_t request_id{};
    std::uint64_t observation_generation{};
    BackupCheck backup;
    Decision decision;
    Reason reason{Reason::native_binding_unverified};
    RuntimeFailure failure{RuntimeFailure::none};
    bool busy{};
    bool checking{};
    bool creation_enabled{};
    std::string loaded_slot{"Unknown"};
};

// Filesystem worker owns SaveSystem and all store/verifier calls. UI and the
// game-thread adapter submit/consume values only. The store and function captures
// must outlive this controller. No native operation is implemented here.
class RuntimeController {
public:
    using VerifyBackup = std::function<BackupCheck()>;
    using MakeTransaction = std::function<std::string()>;
    RuntimeController(IFlagStore& store, VerifyBackup verify, MakeTransaction make_id,
                      std::uint64_t epoch, std::size_t capacity = 16);
    ~RuntimeController();
    RuntimeController(const RuntimeController&) = delete;
    RuntimeController& operator=(const RuntimeController&) = delete;

    // A host must advance generation for every changed identity/load/context
    // observation, and re-capture immediately before executing a popped request.
    SubmitResult observe(std::uint64_t epoch, Observation observation);
    SubmitResult submit(std::uint64_t epoch, RuntimeAction action, std::uint64_t expected_generation);
    SubmitResult complete(std::uint64_t epoch, CreationReceipt receipt, Observation after);
    SubmitResult creation_failed(std::uint64_t epoch, std::string transaction_id);
    std::optional<CreationRequest> pop_creation();
    RuntimeSnapshot snapshot() const;
    bool permit_matches(const Observation& current) const;
    void close();

private:
    enum class EventKind { observation, refresh, create, dispatched, receipt, failed };
    struct Event {
        EventKind kind{};
        std::uint64_t ticket{};
        std::uint64_t expected_generation{};
        Observation observation;
        CreationReceipt receipt;
        std::string transaction;
    };
    struct OutboundCreation {
        CreationRequest request;
        std::uint64_t ticket{};
    };
    SubmitResult enqueue(std::uint64_t epoch, Event event, bool invalidate = true);
    void run(std::stop_token stop);
    void process(Event event);
    void publish(std::uint64_t ticket, Reason reason);
    void refresh(std::uint64_t ticket);
    void verify_backup();
    void cancel_creation();
    void fail(RuntimeFailure failure);

    const std::uint64_t epoch_;
    SaveSystem system_;
    VerifyBackup verify_;
    MakeTransaction make_id_;
    BoundedQueue<Event> input_;
    BoundedQueue<OutboundCreation> creations_{1};
    mutable std::mutex snapshot_mutex_;
    RuntimeSnapshot snapshot_;
    std::mutex wake_mutex_;
    std::condition_variable_any wake_;
    std::atomic<bool> closed_{};
    std::atomic<RuntimeFailure> fault_{RuntimeFailure::none};
    std::atomic<std::uint64_t> next_ticket_{};
    std::atomic<std::uint64_t> latest_ticket_{};
    std::atomic<std::uint64_t> latest_generation_{};
    std::mutex enqueue_mutex_;
    std::mutex ingress_mutex_;
    Observation latest_observation_;
    Observation observed_; // filesystem worker-owned copies, never game objects
    BackupCheck backup_;
    std::uint64_t last_backup_check_{};
    Decision decision_;
    bool dispatched_{};
    std::jthread worker_;
};
} // namespace sbcoop::save
