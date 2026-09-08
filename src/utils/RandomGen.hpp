#pragma once

#include <span>
#include <Geode/utils/random.hpp>

namespace rl {

geode::utils::random::Generator makeRNG();
geode::utils::random::Generator* globalRNG();

template <typename T>
inline T selectRandom(std::span<T> data) {
    return data[rl::globalRNG()->generate<size_t>(0, data.size())];
}

template <typename T>
inline T selectRandom(T val, auto...vals) {
    T data[sizeof...(vals) + 1] { val, vals... };
    return selectRandom<T>(data);
}

}  // namespace rl
