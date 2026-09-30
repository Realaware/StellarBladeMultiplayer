#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>

namespace RC::Startup
{
    using Clock = std::chrono::steady_clock;

    enum class State
    {
        pending,
        ready,
        failed,
    };

    enum class BootstrapState
    {
        pending,
        released,
        failed,
    };

    enum class FailureReason
    {
        none,
        expired,
        explicit_failure,
    };

    // A copied status record. It contains no borrowed state or native pointers.
    struct Snapshot
    {
        State native_state;
        BootstrapState bootstrap_state;
        FailureReason failure_reason;
        std::int64_t budget_seconds;
        Clock::time_point started_at;
        Clock::time_point deadline;
    };

    // Cooperative startup checks cannot interrupt a synchronous native call or
    // prove that its callbacks/threads have stopped. Keep this guard alive until
    // every user of it has finished; a failed state does not authorize unload.
    class Guard final
    {
      public:
        explicit Guard(std::int64_t seconds_budget, Clock::time_point started_at = Clock::now())
            : m_budget_seconds(validate_budget(seconds_budget)),
              m_started_at(started_at),
              m_deadline(make_deadline(started_at, m_budget_seconds))
        {
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        Guard(Guard&&) = delete;
        Guard& operator=(Guard&&) = delete;

        void check_phase(std::string_view phase)
        {
            std::lock_guard lock(m_mutex);
            check_locked(phase, Clock::now());
        }

        void check_phase(std::string_view phase, Clock::time_point now)
        {
            std::lock_guard lock(m_mutex);
            check_locked(phase, now);
        }

        // Releasing the bootstrap gate does not establish native readiness.
        void mark_bootstrap_complete()
        {
            std::lock_guard lock(m_mutex);
            check_locked("bootstrap completion", Clock::now());
            m_bootstrap_state = BootstrapState::released;
        }

        void mark_bootstrap_complete(Clock::time_point now)
        {
            std::lock_guard lock(m_mutex);
            check_locked("bootstrap completion", now);
            m_bootstrap_state = BootstrapState::released;
        }

        void mark_native_ready()
        {
            std::lock_guard lock(m_mutex);
            check_locked("native readiness", Clock::now());
            m_native_state = State::ready;
        }

        void mark_native_ready(Clock::time_point now)
        {
            std::lock_guard lock(m_mutex);
            check_locked("native readiness", now);
            m_native_state = State::ready;
        }

        // Explicit failure is terminal, including a failure racing readiness.
        // Bootstrap pollers receive failure rather than waiting for success.
        void fail()
        {
            std::lock_guard lock(m_mutex);
            fail_locked(FailureReason::explicit_failure);
        }

        [[nodiscard]] Snapshot snapshot() const
        {
            std::lock_guard lock(m_mutex);
            return {m_native_state, m_bootstrap_state, m_failure_reason,
                    m_budget_seconds, m_started_at, m_deadline};
        }

        [[nodiscard]] BootstrapState bootstrap_result() const
        {
            // Passive status reads do not advance time or grant readiness.
            // Pollers must call check_phase() to publish deadline expiration.
            std::lock_guard lock(m_mutex);
            return m_bootstrap_state;
        }

      private:
        static std::int64_t validate_budget(std::int64_t seconds_budget)
        {
            if (seconds_budget <= 0 || seconds_budget > 600)
            {
                throw std::invalid_argument("Startup budget must be between 1 and 600 seconds");
            }
            return seconds_budget;
        }

        static Clock::time_point make_deadline(Clock::time_point started_at, std::int64_t seconds_budget)
        {
            const auto duration = std::chrono::duration_cast<Clock::duration>(std::chrono::seconds(seconds_budget));
            if (started_at > Clock::time_point::max() - duration)
            {
                throw std::out_of_range("Startup deadline cannot be represented");
            }
            return started_at + duration;
        }

        void fail_locked(FailureReason reason)
        {
            if (m_native_state != State::failed)
            {
                m_native_state = State::failed;
                m_failure_reason = reason;
            }
            m_bootstrap_state = BootstrapState::failed;
        }

        void check_locked(std::string_view phase, Clock::time_point now)
        {
            if (m_native_state == State::ready)
            {
                // Startup deadlines do not expire a successful event loop.
                return;
            }
            if (m_native_state != State::failed && now >= m_deadline)
            {
                fail_locked(FailureReason::expired);
            }
            if (m_native_state == State::failed)
            {
                const char* prefix = m_failure_reason == FailureReason::expired
                        ? "Startup deadline expired during " : "Startup failed during ";
                throw std::runtime_error(std::string(prefix) + std::string(phase));
            }
        }

        const std::int64_t m_budget_seconds;
        const Clock::time_point m_started_at;
        const Clock::time_point m_deadline;
        mutable std::mutex m_mutex;
        State m_native_state{State::pending};
        BootstrapState m_bootstrap_state{BootstrapState::pending};
        FailureReason m_failure_reason{FailureReason::none};
    };
}
