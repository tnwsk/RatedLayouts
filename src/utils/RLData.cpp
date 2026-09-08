#include "RLData.hpp"
#include <optional>
#include <string>
#include <unordered_map>
//#include <Geode/Geode.hpp>
#include <Geode/loader/Mod.hpp>
#include <Geode/loader/ModEvent.hpp>
#include <Geode/utils/base64.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/JsonValidation.hpp>
//#include <argon/argon.hpp>
//#include <arc/prelude.hpp>
#include <arc/future/Join.hpp>
#include <arc/future/Select.hpp>
#include <arc/sync/Mutex.hpp>
#include <arc/time/Sleep.hpp>
//#include <asp/time.hpp>
#include <fmt/ranges.h>
#include "RLNetworkUtils.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/NoHashHasher.hpp"
#include "utils/RLArgon.hpp"
#include "utils/StartupFunctions.hpp"
#include <Geode/utils/Keyboard.hpp>

using namespace geode::prelude;
using namespace rl;

// TODO: Merge async caches into one class

namespace {
enum { kDefaultCachePruningSize = 2048, kLocalEndpointRequestLifetime = 120 };
struct UserCacheEntry : public TimedCacheEntry<RLUserInfo> {
    using Base = TimedCacheEntry<RLUserInfo>;
    using TimedCacheEntry<RLUserInfo>::TimedCacheEntry;
    Base& base() & { return *this; }
    const Base& base() const& { return *this; }
    Base&& base() && { return std::move(*this); }
};
}  // namespace

template <>
struct matjson::Serialize<UserCacheEntry> {
    static Result<UserCacheEntry> fromJson(const matjson::Value& value) {
        const RLUserId ID = GEODE_UNWRAP(value["accountId"].asInt());
        UserCacheEntry entry;
        *entry = GEODE_UNWRAP(value.as<RLUserInfo>());
        entry.set(GEODE_UNWRAP(value["timestamp"].asInt()));
        entry->accountId = ID;
        return geode::Ok(entry);
    }
    static matjson::Value toJson(const UserCacheEntry& entry) {
        matjson::Value out;
        auto save = [&out](std::string_view key, auto value) {
            // Don't include values == 0.
            if (value) out[key] = value;
        };
        // Make the cache less nested...
        save("points", entry->points);
        save("stars", entry->stars);
        save("planets", entry->planets);
        save("nameplate", entry->nameplate);
        save("coins", entry->coins);
        save("votes", entry->votes);
        save("isSupporter", entry->isSupporter);
        save("isBooster", entry->isBooster);
        save("isClassicMod", entry->isClassicMod);
        save("isClassicAdmin", entry->isClassicAdmin);
        save("isLeaderboardMod", entry->isLeaderboardMod);
        save("isLeaderboardAdmin", entry->isLeaderboardAdmin);
        save("isPlatMod", entry->isPlatMod);
        save("isPlatAdmin", entry->isPlatAdmin);
        save("isDeveloper", entry->isDeveloper);
        save("isOwner", entry->isOwner);
        out["accountId"] = entry->accountId;
        out["timestamp"] = entry.expiresAt();
        return out;
    }
};

using IdHasher = NoHashHasher<RLUserId>;
using UserCacheType = std::unordered_map<RLUserId, UserCacheEntry, IdHasher>;
using LocalEndpointCacheType = std::unordered_map<std::string, RequestCacheEntry>;

// TODO: Make these RWLocks
static arc::Mutex<UserCacheType> UserCache;
static arc::Mutex<LocalEndpointCacheType> LocalEndpointCache;

#define $try_lock(OUT, ARGS...)                 \
    if (auto lock_ = co_await TryLockFor(ARGS)) \
        if (auto OUT = std::move(lock_).value(); true)

// clang-format off
template <typename MtxType>
static auto TryLockFor(MtxType& mtx, asp::Duration dur)
    -> arc::Future<std::optional<typename MtxType::Guard>> {
    std::optional<typename MtxType::Guard> out;
    co_await arc::select(
        arc::selectee(
            mtx.lock(),
            [&out](auto guard) { out.emplace(std::move(guard)); }
        ),
        arc::selectee(arc::sleepFor(dur))
    );
    co_return out;
}
// clang-format on

