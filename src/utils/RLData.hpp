#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <Geode/Result.hpp>
#include <Geode/utils/function.hpp>
#include <arc/future/Future.hpp>
#include <asp/collections/SmallVec.hpp>
#include <matjson.hpp>
#include "RLNetworkUtils.hpp"
#include "RLConstants.hpp"
#include "utils/JSONHelpers.hpp"  // PositiveInt
#include "utils/TimedCacheEntry.hpp"

// FIXME: Move nameplate somewhere better...

namespace rl {
using RLUserId = int64_t;

/// A structure just holding user stats.
struct RLUserStats {
    int points = 0;
    int planets = 0;
    int stars = 0;
    int coins = 0;
    int votes = 0;

public:
    const RLUserStats& userStats() const { return *this; }
    RLUserStats& init(const RLUserStats& other) { return (*this = other); }
    RLUserStats& init(RLUserStats&& other) { return (*this = other); }
};

/// A structure just holding user roles.
struct RLUserRoles {
    // Special flags
    bool isSupporter = false;
    bool isBooster = false;
    // New role flags returned from server
    bool isClassicMod = false;
    bool isClassicAdmin = false;
    bool isLeaderboardMod = false;
    bool isLeaderboardAdmin = false;
    bool isPlatMod = false;
    bool isPlatAdmin = false;
    bool isDeveloper = false;
    bool isOwner = false;

public:
    const RLUserRoles& userRoles() const { return *this; }
    RLUserRoles& init(const RLUserRoles& other) { return (*this = other); }
    RLUserRoles& init(RLUserRoles&& other) { return (*this = other); }
};

/// Common interface for player info.
/// Used in `ProfilePage` and `CommentCell`.
struct RLUserInfo : public RLUserStats, public RLUserRoles {
    RLUserId accountId = 0;
    int nameplate = 0;

public:
    using Future = arc::Future<RLUserInfo>;
    using ResFuture = arc::Future<geode::Result<RLUserInfo>>;

    /// Gets an `RLUserInfo` instance.
    static ResFuture get(RLUserId id, bool useCache = true);

    constexpr RLUserInfo() = default;
    const RLUserInfo& userInfo() const { return *this; }

    RLUserInfo& clear() { return (*this = RLUserInfo{}); }
    RLUserInfo& init(const RLUserInfo& other) { return (*this = other); }
    RLUserInfo& init(RLUserInfo&& other) { return (*this = std::move(other)); }
    using RLUserRoles::init;
    using RLUserStats::init;

    bool isEmptyUser() const {
        return stars == 0 && planets == 0 && !isSupporter && !isBooster && !isClassicMod &&
               !isClassicAdmin && !isLeaderboardMod && !isLeaderboardAdmin && !isPlatMod &&
               !isPlatAdmin && !isDeveloper && !isOwner;
    }

    bool isPseudoOwner() const { return isOwner || isDeveloper; }
    bool hasSupporterFeatures() const { return isSupporter || isBooster; }
};

struct LocalEndpoint {
    using ResFuture = arc::Future<geode::Result<matjson::Value>>;
    using Verifier = geode::Function<geode::Result<>(matjson::Value const&)>;
    using Params = std::span<const std::string>;

    struct Builder {
        std::string m_name;
        std::optional<asp::Duration> m_expires;
        std::optional<matjson::Value> m_body;
        Verifier m_verify = nullptr;
        asp::SmallVec<std::string, 2> m_params;
        asp::SmallVec<std::string, 1> m_nkparams;
        bool m_nostale = false;
        bool m_progress = false;

    public:
        Builder() = default;
        explicit Builder(std::string name_) : m_name(std::move(name_)) {}

        Builder& name(std::string name_) {
            m_name = std::move(name_);
            return *this;
        }
        Builder& expires(Expires expires_) {
            m_expires = expires_to_duration(expires_);
            return *this;
        }
        Builder& nostale(bool val = true) {
            m_nostale = val;
            return *this;
        }
        Builder& progress(bool val = true) {
            m_progress = val;
            return *this;
        }
        Builder& expires(asp::Duration expires_) {
            m_expires = expires_;
            return *this;
        }
        Builder& ibody(JSONInitListType entries) {
            m_body.emplace(matjson::makeObject(entries));
            return *this;
        }
        Builder& body(matjson::Value const& value) {
            m_body.emplace(value);
            return *this;
        }
        Builder& body(matjson::Value&& value) {
            m_body.emplace(std::move(value));
            return *this;
        }
        Builder& verify(Verifier verify_) {
            m_verify = std::move(verify_);
            return *this;
        }

