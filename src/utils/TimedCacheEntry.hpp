#pragma once

#include <compare>
#include <concepts>
#include <cstdint>
#include <ctime>
#include <limits>
#include <type_traits>
#include <Geode/Result.hpp>
#include <Geode/loader/Log.hpp>
#include <matjson.hpp>
#include <asp/data/nums.hpp>
#include <asp/time/SystemTime.hpp>
#include "RLConfig.hpp"

namespace rl {

/// UNIX timestamp in microseconds.
using RLTimestamp = int64_t;

inline RLTimestamp getCurrentTimestamp() { return asp::SystemTime::now().timeSinceEpoch().micros(); }
inline RLTimestamp getNeverTimestamp() { return std::numeric_limits<RLTimestamp>::max(); }

/// A time in seconds.
enum class Expires : int32_t {
    DEFAULT = 360,
    NEVER = -1,
    ANY = -2,
};

inline constexpr int64_t make_expires_raw(int64_t offset) { return offset >= 0 ? offset : -1; }
inline constexpr Expires make_expires(int64_t offset) {
    return offset >= 0 ? static_cast<Expires>(offset) : Expires::NEVER;
}

inline constexpr RLTimestamp expires_to_timestamp(int64_t seconds) {
    if (seconds < 0) return -1;
    return asp::Duration::fromSecs(seconds).micros();
}
inline constexpr RLTimestamp expires_to_timestamp(Expires ex) {
    if (ex == Expires::NEVER)
        return getNeverTimestamp();
    else if (ex == Expires::ANY)
        return expires_to_timestamp(300);
    else
        return expires_to_timestamp(int32_t(ex));
}

inline constexpr asp::Duration expires_to_duration(Expires ex) {
    if (ex == Expires::NEVER) {
        return asp::Duration::fromMicros(getNeverTimestamp());
    }
    return asp::Duration::fromSecs((ex != Expires::ANY) ? int32_t(ex) : 300);
}

inline namespace literals {
inline constexpr Expires operator""_expires(unsigned long long offset) { return make_expires(offset); }
inline constexpr asp::Duration operator""_micros(unsigned long long d) { return asp::Duration::fromMicros(d); }
inline constexpr asp::Duration operator""_millis(unsigned long long d) { return asp::Duration::fromMillis(d); }
inline constexpr asp::Duration operator""_secs(unsigned long long d) { return asp::Duration::fromSecs(d); }
inline constexpr asp::Duration operator""_mins(unsigned long long d) { return asp::Duration::fromMinutes(d); }
inline constexpr asp::Duration operator""_hours(unsigned long long d) { return asp::Duration::fromHours(d); }
inline constexpr asp::Duration operator""_days(unsigned long long d) { return asp::Duration::fromDays(d); }
}  // namespace literals

inline constexpr RLTimestamp make_raw_expiring_timestamp(RLTimestamp timestamp, RLTimestamp offset) {
    if (timestamp < 0 || offset < 0) {
        return getNeverTimestamp();
    }
    RLTimestamp result;
    if (!asp::checkedAdd(result, timestamp, offset)) {
        return getNeverTimestamp();
    }
    return result;
}
inline constexpr RLTimestamp make_raw_expiring_timestamp(RLTimestamp timestamp, Expires expires) {
    return make_raw_expiring_timestamp(timestamp, expires_to_timestamp(expires));
}
inline constexpr RLTimestamp make_raw_expiring_timestamp(RLTimestamp timestamp, asp::Duration expires) {
    return make_raw_expiring_timestamp(timestamp, expires.micros());
}

////////////////////////////////////////////////////////////////////////////////
// Timestamps

/// Defines a timestamp with a time it will expire.
/// By default expiry times are measured in seconds.
class ExpiringTimestamp {
protected:
    struct raw_tag {};
    RLTimestamp m_expiresAt = 0;

    ExpiringTimestamp(RLTimestamp ts, raw_tag) : m_expiresAt(ts) {}

public:
    ExpiringTimestamp(Expires ex = Expires::DEFAULT) : ExpiringTimestamp(getCurrentTimestamp(), ex) {}
    ExpiringTimestamp(RLTimestamp ts, Expires ex) : m_expiresAt(make_raw_expiring_timestamp(ts, ex)) {}
    ExpiringTimestamp(asp::Duration ex) : ExpiringTimestamp(getCurrentTimestamp(), ex) {}
    ExpiringTimestamp(RLTimestamp ts, asp::Duration ex) : m_expiresAt(make_raw_expiring_timestamp(ts, ex)) {}

    static ExpiringTimestamp FromRaw(RLTimestamp timestamp) {
        return ExpiringTimestamp(timestamp, raw_tag{});
    }

    /// Sets the expiry date to the start of the UNIX epoch.
    void invalidate() { this->m_expiresAt = 0; }
    /// Sets the timestamp absolutely.
    void set(RLTimestamp expires) { this->m_expiresAt = expires; }

    /// "Bumps" back the expiry time.
    RLTimestamp bump(Expires ex) {
        m_expiresAt = make_raw_expiring_timestamp(m_expiresAt, ex);
        return m_expiresAt;
    }
    RLTimestamp update(RLTimestamp ts, Expires ex) {
        m_expiresAt = make_raw_expiring_timestamp(ts, ex);
        return m_expiresAt;
    }

    bool isStale(RLTimestamp cmpTo) const { return m_expiresAt < cmpTo; }
    RL_ALWAYS_INLINE bool isStale() const { return isStale(getCurrentTimestamp()); }

