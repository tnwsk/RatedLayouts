#pragma once

#include <Geode/utils/function.hpp>
#include "RLConfig.hpp"

#define RL_DEFER(...) ::rl::ScopeExit GEODE_CONCAT(_scope, __LINE__) \
    = [& __VA_OPT__(,) __VA_ARGS__] () mutable -> void
// Defines something that runs at the end of the current scope.
#define $defer RL_DEFER()

namespace rl {

class ScopeExit {
    geode::Function<void()> m_data;

public:
    RL_ALWAYS_INLINE ScopeExit(geode::Function<void()> data) : m_data(std::move(data)) {}
    ~ScopeExit() {
        if (m_data) m_data();
    }
};

}  // namespace rl