        Builder& verifySuccess();
        Builder& verifySuccess(std::string_view id);

        Builder& param(std::string_view nameAndData) {
            m_params.push_back(std::string(nameAndData));
            return *this;
        }
        Builder& nkparam(std::string_view nameAndData) {
            m_nkparams.push_back(std::string(nameAndData));
            return *this;
        }

        Builder& param(std::string_view name, std::string_view data);
        Builder& nkparam(std::string_view name, std::string_view data);

        template <std::integral T>
        Builder& param(std::string_view name, T data) {
            return param(name, fmt::to_string(data));
        }
        template <std::integral T>
        Builder& nkparam(std::string_view name, T data) {
            return nkparam(name, fmt::to_string(data));
        }

        Params getParams() const { return Params(m_params.begin(), m_params.end()); }
        Params getNKParams() const { return Params(m_nkparams.begin(), m_nkparams.end()); }

        template <class Self>
        Self&& take(this Self& self) {
            return std::move(self);
        }
        template <class Self>
        Self&& take(this Self const&) = delete;

        ResFuture get();
        ResFuture getWithAuth();
    };

    /// Gets from the endpoint, or loads from cache if possible.
    static ResFuture get(std::string name);
    static ResFuture get(std::string name, matjson::Value body);

    /// Gets from the authenticated endpoint, or loads from cache if possible.
    static ResFuture getWithAuth(std::string name);
    static ResFuture getWithAuth(std::string name, matjson::Value body);

    static inline Builder build() { return Builder(); }
    static inline Builder build(std::string name) { return Builder(std::move(name)); }
};

bool hasRLDataCache();
void clearRLDataCache();

inline std::filesystem::path getDataCachePath() {
    return dirs::getModsSaveDir() / Mod::get()->getID() / "data_cache.json";
}
inline std::filesystem::path getDataCacheZipPath() {
    return dirs::getModsSaveDir() / Mod::get()->getID() / "data_cache.zip";
}

inline bool doesUserHaveBadge(RLBadgeKind K, RLUserRoles const& info) {
    using enum RLBadgeKind;
    switch (K) {
    case Supporter: return info.isSupporter;
    case Booster: return info.isBooster;
    case ClassicAdmin: return info.isClassicAdmin;
    case ClassicMod: return info.isClassicMod;
    case PlatAdmin: return info.isPlatAdmin;
    case PlatMod: return info.isPlatMod;
    case LeaderboardMod: return info.isLeaderboardMod;
    case Owner: return info.isOwner;
    case LeaderboardAdmin: return info.isLeaderboardAdmin;
    case Developer: return info.isDeveloper;
    default: return false;
    }
}

}  // namespace rl

template <>
struct matjson::Serialize<rl::RLUserStats> {
    static geode::Result<rl::RLUserStats> fromJson(const matjson::Value& value) {
        auto unwrapPos = [&value](std::string_view key) -> int {
            return value[key].as<rl::PositiveInt>().unwrapOrDefault();
        };
        rl::RLUserStats stats;
        stats.points = unwrapPos("points");
        stats.stars = unwrapPos("stars");
        stats.planets = unwrapPos("planets");
        //stats.nameplate = unwrapPos("nameplate");
        stats.coins = unwrapPos("coins");
        stats.votes = unwrapPos("votes");
        return geode::Ok(stats);
    }
    static matjson::Value toJson(const rl::RLUserStats& stats) {
        return matjson::makeObject({{"points", stats.points},
                                    {"stars", stats.stars},
                                    {"planets", stats.planets},
                                    //{"nameplate", stats.nameplate},
                                    {"coins", stats.coins},
                                    {"votes", stats.votes}});
    }
};

