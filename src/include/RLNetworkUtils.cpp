#include "RLNetworkUtils.hpp"
#include <Geode/Geode.hpp>
//#include <argon/argon.hpp>
#include <mutex>
#include <optional>
#include <unordered_map>
#include "utils/CachedSettings.hpp"
#include "utils/NoHashHasher.hpp"

#define TRY_LOCK(MUTEX, TIMEOUT, FAIL...)             \
    std::lock_guard lock_(*({                         \
        using namespace std::chrono_literals;         \
        auto& mtx_ = MUTEX;                           \
        if (!mtx_.try_lock_for(TIMEOUT)) return FAIL; \
        &mtx_;                                        \
    }),                                               \
                          std::adopt_lock)

using namespace geode::prelude;
using namespace rl;

// TODO: Use sparse mapping?
using IdHasher = NoHashHasher<int>;
using RequestCacheType = std::unordered_map<int, RequestCacheEntry, IdHasher>;

namespace {
enum { kRequestCacheTimeout = 30 };
}  // namespace

static std::recursive_timed_mutex RequestCacheMutex;
static std::optional<matjson::Value> RequestCache;
static constinit RequestTimestamp LastTimeRequestCacheSavedToFile = -1;
static RequestCacheType LevelRatingCache;

/// Avoid flooding logs with these messages.
// TODO: Make an $execute style helper for things like this?
static RL_ALWAYS_INLINE void logBaseURLOnce(std::string_view ret) {
    static bool notPrinted = true;
    if (notPrinted) [[unlikely]] {
        log::debug("Base URL from game executable: '{}'", ret);
        notPrinted = false;
    }
}

// every gd mod has a little bit of betterinfo in it :) - cvolton
std::string_view rl::getBaseURL() {
    // if(Loader::get()->isModLoaded("km7dev.server_api")) {
    //     auto url = ServerAPIEvents::getCurrentServer().url;
    //     if(!url.empty() && url != "NONE_REGISTERED") {
    //         while(url.ends_with("/")) url.pop_back();
    //         return url;
    //     }
    // }

    static_assert(GEODE_COMP_GD_VERSION == 22081, "Unsupported GD version");
#if defined(GEODE_IS_WINDOWS)
    static constexpr std::uintptr_t urlBaseOffset = 0x558b70;
#elif defined(GEODE_IS_ARM_MAC)
    static constexpr std::uintptr_t urlBaseOffset = 0x77d709;
#elif defined(GEODE_IS_INTEL_MAC)
    static constexpr std::uintptr_t urlBaseOffset = 0x868df0;
#elif defined(GEODE_IS_ANDROID64)
    static constexpr std::uintptr_t urlBaseOffset = 0xECCF90;
#elif defined(GEODE_IS_ANDROID32)
    static constexpr std::uintptr_t urlBaseOffset = 0x96C0DB;
#elif defined(GEODE_IS_IOS)
    static constexpr std::uintptr_t urlBaseOffset = 0x6b8cc2;
#else
    static_assert(false, "Unsupported platform");
#endif

    // The addresses are pointing to "https://www.boomlings.com/database/getGJLevels21.php"
    // in the main game executable
    std::string_view ret = (const char*)(base::get() + urlBaseOffset);
    if (ret.size() > 34) ret = ret.substr(0, 34);

    logBaseURLOnce(ret);
    return ret;
}

static bool cacheMapNeedsPruning(size_t cacheSize, std::int64_t maxItems) {
    if (maxItems <= 0)
        // TODO: Add a default cache size?
        return false;
    // Allow some extra entries to avoid constantly clearing cache (~1.75x).
    const size_t cacheMaxWithTolerance = size_t((maxItems * 7) / 4);
    return cacheSize > cacheMaxWithTolerance;
}

static void pruneCacheMap(std::string_view name, RequestCacheType& cache) {
    const int maxItems = getRequestCacheMaxItems();
    if (!cacheMapNeedsPruning(cache.size(), maxItems)) return;

    std::vector<std::pair<int, std::time_t>> entries;
    entries.reserve(cache.size());
    for (auto const& [id, entry] : cache) {
        entries.emplace_back(id, entry.expiresAt());
    }
    std::sort(entries.begin(), entries.end(), [](auto const& a, auto const& b) {
        return a.second < b.second;
    });

    const size_t removeCount = cache.size() - size_t(maxItems);
    for (size_t i = 0; i < removeCount; ++i) {
        cache.erase(entries[i].first);
    }
    log::debug("Pruned in-memory cache '{}'", name);
}

