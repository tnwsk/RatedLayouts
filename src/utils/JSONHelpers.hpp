#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
//#include <string>
#include <string_view>
#include <Geode/Result.hpp>
#include <matjson.hpp>

namespace rl {

class PositiveInt {
    intmax_t data = 0;

    constexpr PositiveInt(intmax_t value, bool) : data(value) {
        // TODO: assert(value >= 0)?
    }

public:
    constexpr PositiveInt() = default;
    constexpr PositiveInt(PositiveInt const&) = default;
    constexpr PositiveInt(PositiveInt&&) = default;
    constexpr PositiveInt& operator=(PositiveInt const&) = default;
    constexpr PositiveInt& operator=(PositiveInt&&) = default;

    constexpr PositiveInt(intmax_t value) : data(value > 0 ? value : 0) {}
    explicit PositiveInt(uintmax_t value) : PositiveInt(static_cast<intmax_t>(value)) {}

    static geode::Result<PositiveInt> create(intmax_t value) {
        if (value >= 0) [[likely]]
            return geode::Ok(PositiveInt(value, true));
        else [[unlikely]]
            return geode::Err("Value is not positive!");
    }

    constexpr PositiveInt& operator=(intmax_t value) {
        this->data = (value > 0 ? value : 0);
        return *this;
    }

    constexpr intmax_t get() const {
        return this->data;
    }

    template <std::signed_integral Int>
    constexpr operator Int() const {
        constexpr Int intMax = std::numeric_limits<Int>::max();
        if (data <= static_cast<intmax_t>(intMax))
            return static_cast<Int>(data);
        return intMax;
    }
};

}  // namespace rl

template <>
struct matjson::Serialize<rl::PositiveInt> {
    static geode::Result<rl::PositiveInt> fromJson(const matjson::Value& value) {
        GEODE_UNWRAP_INTO(const intmax_t i, value.asInt());
        return rl::PositiveInt::create(i);
    }
    static matjson::Value toJson(rl::PositiveInt i) {
        return matjson::Value(i.get());
    }
};