static RL_ALWAYS_INLINE bool isStale(RequestTimestamp timestamp) {
    return rl::isRequestCacheValid(timestamp);
}

static bool cacheMapNeedsPruning(size_t cacheSize, int64_t& maxItems) {
    if (maxItems <= 0) maxItems = kDefaultCachePruningSize;
    // Allow some extra entries to avoid constantly clearing cache (~1.75x).
    const size_t cacheMaxWithTolerance = size_t((maxItems * 7) / 4);
    return cacheSize > cacheMaxWithTolerance;
}

static void pruneCacheMap(UserCacheType& cache) {
    //int64_t maxItems = getRequestCacheMaxItems();
    int64_t maxItems = kDefaultCachePruningSize;
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
    log::debug("Pruned in-memory user cache");
}

static Result<matjson::Value> parseServerResponse(web::WebResponse const& response) {
    auto asJson = response.json();
    if (!asJson) {
        return Err(fmt::format("Response was not valid JSON: {}", asJson.unwrapErr()));
    }
    auto json = std::move(asJson).unwrap();
    if (!json.isObject()) {
        return Err(fmt::format("Expected object, got {}", jsonValueTypeToString(json.type())));
    }
    return Ok(std::move(json));
}

static std::string parseServerError(web::WebResponse const& error, RLUserId id) {
    if (error.code() == 404) return fmt::format("User info for '{}' not found on server", id);
    return fmt::format("User info for '{}' failed with code {}", id, error.code());
}

static RLUserInfo::ResFuture getUserInfoFromWeb(RLUserId id) {
    ARC_FRAME();
    auto token = co_await RLArgon::resolveAsync();
    if (token.isErr()) {
        log::warn("Argon token missing, aborting role fetch for {}", id);
        co_return Err(std::move(token).unwrapErr());
    }

    auto response = co_await rl::createWebRequest(
                        {{"accountId", id}, {"argonToken", std::move(token).unwrap()}})
                        .post(rl::getAPIEndpoint("profile"));

    log::trace("/profile[{}]: Received response from server", id);
    if (!response.ok()) co_return Err(parseServerError(response, id));

    // Get the actual json response.
    matjson::Value data = ARC_CO_UNWRAP(parseServerResponse(response));
    RLUserInfo out = ARC_CO_UNWRAP(data.as<RLUserInfo>());
    out.accountId = id;
    co_return Ok(std::move(out));
}

template <bool AlwaysEmplace = false>
static RLUserInfo::ResFuture getNewEntry(RLUserId id) noexcept {
    ARC_FRAME();
    RLUserInfo newInfo = ARC_CO_UNWRAP(co_await getUserInfoFromWeb(id));
    auto cache = co_await UserCache.lock();
    if (!cache->contains(id)) {
        pruneCacheMap(*cache);
        cache->emplace(id, UserCacheEntry(Expires::DEFAULT, newInfo));
    } else if constexpr (AlwaysEmplace) {
        UserCacheEntry& curr = cache->at(id);
        if (curr.isStale()) curr = UserCacheEntry(Expires::DEFAULT, newInfo);
    }
    co_return Ok(newInfo);
}

static void fetchStaleUserInfoInTheBackground(RLUserId id) {
    async::spawn([id]() -> arc::Future<> {
        log::debug("Fetching user info for stale request '{}'", id);
        auto res = co_await getNewEntry</*AlwaysEmplace=*/true>(id);
        if (res.isOk())
            log::info("Fetched for stale user info '{}'", id);
        else
            log::error("Failed to fetch for stale user info '{}': {}", id, res.unwrapErr());
        co_return;
    });
}

RLUserInfo::ResFuture RLUserInfo::get(RLUserId id, bool useCache) {
    ARC_FRAME();
    if (!useCache) {
        co_return co_await getUserInfoFromWeb(id);
    }

    /*Initial check*/ {
        auto cache = co_await UserCache.lock();
        if (cache->contains(id)) {
            auto [timestamp, info] = cache->at(id).base();
            if (timestamp.isStale()) fetchStaleUserInfoInTheBackground(id);
            co_return Ok(info);
        }
    }

    co_return co_await getNewEntry(id);
}

