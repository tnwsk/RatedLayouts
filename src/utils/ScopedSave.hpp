#pragma once

namespace rl {

template <class T>
class ScopedSave {
    T& value;
    T old;

public:
    constexpr ScopedSave(T& value_) : value(value_), old(value_) {}
    constexpr ScopedSave(T& value_, T const& x) : ScopedSave(value_) { value_ = x; }
    constexpr ~ScopedSave() { this->value = std::move(old); }
};

template <class T>
ScopedSave(T&) -> ScopedSave<T>;

template <class T>
ScopedSave(T&, auto&&) -> ScopedSave<T>;

}  // namespace rl
