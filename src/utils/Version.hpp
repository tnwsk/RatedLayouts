#pragma once

#include <string_view>
#include <fmt/format.h>

namespace rl {

inline constexpr int RL_VERSION_MAJOR = 1;
inline constexpr int RL_VERSION_MINOR = 0;
inline constexpr int RL_VERSION_PATCH = 17;
inline constexpr std::string_view RL_VERSION_STRING = "1.0.17";

inline std::string_view getLastVersionString() {
    static const std::string last = [] {
        if (RL_VERSION_PATCH > 1)
            return fmt::format("{}.{}.{}", RL_VERSION_MAJOR, RL_VERSION_MINOR, RL_VERSION_PATCH - 1);
        // TODO: Do some other kinda lookup here...
        return "???";
    }();
    return last;
}

} // namespace rl