    RLTimestamp expiresAt() const { return m_expiresAt; }
    ExpiringTimestamp timestamp() const { return *this; }
    asp::Duration duration() const { return asp::Duration::fromMicros(m_expiresAt); }

    ExpiringTimestamp& operator+=(Expires ex) {
        this->bump(ex);
        return *this;
    }

    std::strong_ordering operator<=>(const ExpiringTimestamp&) const = default;
};

inline constexpr ExpiringTimestamp operator+(RLTimestamp timestamp, Expires expires) {
    return ExpiringTimestamp(timestamp, expires);
}
inline constexpr ExpiringTimestamp operator+(Expires expires, RLTimestamp timestamp) {
    return ExpiringTimestamp(timestamp, expires);
}

/// A wrapper type with an expiring time.
template <class T>
class TimedCacheEntry : public ExpiringTimestamp {
protected:
    T m_value;

public:
    TimedCacheEntry() : ExpiringTimestamp(Expires::DEFAULT), m_value() {}
    //TimedCacheEntry(auto&&... args) : ExpiringTimestamp(Expires::DEFAULT), m_value(RL_FWD(args)...) {}
    TimedCacheEntry(Expires ex, auto&&... args) : ExpiringTimestamp(ex), m_value(RL_FWD(args)...) {}
    TimedCacheEntry(asp::Duration ex, auto&&... args) : ExpiringTimestamp(ex), m_value(RL_FWD(args)...) {}
    TimedCacheEntry(ExpiringTimestamp ex, auto&&... args) : ExpiringTimestamp(ex), m_value(RL_FWD(args)...) {}

    TimedCacheEntry(TimedCacheEntry const& other) :
        ExpiringTimestamp(other.m_expiresAt, raw_tag{}), m_value(other.m_value) {}
    TimedCacheEntry(TimedCacheEntry&& other) :
        ExpiringTimestamp(other.m_expiresAt, raw_tag{}), m_value(std::move(other.m_value)) {}
    
    TimedCacheEntry& operator=(TimedCacheEntry const& other) {
        m_expiresAt = other.m_expiresAt;
        m_value = other.m_value;
        return *this;
    }
    TimedCacheEntry& operator=(TimedCacheEntry&& other) {
        m_expiresAt = other.m_expiresAt;
        m_value = std::move(other.m_value);
        return *this;
    }

    auto&& value(this auto&& self) { return RL_FWD(self).m_value; }
    auto&& operator*(this auto&& self) { return RL_FWD(self).m_value; }
    auto operator->(this auto&& self) { return &RL_FWD(self).m_value; }
};

extern template class TimedCacheEntry<matjson::Value>;

namespace timestamp_detail {
template <typename T>
T& test(TimedCacheEntry<T> const&);
}  // namespace timestamp_detail

template <typename T>
concept is_timed_cache_entry = requires(T const& val) { timestamp_detail::test(val); };

template <std::size_t Index, class T>
decltype(auto) get(TimedCacheEntry<T>& value) {
    if constexpr (Index == 0) {
        return value.timestamp();
    } else if constexpr (Index == 1) {
        return value.value();
    }
}

template <std::size_t Index, class T>
decltype(auto) get(TimedCacheEntry<T> const& value) {
    if constexpr (Index == 0) {
        return value.timestamp();
    } else if constexpr (Index == 1) {
        return value.value();
    }
}

template <std::size_t Index, class T>
decltype(auto) get(TimedCacheEntry<T>&& value) {
    if constexpr (Index == 0) {
        return value.timestamp();
    } else if constexpr (Index == 1) {
        return std::move(value).value();
    }
}

}  // namespace rl

// allow destructuring
template <class T>
struct std::tuple_size<rl::TimedCacheEntry<T>> : std::integral_constant<std::size_t, 2> {};

template <class T>
struct std::tuple_element<0, rl::TimedCacheEntry<T>> {
    using type = rl::ExpiringTimestamp;
};

template <class T>
struct std::tuple_element<1, rl::TimedCacheEntry<T>> {
    using type = T;
};

// serde
template <>
struct matjson::Serialize<rl::ExpiringTimestamp> {
    static geode::Result<rl::ExpiringTimestamp> fromJson(const matjson::Value& value) {
        GEODE_UNWRAP_INTO(rl::RLTimestamp expiresAt, value.asInt());
        return geode::Ok(rl::ExpiringTimestamp::FromRaw(expiresAt));
    }
    static matjson::Value toJson(const rl::ExpiringTimestamp& entry) {
        return matjson::Value(static_cast<std::intmax_t>(entry.expiresAt()));
    }
};

template <class T>
struct matjson::Serialize<rl::TimedCacheEntry<T>> {
    static constexpr bool kIsJSONValue = std::same_as<T, matjson::Value>;
    using Entry = rl::TimedCacheEntry<T>;
    static geode::Result<Entry> fromJson(const matjson::Value& value) {
        static_assert(kIsJSONValue || matjson::CanDeserialize<T>);
        GEODE_UNWRAP_INTO(T data, value.get("data"));
        GEODE_UNWRAP_INTO(auto time, value["timestamp"].as<rl::ExpiringTimestamp>());
        return geode::Ok(Entry(time, std::move(data)));
    }
    static matjson::Value toJson(const Entry& entry) {
        static_assert(kIsJSONValue || matjson::CanSerialize<T>);
        return matjson::makeObject({{"data", entry.value()}, {"timestamp", entry.timestamp()}});
    }
};
