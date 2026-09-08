#pragma once

#include <Geode/loader/Mod.hpp>
#include "utils/RLData.hpp"
#include "CachedSettings.def"

#define DECL_SETTING(NAME, TYPE, SETTING) TYPE NAME = geode::Mod::get()->getSettingValue<TYPE>(SETTING);
#define DECL_MODINFO(NAME, ...) bool NAME : 1 = false;

namespace rl {

/// Holds all the settings in the mod itself.
struct CachedSettingsBase {
    RL_SAVED_SETTINGS(DECL_SETTING)
public:
    void reload() { *this = CachedSettingsBase{}; }
};

/// Holds flags for mods.
struct CachedModsInfo {
    RL_SAVED_MODS(DECL_MODINFO)
public:
    CachedModsInfo();
};

/// Holds all the settings in the mod, as well as some for the running program.
struct CachedSettings : public CachedSettingsBase {
    RLUserInfo userData = {};
    bool isCompact = false;
    // Non saved values
    bool isBadgeAPILoaded = false;
    bool isUserGDMod = false;
    const CachedModsInfo modsData;

public:
    class Updater {
        CachedSettings& self;

    public:
        Updater(CachedSettings& thiz_) : self(thiz_) {}
        ~Updater() { self.saveSettings(); }
        CachedSettings* operator->() { return &self; }
        CachedSettings& operator*() { return self; }
    };

    static CachedSettings* get() {
        static CachedSettings settings;
        return &settings;
    }

    static RLUserInfo* user() {
        return &get()->userData;
    }

    static const CachedModsInfo* mods() {
        return &get()->modsData;
    }

    /// Creates an RAII handler for
    static CachedSettings::Updater update() {
        return Updater(*CachedSettings::get());
    }

    using CachedSettingsBase::reload;

    void saveSettings() const;
};

void updateCachedUserData();

}  // namespace rl

#undef DECL_SETTING
#undef DECL_MODINFO