template <>
struct matjson::Serialize<rl::RLUserRoles> {
    static geode::Result<rl::RLUserRoles> fromJson(const matjson::Value& value) {
        rl::RLUserRoles roles;
        roles.isSupporter = value["isSupporter"].asBool().unwrapOrDefault();
        roles.isBooster = value["isBooster"].asBool().unwrapOrDefault();
        roles.isClassicMod = value["isClassicMod"].asBool().unwrapOrDefault();
        roles.isClassicAdmin = value["isClassicAdmin"].asBool().unwrapOrDefault();
        roles.isLeaderboardMod = value["isLeaderboardMod"].asBool().unwrapOrDefault();
        roles.isLeaderboardAdmin = value["isLeaderboardAdmin"].asBool().unwrapOrDefault();
        roles.isPlatMod = value["isPlatMod"].asBool().unwrapOrDefault();
        roles.isPlatAdmin = value["isPlatAdmin"].asBool().unwrapOrDefault();
        roles.isDeveloper = value["isDeveloper"].asBool().unwrapOrDefault();
        roles.isOwner = value["isOwner"].asBool().unwrapOrDefault();
        return geode::Ok(roles);
    }
    static matjson::Value toJson(const rl::RLUserRoles& roles) {
        return matjson::makeObject({{"isSupporter", roles.isSupporter},
                                    {"isBooster", roles.isBooster},
                                    {"isClassicMod", roles.isClassicMod},
                                    {"isClassicAdmin", roles.isClassicAdmin},
                                    {"isLeaderboardMod", roles.isLeaderboardMod},
                                    {"isLeaderboardAdmin", roles.isLeaderboardAdmin},
                                    {"isPlatMod", roles.isPlatMod},
                                    {"isPlatAdmin", roles.isPlatAdmin},
                                    {"isDeveloper", roles.isDeveloper},
                                    {"isOwner", roles.isOwner}});
    }
};

template <>
struct matjson::Serialize<rl::RLUserInfo> {
    static geode::Result<rl::RLUserInfo> fromJson(const matjson::Value& value) {
        auto unwrapPos = [&value](std::string_view key) -> int {
            return value[key].as<rl::PositiveInt>().unwrapOrDefault();
        };
        rl::RLUserInfo info;
        info.points = unwrapPos("points");
        info.stars = unwrapPos("stars");
        info.planets = unwrapPos("planets");
        info.nameplate = unwrapPos("nameplate");
        info.coins = unwrapPos("coins");
        info.votes = unwrapPos("votes");
        info.isSupporter = value["isSupporter"].asBool().unwrapOrDefault();
        info.isBooster = value["isBooster"].asBool().unwrapOrDefault();
        info.isClassicMod = value["isClassicMod"].asBool().unwrapOrDefault();
        info.isClassicAdmin = value["isClassicAdmin"].asBool().unwrapOrDefault();
        info.isLeaderboardMod = value["isLeaderboardMod"].asBool().unwrapOrDefault();
        info.isLeaderboardAdmin = value["isLeaderboardAdmin"].asBool().unwrapOrDefault();
        info.isPlatMod = value["isPlatMod"].asBool().unwrapOrDefault();
        info.isPlatAdmin = value["isPlatAdmin"].asBool().unwrapOrDefault();
        info.isDeveloper = value["isDeveloper"].asBool().unwrapOrDefault();
        info.isOwner = value["isOwner"].asBool().unwrapOrDefault();
        return geode::Ok(info);
    }
    static matjson::Value toJson(const rl::RLUserInfo& info) {
        return matjson::makeObject({{"points", info.points},
                                    {"stars", info.stars},
                                    {"planets", info.planets},
                                    {"nameplate", info.nameplate},
                                    {"coins", info.coins},
                                    {"votes", info.votes},
                                    {"isSupporter", info.isSupporter},
                                    {"isBooster", info.isBooster},
                                    {"isClassicMod", info.isClassicMod},
                                    {"isClassicAdmin", info.isClassicAdmin},
                                    {"isLeaderboardMod", info.isLeaderboardMod},
                                    {"isLeaderboardAdmin", info.isLeaderboardAdmin},
                                    {"isPlatMod", info.isPlatMod},
                                    {"isPlatAdmin", info.isPlatAdmin},
                                    {"isDeveloper", info.isDeveloper},
                                    {"isOwner", info.isOwner}});
    }
};
