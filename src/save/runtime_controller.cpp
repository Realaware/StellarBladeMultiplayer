#include "sbcoop/save/runtime_controller.hpp"

#include <limits>
#include <stdexcept>

namespace sbcoop::save {
RuntimeController::RuntimeController(IFlagStore& store, VerifyBackup verify, MakeTransaction make_id,
                                     std::uint64_t epoch, std::size_t capacity)
    : epoch_(epoch), system_(store), verify_(std::move(verify)), make_id_(std::move(make_id)), input_(capacity) {
    if (epoch == 0 || !verify_ || !make_id_) { throw std::invalid_argument("invalid save runtime configuration"); }
    snapshot_.epoch = epoch_;
    worker_ = std::jthread([this](std::stop_token stop) { run(stop); });
}
RuntimeController::~RuntimeController() { close(); }
void RuntimeController::fail(RuntimeFailure failure) {
    auto expected = RuntimeFailure::none;
    fault_.compare_exchange_strong(expected, failure);
    const std::lock_guard lock(wake_mutex_);
    wake_.notify_one();
}
SubmitResult RuntimeController::enqueue(std::uint64_t epoch, Event event, bool invalidate) {
    const std::lock_guard producer_lock(enqueue_mutex_);
    if (epoch != epoch_) { return SubmitResult::stale_epoch; }
    if (closed_ || fault_ != RuntimeFailure::none) { return SubmitResult::closed; }
    if (invalidate) {
        auto ticket = next_ticket_.load();
        do {
            if (ticket == std::numeric_limits<std::uint64_t>::max()) {
                fail(RuntimeFailure::sequence_exhausted); return SubmitResult::sequence_exhausted;
            }
        } while (!next_ticket_.compare_exchange_weak(ticket, ticket + 1));
        event.ticket = ticket + 1;
        // Producer serialization keeps the fence and queue order consistent.
        auto latest = latest_ticket_.load();
        while (latest < event.ticket && !latest_ticket_.compare_exchange_weak(latest, event.ticket)) {}
    }
    if (!input_.try_push(std::move(event))) { fail(RuntimeFailure::overflow); return SubmitResult::overflow; }
    const std::lock_guard lock(wake_mutex_);
    wake_.notify_one();
    return SubmitResult::accepted;
}
SubmitResult RuntimeController::observe(std::uint64_t epoch, Observation observation) {
    if (epoch != epoch_) { return SubmitResult::stale_epoch; }
    if (closed_ || fault_ != RuntimeFailure::none) { return SubmitResult::closed; }
    if (!valid_observation(observation)) { fail(RuntimeFailure::invalid_observation); return SubmitResult::invalid_observation; }
    const std::lock_guard lock(ingress_mutex_);
    if (observation.generation < latest_observation_.generation) { return SubmitResult::stale_generation; }
    if (observation.generation == latest_observation_.generation) {
        if (observation == latest_observation_) { return SubmitResult::accepted; }
        fail(RuntimeFailure::invalid_observation); return SubmitResult::invalid_observation;
    }
    latest_observation_ = observation;
    latest_generation_ = observation.generation;
    Event event; event.kind = EventKind::observation; event.observation = std::move(observation);
    return enqueue(epoch, std::move(event));
}
SubmitResult RuntimeController::submit(std::uint64_t epoch, RuntimeAction action, std::uint64_t expected_generation) {
    Event event; event.kind = action == RuntimeAction::create ? EventKind::create : EventKind::refresh;
    event.expected_generation = expected_generation;
    return enqueue(epoch, std::move(event));
}
SubmitResult RuntimeController::complete(std::uint64_t epoch, CreationReceipt receipt, Observation after) {
    if (epoch != epoch_) { return SubmitResult::stale_epoch; }
    if (closed_ || fault_ != RuntimeFailure::none) { return SubmitResult::closed; }
    if (!valid_observation(after) || !valid_id(receipt.transaction_id) ||
        receipt.created.account.size() > 256 || receipt.created.slot.size() > 256 ||
        receipt.created.instance.size() > 256 || receipt.created.profile.size() > 256) {
        fail(RuntimeFailure::invalid_observation); return SubmitResult::invalid_observation;
    }
    const std::lock_guard lock(ingress_mutex_);
    if (after.generation < latest_observation_.generation) {
        creation_failed(epoch, receipt.transaction_id); return SubmitResult::stale_generation;
    }
    if (after.generation == latest_observation_.generation && after != latest_observation_) {
        fail(RuntimeFailure::invalid_observation); return SubmitResult::invalid_observation;
    }
    latest_observation_ = after;
    latest_generation_ = after.generation;
    Event event; event.kind = EventKind::receipt; event.receipt = std::move(receipt); event.observation = std::move(after);
    return enqueue(epoch, std::move(event));
}
SubmitResult RuntimeController::creation_failed(std::uint64_t epoch, std::string transaction_id) {
    if (epoch != epoch_) { return SubmitResult::stale_epoch; }
    if (closed_ || fault_ != RuntimeFailure::none) { return SubmitResult::closed; }
    if (!valid_id(transaction_id)) { fail(RuntimeFailure::invalid_observation); return SubmitResult::invalid_observation; }
    Event event; event.kind = EventKind::failed; event.transaction = std::move(transaction_id);
    return enqueue(epoch, std::move(event));
}
std::optional<CreationRequest> RuntimeController::pop_creation() {
    if (closed_ || fault_ != RuntimeFailure::none) { return std::nullopt; }
    auto request = creations_.try_pop();
    if (!request) { return std::nullopt; }
    if (request->request.observed_generation != latest_generation_ || request->ticket != latest_ticket_) {
        creation_failed(epoch_, request->request.transaction_id);
        return std::nullopt;
    }
    Event event; event.kind = EventKind::dispatched; event.transaction = request->request.transaction_id;
    if (enqueue(epoch_, std::move(event), false) != SubmitResult::accepted) { return std::nullopt; }
    return std::move(request->request);
}
RuntimeSnapshot RuntimeController::snapshot() const {
    const std::lock_guard lock(snapshot_mutex_);
    auto result = snapshot_;
    result.failure = closed_ ? RuntimeFailure::closed : fault_.load();
    result.checking = result.failure == RuntimeFailure::none && result.request_id != latest_ticket_;
    if (result.failure != RuntimeFailure::none || result.request_id != latest_ticket_ ||
        result.observation_generation != latest_generation_) {
        result.creation_enabled = false;
        result.decision = {Reason::stale_observation, std::nullopt, 0};
        if (result.failure == RuntimeFailure::none) { result.reason = Reason::stale_observation; }
    }
    return result;
}
bool RuntimeController::permit_matches(const Observation& current) const {
    const auto state = snapshot();
    return state.failure == RuntimeFailure::none && !state.busy && SaveSystem::permit_matches(state.decision, current);
}
void RuntimeController::close() {
    if (closed_.exchange(true)) { return; }
    input_.close(); creations_.close();
    worker_.request_stop(); wake_.notify_all();
    if (worker_.joinable()) { worker_.join(); }
}
void RuntimeController::run(std::stop_token stop) {
    try {
        while (!stop.stop_requested() && fault_ == RuntimeFailure::none) {
            if (auto event = input_.try_pop()) { process(std::move(*event)); continue; }
            std::unique_lock lock(wake_mutex_);
            wake_.wait(lock, stop, [this] { return input_.size() != 0 || fault_ != RuntimeFailure::none; });
        }
    } catch (...) { fail(RuntimeFailure::worker_exception); }
    cancel_creation(); // preserve unfinished intent across shutdown/error
}
void RuntimeController::cancel_creation() {
    system_.fail_creation(); dispatched_ = false;
    while (creations_.try_pop()) {}
}
void RuntimeController::publish(std::uint64_t ticket, Reason reason) {
    RuntimeSnapshot next;
    next.epoch = epoch_; next.request_id = ticket; next.observation_generation = observed_.generation;
    next.backup = backup_; next.decision = decision_; next.reason = reason;
    next.busy = system_.busy();
    next.creation_enabled = !closed_ && fault_ == RuntimeFailure::none &&
        system_.creation_readiness(observed_, backup_) == Reason::allowed;
    next.loaded_slot = observed_.loaded ? observed_.loaded->slot : observed_.state == LoadedState::none ? "None" : "Unknown";
    const std::lock_guard lock(snapshot_mutex_);
    snapshot_ = std::move(next);
}
void RuntimeController::refresh(std::uint64_t ticket) {
    decision_ = {}; // a cached permission never survives a fresh check
    verify_backup();
    if (!closed_ && fault_ == RuntimeFailure::none && !system_.busy()) { decision_ = system_.check_loaded(observed_, backup_); }
    publish(ticket, decision_.reason);
}
void RuntimeController::verify_backup() {
    backup_ = verify_();
    if (backup_.check_id == 0 || backup_.check_id <= last_backup_check_) {
        backup_.state = BackupState::unknown; backup_.archive_sha256.clear(); return;
    }
    last_backup_check_ = backup_.check_id;
}
void RuntimeController::process(Event event) {
    if (closed_ || fault_ != RuntimeFailure::none) { return; }
    if (event.kind == EventKind::dispatched) {
        const auto pending = system_.pending_request();
        if (pending && pending->transaction_id == event.transaction) { dispatched_ = true; }
        return;
    }
    if (event.kind == EventKind::failed) {
        const auto pending = system_.pending_request();
        if (pending && pending->transaction_id == event.transaction) { cancel_creation(); }
        decision_ = {}; publish(event.ticket, Reason::creation_failed); return;
    }
    if (event.kind == EventKind::observation) {
        if (event.observation.generation < observed_.generation) {
            decision_ = {}; publish(event.ticket, Reason::stale_observation); return;
        }
        observed_ = std::move(event.observation);
        if (system_.busy()) {
            if (!dispatched_) { cancel_creation(); }
            decision_ = {}; publish(event.ticket, Reason::stale_observation); return;
        }
        refresh(event.ticket); return;
    }
    if (event.kind == EventKind::receipt) {
        const bool current = event.observation.generation >= observed_.generation;
        if (current) { observed_ = event.observation; }
        if (!dispatched_ || !current) {
            cancel_creation();
            decision_ = {}; publish(event.ticket, Reason::stale_observation); return;
        }
        verify_backup();
        if (closed_ || fault_ != RuntimeFailure::none) { return; }
        if (!valid_backup_check(backup_)) {
            cancel_creation();
            decision_ = {}; publish(event.ticket, Reason::backup_unverified); return;
        }
        if (event.observation.generation != latest_generation_) {
            cancel_creation();
            decision_ = {}; publish(event.ticket, Reason::stale_observation); return;
        }
        const auto reason = system_.finish_creation(event.receipt, event.observation);
        if (reason == Reason::invalid_transaction) { decision_ = {}; publish(event.ticket, reason); return; }
        dispatched_ = false; observed_ = std::move(event.observation);
        while (creations_.try_pop()) {}
        decision_ = reason == Reason::allowed ? system_.check_loaded(observed_, backup_) : Decision{};
        publish(event.ticket, reason); return;
    }
    if (event.expected_generation != observed_.generation || observed_.generation != latest_generation_) {
        decision_ = {}; publish(event.ticket, Reason::stale_observation); return;
    }
    if (event.kind == EventKind::refresh) { refresh(event.ticket); return; }
    decision_ = {};
    verify_backup();
    if (closed_ || fault_ != RuntimeFailure::none) { return; }
    if (observed_.generation != latest_generation_) { publish(event.ticket, Reason::stale_observation); return; }
    const auto reason = system_.begin_creation(observed_, backup_, make_id_());
    if (reason == Reason::allowed) {
        dispatched_ = false;
        if (!creations_.try_push({*system_.pending_request(), event.ticket})) { fail(RuntimeFailure::overflow); cancel_creation(); }
    }
    publish(event.ticket, reason);
}
} // namespace sbcoop::save
