#pragma once

#include <Geode/Geode.hpp>  // TODO: Remove
#include <argon/argon.hpp>
#include <Geode/Result.hpp>
#include <arc/future/Future.hpp>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string_view>
#include "RLConfig.hpp"
#include "RLConstants.hpp"
#include "utils/TimedCacheEntry.hpp"

using namespace geode::prelude;

namespace rl {
using JSONInitListType = std::initializer_list<std::pair<std::string, matjson::Value>>;

inline std::string getAPIEndpoint(std::string_view dest) {
    return fmt::format("{}/{}", rl::BASE_API_URL, dest);
}

inline web::WebRequest createWebRequest(JSONInitListType entries) {
    web::WebRequest req;
    req.bodyJSON(matjson::makeObject(entries));
    return req;
}
inline web::WebRequest createWebRequest(matjson::Value const& json) {
    web::WebRequest req;
    req.bodyJSON(json);
    return req;
}

// Returns the server's response body as the error message when it is
// non-empty, otherwise falls back to the provided fallback string.
inline std::string getResponseFailMessage(web::WebResponse const& response,
                                          std::string const& fallback) {
    auto message = response.string().unwrapOrDefault();
    if (!message.empty()) return message;
    return fallback;
}

struct ExpiredToken {
    enum Kind {
        NONE = 0,
        ARGON = 1,
        SESSION = 2,
        REFRESH = 2,
    };
};

// Checks if the response indicates a value is expired.
inline ExpiredToken::Kind getExpiredTokenKind(web::WebResponse const& response) {
    if (response.code() != 401) return ExpiredToken::NONE;
    if (auto header = response.header("Www-Authenticate")) {
        std::string_view auth = *header;
        if (auth == "Argon")
            return ExpiredToken::ARGON;
        else if (auth == "Session")
            return ExpiredToken::SESSION;
        else if (auth == "Refresh")
            return ExpiredToken::REFRESH;
    }
    return ExpiredToken::NONE;
}

std::string_view getBaseURL();

inline bool isGDPS() { return getBaseURL() != "https://www.boomlings.com/database"; }

////////////////////////////////////////////////////////////////////////////////
// Caches

// TODO: Tbh this whole setup is clunky, refactor it all

using RequestTimestamp = RLTimestamp;
using RequestCacheEntry = TimedCacheEntry<matjson::Value>;

inline std::filesystem::path getRLSaveDir() {
    return dirs::getModsSaveDir() / Mod::get()->getID();
}

inline std::filesystem::path getRequestCachePath() {
    return rl::getRLSaveDir() / "request_cache.json";
}

inline std::filesystem::path getNameplateCacheDir() {
    return rl::getRLSaveDir() / "nameplates";
}

inline std::filesystem::path getIconsCacheDir() {
    return rl::getRLSaveDir() / "icons";
}

/// Defined in `utils/LazyNameplate.cpp`.
std::filesystem::path getNameplateCachePath(int nameplateId);
/// Defined in `utils/LazyNameplate.cpp`.
std::filesystem::path getIconsCachePath(std::string const& url);

int64_t getRequestCacheLifetimeSeconds();
inline Expires getRequestCacheExpires() {
    return rl::make_expires(getRequestCacheLifetimeSeconds());
}
inline asp::Duration getRequestCacheExpiresDuration() {
    return asp::Duration::fromSecs(getRequestCacheLifetimeSeconds());
}
int getRequestCacheMaxItems();

inline RequestCacheEntry makeRequestCacheEntry(matjson::Value const& val) {
    return RequestCacheEntry(getRequestCacheExpires(), val);
}
inline RequestCacheEntry makeRequestCacheEntry(matjson::Value&& val) {
    return RequestCacheEntry(getRequestCacheExpires(), std::move(val));
}

RL_ALWAYS_INLINE bool isRequestCacheValid(RequestTimestamp timestamp, int64_t lifetime) {
    return (rl::getCurrentTimestamp() - timestamp) < lifetime;
}
bool isRequestCacheValid(RequestTimestamp timestamp);

/// Checks if either cache is in use, and the cache path is valid.
bool requestCacheExists();
matjson::Value* loadRequestCacheRootLockfree();
[[deprecated]] matjson::Value loadRequestCacheRoot();
[[nodiscard]] bool saveRequestCacheRoot();
/// Bypasses the cached data and stores to the cache file directly.
[[deprecated]] bool saveRequestCacheRoot(matjson::Value const& root);

bool saveNameplateCache(int nameplateId, std::string const& data);
bool hasNameplateCache(int nameplateId);

bool saveIconsCache(std::string const& url, std::string const& data);
bool hasIconsCache(std::string const& url);

std::optional<RequestCacheEntry> loadRequestCacheEntry(std::string_view section, int id);
void storeRequestCacheEntry(std::string_view section, int id, matjson::Value const& data);
void removeRequestCacheEntry(std::string_view section, int id);

void clearNameplateCache();
void clearIconsCache();
void clearRequestCache();

std::optional<matjson::Value> getCachedCommentRole(int accountId);
std::optional<matjson::Value> getStaleCommentRole(int accountId);
void setCachedCommentRole(int accountId, matjson::Value const& data);

std::optional<matjson::Value> getCachedLevelRating(int levelId);
std::optional<matjson::Value> getStaleLevelRating(int levelId);
void setCachedLevelRating(int levelId, matjson::Value const& data);
void removeCachedLevelRating(int levelId);
}  // namespace rl