////////////////////////////////////////////////////////////////////////////////
// LocalEndpoint

using Builder = LocalEndpoint::Builder;

namespace {
struct LocalEndpointData {
    std::string name;
    std::string endpoint;
    std::string key;
    asp::Duration expires;
    std::optional<matjson::Value> body;
    LocalEndpoint::Verifier verify_ = nullptr;
    bool nostale = false;
    bool progress = false;

public:
    LocalEndpointData(std::string_view name_, bool init);
    LocalEndpointData(std::string_view name_);
    LocalEndpointData(std::string_view name_, matjson::Value&& body_);
    LocalEndpointData(Builder&& builder);

    Result<> verify(matjson::Value const& value) {
        if (verify_)
            return verify_(value);
        else
            return Ok();
    }

    RL_ALWAYS_INLINE void log(std::string_view msg) const {
        if (progress) this->logImpl(msg);
    }

private:
    void initName(std::string_view name_) {
        this->name = std::string(name_.substr(0, name_.find('?')));
    }
    void initWithParams(std::string_view name_,
                        LocalEndpoint::Params params,
                        LocalEndpoint::Params nkparams) {
        if (params.empty() && nkparams.empty()) {
            // Skip parameter initialization
            return initName(name_);
        }
        // Init with some extra values.
        const size_t pos = name_.find('?');
        char prefix = (pos == std::string_view::npos) ? '?' : '&';
        this->name = std::string(name_.substr(0, pos));

        if (!nkparams.empty()) {
            this->endpoint += fmt::format("{}{}", prefix, fmt::join(nkparams, "&"));
            prefix = '&';
        }
        if (!params.empty()) {
            std::string formatted = fmt::format("{}{}", prefix, fmt::join(params, "&"));
            this->key += formatted;
            this->endpoint += formatted;
        }
    }
    void initBody(matjson::Value&& body_) {
        if (body.has_value()) [[unlikely]] {
            log::warn("Body for '{}' already has value!", name);
            return;
        }
        body.emplace(std::move(body_));
        this->key.push_back('#');
        this->key += base64::encode(body->dump(0), base64::Base64Variant::NormalNoPad);
    }

    void logImpl(std::string_view msg) const;
};
}  // namespace

LocalEndpointData::LocalEndpointData(std::string_view name_, bool init) :
    endpoint(rl::getAPIEndpoint(name_)), key(name_), expires(getRequestCacheExpiresDuration()) {
    // Check if we want the name to be automatically initialized...
    if (init) this->initName(name_);
}

LocalEndpointData::LocalEndpointData(std::string_view name_) : LocalEndpointData(name_, true) {}

LocalEndpointData::LocalEndpointData(std::string_view name_, matjson::Value&& body_) :
    LocalEndpointData(name_, true) {
    this->initBody(std::move(body_));
}

LocalEndpointData::LocalEndpointData(Builder&& builder) : LocalEndpointData(builder.m_name, false) {
    this->initWithParams(builder.m_name, builder.getParams(), builder.getNKParams());
    if (builder.m_body) this->initBody(*std::move(builder.m_body));
    if (builder.m_expires) this->expires = *builder.m_expires;
    this->verify_ = std::move(builder.m_verify);
    this->nostale = builder.m_nostale;
    this->progress = builder.m_progress;
}

void LocalEndpointData::logImpl(std::string_view msg) const {
    log::debug("{} progress: {}", name, msg);
}

static web::WebRequest setupRemote(LocalEndpointData const& data, bool withAuth) {
    web::WebRequest req;
    auto body = data.body;
    if (withAuth) {
        if (!body) body = matjson::Value::object();
        body->set("accountId", CachedSettings::user()->accountId);
        body->set("argonToken", RLArgon::token());
    }
    if (body) req.bodyJSON(*body);
    // Log progress
    //if (data.progress) {
    //    req.onProgress([name = data.name](web::WebProgress const& prog) {
    //        log::debug("{} progress: {}\% Download, {}\% Upload",
    //                   name,
    //                   prog.downloadProgress().value_or(0),
    //                   prog.uploadProgress().value_or(0));
    //    });
    //}
    return req;
}