static std::optional<matjson::Value> getCached(std::string_view name,
                                               RequestCacheType& cache,
                                               int id) {
    //std::lock_guard lock(RequestCacheMutex);
    TRY_LOCK(RequestCacheMutex, 1s, std::nullopt);
    auto it = cache.find(id);
    if (it != cache.end() && it->second.isStale()) {
        return *it->second;
    }
    auto entry = rl::loadRequestCacheEntry(name, id);
    if (entry) {
        cache[id] = *entry;
        if (entry->isStale()) return entry->value();
    }
    return std::nullopt;
}

static std::optional<matjson::Value> getStale(std::string_view name,
                                              RequestCacheType& cache,
                                              int id) {
    //std::lock_guard lock(RequestCacheMutex);
    TRY_LOCK(RequestCacheMutex, 1s, std::nullopt);
    auto it = cache.find(id);
    if (it != cache.end()) return *it->second;
    auto entry = rl::loadRequestCacheEntry(name, id);
    if (entry) {
        cache[id] = *entry;
        return entry->value();
    }
    return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////
// Request Cache

int64_t rl::getRequestCacheLifetimeSeconds() {
    if (!Mod::get()) return 360;
    return CachedSettings::get()->requestCacheLifetime;
}

int rl::getRequestCacheMaxItems() {
    if (!Mod::get()) return 128;
    return CachedSettings::get()->requestCacheMaxItems;
}

bool rl::isRequestCacheValid(RequestTimestamp timestamp) {
    std::int64_t lifetime = getRequestCacheLifetimeSeconds();
    if (lifetime <= 0) return false;
    return isRequestCacheValid(timestamp, asp::Duration::fromSecs(lifetime).micros());
}

static matjson::Value loadRequestCacheRootFromFile() {
    auto path = getRequestCachePath();
    auto existing = utils::file::readString(path);
    if (!existing) return matjson::Value::object();

    auto parsed = matjson::parse(existing.unwrap());
    if (!parsed) return matjson::Value::object();

    matjson::Value root = std::move(parsed).unwrap();
    if (!root.isObject()) return matjson::Value::object();
    return root;
}

static matjson::Value* loadRequestSection(std::string_view section) {
    auto& root = *loadRequestCacheRootLockfree();
    if (!root.isObject()) return nullptr;

    auto& sectionValue = root[section];
    if (!sectionValue.isObject()) return nullptr;

    return &sectionValue;
}

static matjson::Value* loadRequestEntry(std::string_view section, int id) {
    if (auto* sectionPtr = loadRequestSection(section)) {
        auto& sectionValue = *sectionPtr;
        std::string key = geode::utils::numToString(id);
        auto& entry = sectionValue[key];
        if (entry.isObject()) return &entry;
    }
    return nullptr;
}

bool rl::requestCacheExists() {
    std::lock_guard lock(RequestCacheMutex);
    if (!LevelRatingCache.empty() || RequestCache.has_value()) {
        return true;
    }
    return std::filesystem::exists(getRequestCachePath());
}

matjson::Value* rl::loadRequestCacheRootLockfree() {
    if (RequestCache.has_value()) [[likely]] {
        return &*RequestCache;
    } else [[unlikely]] {
        // Create from file data!
        return &RequestCache.emplace(loadRequestCacheRootFromFile());
    }
}

matjson::Value rl::loadRequestCacheRoot() {
    std::lock_guard lock(RequestCacheMutex);
    if (auto* root = rl::loadRequestCacheRootLockfree()) [[likely]] {
        return *root;
    }
    return matjson::Value::object();
}

bool rl::saveRequestCacheRoot() {
    std::lock_guard lock(RequestCacheMutex);
    if (!RequestCache) return true;  // No cache
    auto path = getRequestCachePath();
    std::filesystem::create_directories(path.parent_path());
    auto writeRes = utils::file::writeStringSafe(path, RequestCache->dump(0));
    return writeRes.isOk();
}

bool rl::saveRequestCacheRoot(matjson::Value const& root) {
    auto path = getRequestCachePath();
    std::filesystem::create_directories(path.parent_path());
    auto writeRes = utils::file::writeStringSafe(path, root.dump(0));
    return writeRes.isOk();
}

static bool doesRequestCacheNeedSave() {
    if (LastTimeRequestCacheSavedToFile <= -1) return true;
    const RequestTimestamp diff = getCurrentTimestamp() - LastTimeRequestCacheSavedToFile;
    return diff < kRequestCacheTimeout;
}

static bool saveRequestCacheRootWithTimeout() {
    if (!doesRequestCacheNeedSave()) return true;
    // TODO: Make this async?
    if (rl::saveRequestCacheRoot()) {
        LastTimeRequestCacheSavedToFile = getCurrentTimestamp();
        return true;
    }
    // Save failed.
    return false;
}

std::optional<RequestCacheEntry> rl::loadRequestCacheEntry(std::string_view section, int id) {
    std::lock_guard lock(RequestCacheMutex);
    if (auto* entry = loadRequestEntry(section, id)) {
        auto entryOrErr = entry->as<RequestCacheEntry>();
        if (entryOrErr.isOk()) return std::move(entryOrErr).unwrap();
    }
    return std::nullopt;
}

void rl::storeRequestCacheEntry(std::string_view section, int id, matjson::Value const& data) {
    std::lock_guard lock(RequestCacheMutex);
    if (auto* sectionPtr = loadRequestSection(section)) {
        std::string key = geode::utils::numToString(id);
        sectionPtr->set(key, makeRequestCacheEntry(data));
        saveRequestCacheRootWithTimeout();
    }
}

void rl::removeRequestCacheEntry(std::string_view section, int id) {
    std::lock_guard lock(RequestCacheMutex);
    if (auto* sectionPtr = loadRequestSection(section)) {
        std::string key = geode::utils::numToString(id);
        if (!sectionPtr->erase(key)) return;
        // Save the new value.
        saveRequestCacheRootWithTimeout();
    }
}

void rl::clearRequestCache() {
    std::lock_guard lock(RequestCacheMutex);
    LevelRatingCache.clear();
    if (RequestCache) RequestCache.reset();
    auto requestPath = rl::getRequestCachePath();
    if (std::filesystem::exists(requestPath)) {
        std::error_code ec;
        std::filesystem::remove(requestPath, ec);
        // TODO: Log removal failure with ec?
    }
    rl::clearNameplateCache();
}

////////////////////////////////////////////////////////////////////////////////
// Nameplate Cache

bool rl::saveNameplateCache(int nameplateId, std::string const& data) {
    // TODO: Cache nameplates
    auto path = rl::getNameplateCachePath(nameplateId);
    std::filesystem::create_directories(path.parent_path());
    auto writeRes = utils::file::writeString(utils::string::pathToString(path), data);
    return writeRes.isOk();
}

bool rl::hasNameplateCache(int nameplateId) {
    return std::filesystem::exists(rl::getNameplateCachePath(nameplateId));
}

void rl::clearNameplateCache() {
    auto path = rl::getNameplateCacheDir();
    if (std::filesystem::exists(path)) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Icon Cache

bool rl::saveIconsCache(std::string const& url, std::string const& data) {
    // TODO: Cache icons
    auto path = rl::getIconsCachePath(url);
    std::filesystem::create_directories(path.parent_path());
    auto writeRes = utils::file::writeString(utils::string::pathToString(path), data);
    return writeRes.isOk();
}

bool rl::hasIconsCache(std::string const& url) {
    return std::filesystem::exists(rl::getIconsCachePath(url));
}

void rl::clearIconsCache() {
    auto path = rl::getIconsCacheDir();
    if (std::filesystem::exists(path)) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Rating Cache

std::optional<matjson::Value> rl::getCachedLevelRating(int levelId) {
    return getCached("levelRatingCache", LevelRatingCache, levelId);
}

std::optional<matjson::Value> rl::getStaleLevelRating(int levelId) {
    return getStale("levelRatingCache", LevelRatingCache, levelId);
}

void rl::setCachedLevelRating(int levelId, matjson::Value const& data) {
    std::lock_guard lock(RequestCacheMutex);
    LevelRatingCache[levelId] = makeRequestCacheEntry(data);
    pruneCacheMap("levelRatingCache", LevelRatingCache);
    storeRequestCacheEntry("levelRatingCache", levelId, data);
}

void rl::removeCachedLevelRating(int levelId) {
    std::lock_guard lock(RequestCacheMutex);
    LevelRatingCache.erase(levelId);
    removeRequestCacheEntry("levelRatingCache", levelId);
    log::debug("Removed cached level rating for level {}", levelId);
}

////////////////////////////////////////////////////////////////////////////////
// Miscellaneous

/// Saves in a way that should avoid deadlocks.
static bool saveRequestCacheOnExit() {
    using namespace std::chrono_literals;
    if (!RequestCacheMutex.try_lock_for(2s)) return false;
    // Try saving the data;
    const bool saved = rl::saveRequestCacheRoot();
    if (saved) RequestCache.reset();
    RequestCacheMutex.unlock();
    return saved;
}

$on_game(Exiting) {
    // Ensure the data has been saved to a file.
    if (!saveRequestCacheOnExit()) {
        log::error("Could not save the request cache!");
    }
}
