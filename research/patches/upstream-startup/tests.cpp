// Compile the public header in an embedding context with a representative
// function-like engine assertion macro. A method named check would expand here.
#define check(expression) static_cast<void>(expression)
#include "StartupGuard.hpp"
#ifndef check
#error StartupGuard.hpp must preserve the embedding assertion macro
#endif
#undef check

#include <atomic>
#include <iostream>
#include <latch>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
    using namespace RC::Startup;
    using namespace std::chrono_literals;

    void require(bool condition, std::string_view message)
    {
        if (!condition)
        {
            throw std::runtime_error(std::string(message));
        }
    }

    template <typename Exception, typename Operation>
    void expect_throw(Operation&& operation, std::string_view message_fragment = {})
    {
        try
        {
            std::forward<Operation>(operation)();
        }
        catch (const Exception& error)
        {
            require(std::string_view(error.what()).find(message_fragment) != std::string_view::npos,
                    "failure did not identify the expected phase/reason");
            return;
        }
        throw std::runtime_error("expected failure did not occur");
    }

    void invalid_budgets()
    {
        const std::int64_t invalid[] = {std::numeric_limits<std::int64_t>::min(), -1, 0,
                                       601, std::numeric_limits<std::int64_t>::max()};
        for (const auto budget : invalid)
        {
            expect_throw<std::invalid_argument>([budget] { Guard guard(budget); });
        }
        Guard maximum(600);
        require(maximum.snapshot().budget_seconds == 600, "valid maximum budget was rejected");
    }

    void injected_start_time_keeps_deadline_representable()
    {
        expect_throw<std::out_of_range>([] { Guard guard(1, Clock::time_point::max()); });
        Guard latest(1, Clock::time_point::max() - 1s);
        require(latest.snapshot().deadline == Clock::time_point::max(), "latest representable deadline changed");
        Guard earliest(1, Clock::time_point::min());
        require(earliest.snapshot().deadline == Clock::time_point::min() + 1s,
                "earliest injected start produced an invalid deadline");
    }

    void immediate_readiness_and_successful_lifetime()
    {
        const auto start = Clock::time_point{};
        Guard guard(1, start);
        guard.check_phase("initial phase", start);
        guard.mark_bootstrap_complete(start);
        guard.mark_native_ready(start);
        guard.check_phase("event loop", start + 1h);
        const auto result = guard.snapshot();
        require(result.native_state == State::ready, "successful startup expired later");
        require(result.bootstrap_state == BootstrapState::released, "successful bootstrap did not release");
        require(result.failure_reason == FailureReason::none, "success carried a failure reason");
    }

    void bootstrap_is_not_native_readiness()
    {
        const auto start = Clock::time_point{};
        Guard guard(10, start);
        const auto before = guard.snapshot();
        guard.mark_bootstrap_complete(start + 1s);
        const auto released = guard.snapshot();
        require(before.native_state == State::pending && before.bootstrap_state == BootstrapState::pending,
                "guard did not start pending");
        require(released.native_state == State::pending && released.bootstrap_state == BootstrapState::released,
                "bootstrap release granted native readiness");
        require(before.bootstrap_state == BootstrapState::pending, "snapshot borrowed mutable guard state");
        require(guard.bootstrap_result() == BootstrapState::released, "bootstrap polling result disagreed");
        guard.check_phase("later prerequisite", start + 9s);
    }

    void never_ready_expires_without_downstream_initialization()
    {
        const auto start = Clock::time_point{};
        Guard guard(2, start);
        guard.mark_bootstrap_complete(start);
        guard.check_phase("FName verification", start + 1s);
        bool downstream_ran = false;
        expect_throw<std::runtime_error>([&] {
            guard.check_phase("FName verification", start + 2s);
            downstream_ran = true;
        }, "FName verification");
        const auto result = guard.snapshot();
        require(!downstream_ran, "expired startup advanced to a dependent stage");
        require(result.native_state == State::failed && result.bootstrap_state == BootstrapState::failed,
                "expiry did not publish terminal failure");
        require(result.failure_reason == FailureReason::expired, "expiry lost its reason");
        expect_throw<std::runtime_error>([&] { guard.check_phase("retry", start); }, "expired");
    }

    void readiness_at_or_after_deadline_is_rejected()
    {
        const auto start = Clock::time_point{};
        for (const auto arrival : {start + 3s, start + 3s + Clock::duration{1}, start + 4s})
        {
            Guard guard(3, start);
            expect_throw<std::runtime_error>([&] { guard.mark_native_ready(arrival); }, "native readiness");
            require(guard.snapshot().native_state == State::failed, "late readiness was accepted");
            expect_throw<std::runtime_error>([&] { guard.mark_native_ready(start); });
        }
        Guard before_deadline(3, start);
        before_deadline.mark_native_ready(start + 3s - Clock::duration{1});
        require(before_deadline.snapshot().native_state == State::ready, "readiness before the deadline failed");
    }

    void caller_failure_releases_bootstrap_and_rejects_late_completion()
    {
        const auto start = Clock::time_point{};
        Guard guard(5, start);
        try
        {
            throw std::runtime_error("synthetic bootstrap prerequisite failed");
        }
        catch (const std::runtime_error&)
        {
            // This exercises caller failure publication. It does not model
            // Windows thread creation, a native wait, or native teardown.
            guard.fail();
        }
        require(guard.bootstrap_result() == BootstrapState::failed, "caller failure did not release bootstrap");
        expect_throw<std::runtime_error>([&] { guard.mark_bootstrap_complete(start); }, "bootstrap completion");
        expect_throw<std::runtime_error>([&] { guard.mark_native_ready(start + 1s); }, "native readiness");
        expect_throw<std::runtime_error>([&] { guard.check_phase("caller failure", start); }, "caller failure");
        guard.fail();
        const auto result = guard.snapshot();
        require(result.native_state == State::failed && result.bootstrap_state == BootstrapState::failed,
                "late completion reopened a failed guard");
        require(result.failure_reason == FailureReason::explicit_failure, "failure reason was replaced");
    }

    void bootstrap_completion_at_deadline_fails()
    {
        const auto start = Clock::time_point{};
        Guard guard(1, start);
        expect_throw<std::runtime_error>([&] { guard.mark_bootstrap_complete(start + 1s); }, "bootstrap completion");
        require(guard.bootstrap_result() == BootstrapState::failed, "late bootstrap was released as success");
    }

    void explicit_failure_after_ready_is_terminal()
    {
        const auto start = Clock::time_point{};
        Guard guard(1, start);
        guard.mark_native_ready(start);
        guard.fail();
        expect_throw<std::runtime_error>([&] { guard.mark_native_ready(start); });
        expect_throw<std::runtime_error>([&] { guard.check_phase("event loop", start + 1h); });
        require(guard.snapshot().native_state == State::failed, "failure after readiness was reopened");
    }

    void concurrent_failure_and_readiness()
    {
        const auto start = Clock::time_point{};
        for (int attempt = 0; attempt < 128; ++attempt)
        {
            Guard guard(10, start);
            std::latch gate(1);
            std::atomic_bool unexpected{false};
            std::jthread completing([&] {
                gate.wait();
                try
                {
                    guard.mark_native_ready(start);
                }
                catch (const std::runtime_error&)
                {
                    // Failure won the race, so readiness must be rejected.
                }
                catch (...)
                {
                    unexpected.store(true);
                }
            });
            std::jthread failing;
            try
            {
                failing = std::jthread([&] {
                    gate.wait();
                    try
                    {
                        guard.fail();
                    }
                    catch (...)
                    {
                        unexpected.store(true);
                    }
                });
            }
            catch (...)
            {
                // Release an already-created worker before its owning jthread
                // joins during unwinding; its captured guard is still alive.
                gate.count_down();
                throw;
            }
            gate.count_down();
            completing.join();
            failing.join();
            require(!unexpected.load(), "concurrent readiness raised an unexpected error");
            const auto result = guard.snapshot();
            require(result.native_state == State::failed && result.bootstrap_state == BootstrapState::failed,
                    "concurrent completion overrode failure");
            expect_throw<std::runtime_error>([&] { guard.mark_native_ready(start); });
        }
    }
}

int main()
{
    struct Test
    {
        std::string_view name;
        void (*run)();
    };
    const Test tests[] = {
        {"invalid budgets", invalid_budgets},
        {"injected deadline representation", injected_start_time_keeps_deadline_representable},
        {"immediate readiness and successful lifetime", immediate_readiness_and_successful_lifetime},
        {"bootstrap is not native readiness", bootstrap_is_not_native_readiness},
        {"never-ready expiry blocks downstream initialization", never_ready_expires_without_downstream_initialization},
        {"deadline boundary rejects readiness", readiness_at_or_after_deadline_is_rejected},
        {"caller failure rejects late completion", caller_failure_releases_bootstrap_and_rejects_late_completion},
        {"late bootstrap fails", bootstrap_completion_at_deadline_fails},
        {"failure after ready is terminal", explicit_failure_after_ready_is_terminal},
        {"concurrent failure and readiness", concurrent_failure_and_readiness},
    };

    int failures = 0;
    for (const auto& test : tests)
    {
        try
        {
            test.run();
            std::cout << "PASS: " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failures;
            std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}