static LocalEndpoint::ResFuture getRemoteEndpointCommon(web::WebResponse response,
                                                        LocalEndpointData data,
                                                        bool withAuth,
                                                        bool alwaysEmplace = false) {
    // FIXME: Handle auth refreshing
    if (!response.ok()) {
        log::warn("/{} returned non-ok status: {}", data.name, response.code());
        co_return Err(fmt::format("Failed to fetch from /{}", data.name));
    }

    auto jsonOrErr = response.json();
    if (!jsonOrErr) {
        log::warn("Failed to parse /{} JSON", data.name);
        co_return Err(fmt::format("Invalid server response", data.name));
    }

    auto json = std::move(jsonOrErr).unwrap();
    if (auto err = data.verify(json); err.isErr()) {
        std::string msg = std::move(err).unwrapErr();
        if (msg.empty())
            co_return Err("JSON verification failed");
        else
            co_return Err(std::move(msg));
    }

    auto cache = co_await LocalEndpointCache.lock();
    if (!cache->contains(data.key)) {
        cache->emplace(data.key, RequestCacheEntry(data.expires, json));
        data.log("added entry to cache");
    } else if (alwaysEmplace) {
        RequestCacheEntry& curr = cache->at(data.key);
        if (curr.isStale()) {
            curr = RequestCacheEntry(data.expires, json);
            data.log("added entry to cache (forced)");
        }
    }

    co_return Ok(std::move(json));
}

template <bool WithAuth>
static LocalEndpoint::ResFuture getRemoteEndpoint(LocalEndpointData data,
                                                  bool alwaysEmplace = false) {
    ARC_FRAME();
    data.log("setting up remote endpoint");
    web::WebRequest req = setupRemote(data, WithAuth);
    auto response = co_await req.get(data.endpoint);
    data.log("fetching remote endpoint");
    co_return co_await getRemoteEndpointCommon(
        std::move(response), std::move(data), WithAuth, alwaysEmplace);
}

template <bool WithAuth>
static void fetchRemoteEndpointInTheBackground(LocalEndpointData&& data) {
    async::spawn([data = std::move(data)]() mutable -> arc::Future<> {
        std::string name = data.name;
        log::debug("Fetching for stale request at /{}", name);
        auto res = co_await getRemoteEndpoint<WithAuth>(std::move(data), /*alwaysEmplace=*/true);
        if (res.isOk())
            log::info("Fetched for stale request at /{}", name);
        else
            log::error("Failed to fetch for stale request at /{}: {}", name, res.unwrapErr());
        co_return;
    });
}

template <bool WithAuth>
static LocalEndpoint::ResFuture getLocalEndpointCommon(LocalEndpointData data) {
    ARC_FRAME();

    /*Initial Check*/ {
        auto cache = co_await LocalEndpointCache.lock();
        if (cache->contains(data.key)) {
            data.log("found entry in cache");
            auto [timestamp, response] = cache->at(data.key);
            const bool isStale = timestamp.isStale();
            if (!data.nostale || !isStale) {
                if (isStale) fetchRemoteEndpointInTheBackground<WithAuth>(std::move(data));
                data.log("returning cached response");
                co_return Ok(std::move(response));
            }
            data.log("response is stale and nostale is on");
        }
    }

    co_return co_await getRemoteEndpoint<WithAuth>(std::move(data), data.nostale);
}

LocalEndpoint::ResFuture LocalEndpoint::get(std::string name) {
    ARC_FRAME();
    LocalEndpointData data(name);
    co_return co_await getLocalEndpointCommon</*WithAuth=*/false>(std::move(data));
}

LocalEndpoint::ResFuture LocalEndpoint::get(std::string name, matjson::Value body) {
    ARC_FRAME();
    LocalEndpointData data(name, std::move(body));
    co_return co_await getLocalEndpointCommon</*WithAuth=*/false>(std::move(data));
}

LocalEndpoint::ResFuture LocalEndpoint::getWithAuth(std::string name) {
    ARC_FRAME();
    LocalEndpointData data(name);
    co_return co_await getLocalEndpointCommon</*WithAuth=*/true>(std::move(data));
}

