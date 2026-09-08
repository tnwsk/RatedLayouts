#include "CachedSettings.hpp"
#include "utils/StartupFunctions.hpp"
#include <concepts>
#include <Geode/loader/ModEvent.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/binding/GJAccountManager.hpp>

using namespace geode::prelude;
using namespace rl;

static constinit comm::ListenerHandle* GlobalListener = nullptr;

// FIXME: Add updates for individual settings instead of reloading the whole object...
// TODO: Make all save data account specific...

static bool IsJSONObject(const matjson::Value& value) { return value.isObject(); }
static bool IsJSONArray(const matjson::Value& value) { return value.isArray(); }

static void setupGlobalListener(Mod* mod) {
    if (GlobalListener) return;
    ::GlobalListener = SettingChangedEventV3()
                           .listen(
                               [mod](std::string_view modID,
                                     std::string_view key,
                                     std::shared_ptr<SettingV3> setting) {
        if (geode::getModID(mod) == modID) {
            CachedSettings::get()->reload();
            log::info("Reloaded cached settings");
        }
    },
                               Priority::VeryEarly)
                           .leak();
}

template <class CachedSettingsTy, class CB>
    requires std::same_as<CachedSettings, std::remove_cv_t<CachedSettingsTy>>
static void runDefaults(CachedSettingsTy* CS, CB&& cb) {
    cb(CS->isCompact, Keys::COMPACT);
}

#define DECL_MODINFO_INIT(NAME, MODIDS)                        \
    static constexpr std::string_view NAME##_ids[] = {MODIDS}; \
    for (auto modid : NAME##_ids) {                            \
        if (CheckIfModIsOrWillBeLoaded(modid)) {               \
            this->NAME = true;                                 \
            break;                                             \
        }                                                      \
    }

RL_NO_INLINE static bool CheckIfModIsOrWillBeLoaded(std::string_view modid) {
    if (Mod* mod = Loader::get()->getInstalledMod(modid)) {
        // Baller if loaded...
        if (mod->isLoaded()) return true;
        if (mod->isCurrentlyLoading()) return true;
        return mod->shouldLoad();
    }
    return false;
}

CachedModsInfo::CachedModsInfo() {
    RL_SAVED_MODS(DECL_MODINFO_INIT)
}

static bool loadUserData(CachedSettings* CS, matjson::Value& data) {
    CS->userData.clear();
    const int currId = GJAccountManager::get()->m_accountID;
    if (currId <= 0) {
        log::warn("User not logged in!");
        return false;
    }
    // Change the cached account id.
    CS->userData.accountId = GJAccountManager::get()->m_accountID;
    // Load cached player data.
    auto infoMap = data.get(Keys::USER_INFO);
    if (!infoMap.isOkAnd(&IsJSONObject)) {
        log::warn("User info object not stored!");
        return false;
    }
    // Load the entry for the current account.
    std::string account = utils::numToString(currId);
    auto info = infoMap.unwrap().get(account);
    if (!info.isOkAnd(&IsJSONObject)) {
        log::warn("User info for account '{}' not stored!", currId);
        return false;
    }
    // Load this specific data.
    if (auto userData = info.unwrap().as<RLUserInfo>()) {
        CS->userData = std::move(userData).unwrap();
        CS->userData.accountId = currId;
        return true;
    }
    log::error("User info for account '{}' is invalid!", currId);
    return false;
}

void rl::updateCachedUserData() {
    // Don't update before we do anything cool...
    if (!GlobalListener) return;
    auto* CS = CachedSettings::get();
    CS->saveSettings();
    const int currId = GJAccountManager::get()->m_accountID;
    [[maybe_unused]] bool didLogin = loadUserData(CS, Mod::get()->getSaveContainer());
#if RL_DEV
    if (didLogin)
        Notification::create(fmt::format("Logged in with id: {}!", currId),
                             NotificationIcon::Success)
            ->show();
    else if (GJAccountManager::get()->m_accountID == 0)
        Notification::create("Logged out!", NotificationIcon::Success)->show();
    else
        Notification::create("User update failed.", NotificationIcon::Error)->show();
#endif
}

void rl::CachedSettings_init() {
    Mod* const mod = Mod::get();
    setupGlobalListener(mod);
    auto* CS = CachedSettings::get();
    loadUserData(CS, mod->getSaveContainer());
    // Load other stuff.
    runDefaults(CS, [mod]<class T>(T& into, std::string_view key) {
        if (mod->hasSavedValue(Keys::COMPACT)) into = mod->getSavedValue<T>(Keys::COMPACT);
    });
}

void CachedSettings::saveSettings() const {
    Mod* const mod = Mod::get();
    matjson::Value& data = mod->getSaveContainer();
    if (userData.accountId > 0) {
        std::string account = utils::numToString(userData.accountId);
        if (auto infoMap = data.get(Keys::USER_INFO); infoMap.isOkAnd(&IsJSONObject)) {
            // Here we just update the value for the user account.
            infoMap.unwrap()[account] = userData;
        } else {
            // Create a new container for the data.
            data[Keys::USER_INFO] = matjson::makeObject({{account, userData}});
        }
    }
    // Save other stuff.
    runDefaults(this, [mod](auto& from, std::string_view key) { mod->setSavedValue(key, from); });
}

$on_game(Exiting) { CachedSettings::get()->saveSettings(); }
