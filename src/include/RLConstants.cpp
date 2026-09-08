#include "RLConstants.hpp"
#include "utils/CachedSettings.hpp"

// clang-format off
using namespace rl;

constexpr const char RLBadgeInfo::DEFAULT[1] = {};

static RLUserInfo const& GetInfo() {
    return CachedSettings::get()->userData;
}

bool rl::isUserHasPerms() {
    // check if user has any roles by checking saved values
    RLUserInfo const& info = GetInfo();
    return info.isClassicMod ||
           info.isClassicAdmin ||
           info.isLeaderboardMod ||
           info.isLeaderboardAdmin ||
           info.isPlatMod ||
           info.isPlatAdmin;
}

bool rl::isUserDeveloper() {
    return GetInfo().isDeveloper;
}

// global admin/mod
bool rl::isUserAdmin() {
    RLUserInfo const& info = GetInfo();
    return info.isClassicAdmin ||
           info.isLeaderboardAdmin ||
           info.isPlatAdmin ||
           info.isDeveloper;
}

bool rl::isUserMod() {
    RLUserInfo const& info = GetInfo();
    return info.isClassicMod ||
           info.isLeaderboardMod ||
           info.isPlatMod ||
           info.isDeveloper;
}

// specific admin/mod
bool rl::isUserClassicRole() {
    RLUserInfo const& info = GetInfo();
    return info.isClassicAdmin || info.isClassicMod;
}

bool rl::isUserPlatformerRole() {
    RLUserInfo const& info = GetInfo();
    return info.isPlatAdmin || info.isPlatMod;
}

bool rl::isUserLeaderboardRole() {
    RLUserInfo const& info = GetInfo();
    return info.isLeaderboardAdmin || info.isLeaderboardMod;
}

// supporter roles
bool rl::isUserSupporter() {
    RLUserInfo const& info = GetInfo();
    return info.isSupporter || info.isBooster;
}

// check individual roles
bool rl::isUserOwner() {
    return GetInfo().isOwner;
}

bool rl::isUserClassicAdmin() {
    return GetInfo().isClassicAdmin;
}

bool rl::isUserClassicMod() {
    return GetInfo().isClassicMod;
}

bool rl::isUserPlatformerAdmin() {
    return GetInfo().isPlatAdmin;
}

bool rl::isUserPlatformerMod() {
    return GetInfo().isPlatMod;
}

bool rl::isUserLeaderboardAdmin() {
    return GetInfo().isLeaderboardAdmin;
}

bool rl::isUserLeaderboardMod() {
    return GetInfo().isLeaderboardMod;
}