LocalEndpoint::ResFuture LocalEndpoint::getWithAuth(std::string name, matjson::Value body) {
    ARC_FRAME();
    LocalEndpointData data(name, std::move(body));
    co_return co_await getLocalEndpointCommon</*WithAuth=*/true>(std::move(data));
}

LocalEndpoint::ResFuture Builder::get() {
    LocalEndpointData data(std::move(*this));
    return getLocalEndpointCommon</*WithAuth=*/false>(std::move(data));
}

LocalEndpoint::ResFuture Builder::getWithAuth() {
    LocalEndpointData data(std::move(*this));
    return getLocalEndpointCommon</*WithAuth=*/true>(std::move(data));
}

// Verify

Builder& Builder::verifySuccess(std::string_view id) {
    return this->verify([id = std::string(id)](matjson::Value const& res) -> Result<> {
        if (!res["success"].asBool().unwrapOr(false)) [[unlikely]]
            return Err(fmt::format("server returned success=false for {}", id));
        return Ok();
    });
}
Builder& Builder::verifySuccess() { return this->verifySuccess(this->m_name); }

// Params

static void AddParam(Builder* self, auto& params, std::string_view name, auto&& data) {
    if (!name.empty())
        params.emplace_back(fmt::format("{}={}", name, data));
    else
        log::warn("Passed empty parameter to '{}'", self->m_name);
}

Builder& Builder::param(std::string_view name, std::string_view data) {
    AddParam(this, m_params, name, data);
    return *this;
}
Builder& Builder::nkparam(std::string_view name, std::string_view data) {
    AddParam(this, m_nkparams, name, data);
    return *this;
}

////////////////////////////////////////////////////////////////////////////////
// Miscellaneous

bool rl::hasRLDataCache() {
    /*UserInfo*/ {
        auto cache = UserCache.blockingLock();
        if (!cache->empty()) return true;
    }
    /*LocalEndpoint*/ {
        auto cache = LocalEndpointCache.blockingLock();
        if (!cache->empty()) return true;
    }
    return false;
}

void rl::clearRLDataCache() {
    async::spawn([]() -> arc::Future<> {
        auto cache = co_await UserCache.lock();
        cache->clear();
        log::info("Cleared UserCache");
    });
    async::spawn([]() -> arc::Future<> {
        auto cache = co_await LocalEndpointCache.lock();
        cache->clear();
        log::info("Cleared LocalEndpointCache");
    });
}

static constexpr std::string_view CacheFileName = "data_cache.json";

#if 1
static Result<matjson::Value> loadDataCacheRootFromFile() {
    auto path = getDataCacheZipPath();
    if (!std::filesystem::exists(path))
        return Err("data cache does not exist");
    Result<file::Unzip> zip = file::Unzip::create(path);
    if (!zip)
        return Err(fmt::format("failed to read from \"{}\": {}",
                               utils::string::pathToString(path),
                               zip.unwrapErr()));
    auto rawCacheData = zip.unwrap().extract(CacheFileName);
    if (!rawCacheData)
        return Err(fmt::format("failed to read data cache from zip: {}",
                               rawCacheData.unwrapErr()));
    auto cacheData = std::move(rawCacheData).unwrap();
    std::string_view existing(reinterpret_cast<char*>(cacheData.data()), cacheData.size());
    GEODE_UNWRAP_INTO(matjson::Value root, matjson::parse(existing));
    if (!root.isObject()) return Err("root is not an object!");
    return Ok(std::move(root));
}

static void saveDataCacheRootToFile(matjson::Value const& root) {
    auto path = getDataCacheZipPath();
    std::filesystem::create_directories(path.parent_path());
    Result<file::Zip> zip = file::Zip::create(path);
    if (zip.isErr()) {
        log::error("Failed to create data cache zip: {}", zip.unwrapErr());
        return;
    }
    auto writeRes = zip.unwrap().add(CacheFileName, root.dump(0));
    //auto writeRes = utils::file::writeStringSafe(path, root.dump(0));
    if (writeRes.isOk())
        log::info("Saved data cache to file: {}", utils::string::pathToString(path));
    else
        log::error("Failed to save data cache: {}", writeRes.unwrapErr());
}
#else
static Result<matjson::Value> loadDataCacheRootFromFile() {
    auto path = getDataCachePath();
    std::filesystem::create_directories(path.parent_path());
    auto existing = utils::file::readString(path);
    if (!existing)
        return Err(fmt::format("failed to read from \"{}\": {}",
                               utils::string::pathToString(path),
                               existing.unwrapErr()));
    GEODE_UNWRAP_INTO(matjson::Value root, matjson::parse(existing.unwrap()));
    if (!root.isObject()) return Err("root is not an object!");
    return Ok(std::move(root));
}

