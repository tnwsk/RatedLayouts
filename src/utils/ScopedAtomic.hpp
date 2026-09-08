#pragma once

#include <atomic>
#include <concepts>
#include <RLConfig.hpp>

namespace rl {

template <std::integral T = int>
struct ScopedAtomicCounter {
    std::atomic<T>* m_instance = nullptr;

    template <bool SetNull = true>
    RL_ALWAYS_INLINE void reset() {
        if (m_instance) {
            m_instance->fetch_sub(1, std::memory_order_acquire);
            if constexpr (SetNull) m_instance = nullptr;
        }
    }

public:
    ScopedAtomicCounter(std::atomic<T>& v) : m_instance(&v) {
        m_instance->fetch_add(1, std::memory_order_release);
    }
    ~ScopedAtomicCounter() { reset<false>(); }

    RL_ALWAYS_INLINE ScopedAtomicCounter(ScopedAtomicCounter&& that) : m_instance(that.m_instance) {
        that.m_instance = nullptr;
    }
    RL_ALWAYS_INLINE ScopedAtomicCounter& operator=(ScopedAtomicCounter&& that) {
        if (m_instance) {
            if (m_instance == that.m_instance) {
                that.reset();
            } else {
                reset<false>();
                m_instance = that.m_instance;
                that.m_instance = nullptr;
            }
        } else {
            m_instance = that.m_instance;
            that.reset();
        }
        return *this;
    }

    ScopedAtomicCounter(ScopedAtomicCounter const&) = delete;
    ScopedAtomicCounter& operator=(ScopedAtomicCounter const&) = delete;
};

template <std::integral T = int>
struct ScopedAtomicGuard {
    std::atomic<T>& m_instance;

public:
    ScopedAtomicGuard(std::atomic<T>& v) : m_instance(v) {
        m_instance.fetch_add(1, std::memory_order_release);
    }
    ~ScopedAtomicGuard() { m_instance.fetch_sub(1, std::memory_order_acquire); }

    ScopedAtomicGuard(ScopedAtomicGuard&&) = delete;
    ScopedAtomicGuard& operator=(ScopedAtomicGuard&&) = delete;
    ScopedAtomicGuard(ScopedAtomicGuard const&) = delete;
    ScopedAtomicGuard& operator=(ScopedAtomicGuard const&) = delete;
};

template <typename T>
ScopedAtomicCounter(std::atomic<T>&) -> ScopedAtomicCounter<T>;

template <typename T>
ScopedAtomicGuard(std::atomic<T>&) -> ScopedAtomicGuard<T>;

}  // namespace rl