static void saveDataCacheRootToFile(matjson::Value const& root) {
    auto path = getDataCachePath();
    std::filesystem::create_directories(path.parent_path());
    auto writeRes = utils::file::writeStringSafe(path, root.dump(0));
    if (writeRes.isOk())
        log::info("Saved data cache to file: {}", utils::string::pathToString(path));
    else
        log::error("Failed to save data cache: {}", writeRes.unwrapErr());
}
#endif

static arc::Future<> loadUserInfo(std::vector<matjson::Value> info) {
    std::vector<UserCacheEntry> entries;
    entries.reserve(info.size());
    for (auto const& val : info) {
        auto entry = val.as<UserCacheEntry>();
        if (entry.isErr()) continue;
        entries.emplace_back(std::move(entry).unwrap());
    }
    /*Load the entries*/ {
        auto cache = co_await UserCache.lock();
        cache->reserve(entries.size());
        for (auto const& entry : entries) {
            const RLUserId accountId = entry->accountId;
            if (cache->contains(accountId)) continue;
            cache->emplace(accountId, entry);
        }
    }
    log::info("Loaded {} user info entries from file", entries.size());
    co_return;
}

static std::vector<matjson::Value> saveUserInfo() {
    std::vector<matjson::Value> out;
    auto cache = UserCache.blockingLock();
    out.reserve(cache->size());
    for (auto const& [_, entry] : *cache) out.emplace_back(entry);
    cache->clear();
    return out;
}

static arc::Future<> loadLocalEndpoint(matjson::Value info) {
    std::vector<std::pair<std::string, RequestCacheEntry>> entries;
    entries.reserve(info.size());
    for (auto const& [endpoint, val] : info) {
        auto entry = val.as<RequestCacheEntry>();
        if (entry.isErr()) continue;
        entries.emplace_back(endpoint, std::move(entry).unwrap());
    }
    /*Load the entries*/ {
        auto cache = co_await LocalEndpointCache.lock();
        cache->reserve(entries.size());
        for (auto& [endpoint, entry] : entries) {
            if (cache->contains(endpoint)) continue;
            cache->emplace(std::move(endpoint), std::move(entry));
        }
    }
    log::info("Loaded {} user info entries from file", entries.size());
    co_return;
}

static matjson::Value saveLocalEndpoint() {
    matjson::Value out;
    auto cache = LocalEndpointCache.blockingLock();
    for (auto const& [endpoint, entry] : *cache) out[endpoint] = entry;
    cache->clear();
    return out;
}

void rl::RLData_init() {
    auto rootOrErr = loadDataCacheRootFromFile();
    if (rootOrErr.isErr()) {
        log::warn("Failed to load cache data: {}", rootOrErr.unwrapErr());
        return;
    }

    matjson::Value root = std::move(rootOrErr).unwrap();
    log::info("Loading cached data");
    //log::debug("{}", root.dump(0));
    if (root.contains("userInfo")) {
        auto& data = root["userInfo"];
        if (auto arr = data.asArray())
            async::spawn(loadUserInfo(std::move(arr.unwrap())));
        else
            log::warn("\"userInfo\" expected array, got {}", jsonValueTypeToString(data.type()));
    }
    if (root.contains("localEndpoint")) {
        auto& data = root["localEndpoint"];
        if (data.isObject())
            async::spawn(loadLocalEndpoint(std::move(data)));
        else
            log::warn("\"localEndpoint\" expected object, got {}",
                      jsonValueTypeToString(data.type()));
    }
}

$on_game(Exiting) {
    matjson::Value out;
    out["userInfo"] = saveUserInfo();
    out["localEndpoint"] = saveLocalEndpoint();
    saveDataCacheRootToFile(out);
}
