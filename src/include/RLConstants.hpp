#pragma once

#include <utility>
#include <Geode/Geode.hpp>
#include <Geode/binding/GJAccountManager.hpp>
//#include <Geode/utils/base64.hpp>
#include "RLConfig.hpp"

using namespace geode::prelude;

namespace rl {
// Account ID of ArcticWoof
constexpr int ARCTICWOOF_ACCOUNT_ID = 7689052;

// Base URL for all Rated Layouts API endpoints.
inline constexpr std::string_view BASE_API_URL = "https://gdrate.arcticwoof.xyz";

struct Keys {
    static inline constexpr std::string_view USER_INFO = "user_info";
    //static inline constexpr std::string_view USER_INFO_ID = "user_info_id";
    static inline constexpr std::string_view NAMEPLATE = "selected_nameplate";
    static inline constexpr std::string_view COMPACT = "compact_mode";
    static inline constexpr std::string_view REROUTED = "did_reroute";
};

bool isUserHasPerms();
bool isUserDeveloper();

// global admin/mod
bool isUserAdmin();
bool isUserMod();

// specific admin/mod
bool isUserClassicRole();
bool isUserPlatformerRole();
bool isUserLeaderboardRole();

// supporter roles
bool isUserSupporter();

// check individual roles
bool isUserOwner();
bool isUserClassicAdmin();
bool isUserClassicMod();
bool isUserPlatformerAdmin();
bool isUserPlatformerMod();
bool isUserLeaderboardAdmin();
bool isUserLeaderboardMod();

enum class RLBadgeKind {
    Owner = 10,
    Developer = 12,
    ClassicAdmin = 5,
    PlatAdmin = 7,
    LeaderboardAdmin = 11,
    ClassicMod = 6,
    PlatMod = 8,
    LeaderboardMod = 9,
    Supporter = 3,
    Booster = 4,
};

struct RLBadgeInfo {
    static const char DEFAULT[];
    const char* id = "";
    const char* title = "";
    const char* desc = "";
    const char* sprite = "";
    const char* bigSprite = DEFAULT;
    RLBadgeKind kind = static_cast<RLBadgeKind>(-1);
    int prec = 5;

public:
    constexpr bool isValid() const {
        return int(RLBadgeKind::Supporter) <= int(kind) && int(kind) <= int(RLBadgeKind::Developer);
    }
    constexpr bool isDefaultBigSprite() const {
        return bigSprite == DEFAULT;
    }
    constexpr std::pair<const char*, bool> getBigSprite() const {
        if (isDefaultBigSprite()) return {sprite, false};
        else return {bigSprite, true};
    }
};

// clang-format off
inline constexpr RLBadgeInfo g_InfoOwner{
    "rl-owner"_spr,
    "Rated Layouts Owner",
    "<cf>This user</c> is the owner of <cl>Rated Layouts</c>. "
    "They have the ability to <cg>promote admins</c>, manages the entire <cg>community</c> "
    "and <co>moderation team</c> and has <cy>all permissions</c> within <cl>Rated Layouts</c>.",
    "RL_badgeOwner.png"_spr,
    "RL_badgeOwnerBig.png"_spr,
    RLBadgeKind::Owner, 1
};
inline constexpr RLBadgeInfo g_InfoDeveloper{
    "rl-developer"_spr,
    "Rated Layouts Developer",
    "<cf>This user</c> is a developer for <cl>Rated Layouts</c>. "
    "They are responsible for maintaining the <co>Backend Server</c>, <cb>Discord Bot</c>, "
    "<cp>Geode Mod</c> and other infrastructure of this mod.",
    "RL_badgeDeveloper.png"_spr,
    RLBadgeInfo::DEFAULT,
    RLBadgeKind::Developer, 1
};

inline constexpr RLBadgeInfo g_InfoClassicAdmin{
    "rl-classic-admin"_spr,
    "Classic Layout Admin",
    "<cr>Classic Layout Admin</c> has the ability to <cy>rate classic levels</c>, "
    "<cb>suggest classic levels</c>, <cc>manage Featured Layouts</c> and <cg>promote users "
    "into Classic Layout Mod</c> for <cl>Rated Layouts</c>.",
    "RL_badgeAdmin01.png"_spr,
    "RL_badgeAdminBig.png"_spr,
    RLBadgeKind::ClassicAdmin, 2
};
inline constexpr RLBadgeInfo g_InfoPlatAdmin{
    "rl-plat-admin"_spr,
    "Platformer Layout Admin",
    "<cr>Platformer Layout Admin</c> has the ability to <cy>rate platformer levels</c>, "
    "<cb>suggest platformer levels</c>, <cc>manage Featured Layouts</c> and <cg>promote users "
    "into Platformer Layout Mod</c> for <cl>Rated Layouts</c>.",
    "RL_badgePlatAdmin01.png"_spr,
    RLBadgeInfo::DEFAULT,
    RLBadgeKind::PlatAdmin, 2
};
inline constexpr RLBadgeInfo g_InfoLeaderboardAdmin{
    "rl-lb-admin"_spr,
    "LB Layout Admin",
    "<cr>Leaderboard Layout Admin</c> has the same abilities as <cc>Leaderboard Layout Moderator</c>, "
    "but can also <cf>whitelist</c> users and <cg>promote users into Leaderboard Layout Mod</c>.",
    "RL_badgelbAdmin01.png"_spr,
    RLBadgeInfo::DEFAULT,
    RLBadgeKind::LeaderboardAdmin, 2
};

inline constexpr RLBadgeInfo g_InfoClassicMod{
    "rl-classic-mod"_spr,
    "Classic Layout Mod",
    "<cb>Classic Layout Moderator</c> can suggest levels for classic layouts to <cr>Classic Layout Admins</c>.",
    "RL_badgeMod01.png"_spr,
    "RL_badgeModBig.png"_spr,
    RLBadgeKind::ClassicMod, 3
};
inline constexpr RLBadgeInfo g_InfoPlatMod{
    "rl-plat-mod"_spr,
    "Platformer Layout Mod",
    "<cb>Platformer Layout Moderator</c> can suggest levels for platformer layouts to <cr>Platformer Layout Admins</c>.",
    "RL_badgePlatMod01.png"_spr,
    RLBadgeInfo::DEFAULT,
    RLBadgeKind::PlatMod, 3
};
inline constexpr RLBadgeInfo g_InfoLeaderboardMod{
    "rl-lb-mod"_spr,
    "LB Layout Mod",
    "<cb>Leaderboard Layout Moderator</c> is responsible for <co>managing and "
    "moderating the leaderboard</c> section of <cl>Rated Layouts</c>.",
    "RL_badgelbMod01.png"_spr,
    RLBadgeInfo::DEFAULT,
    RLBadgeKind::LeaderboardMod, 3
};

inline constexpr RLBadgeInfo g_InfoSupporter{
    "rl-supporter"_spr,
    "Layout Supporter",
    "<cp>Layout Supporter</c> are those who donated to support the development of <cl>Rated Layouts</c>.",
    "RL_badgeSupporter.png"_spr,
    "RL_badgeSupporterBig.png"_spr,
    RLBadgeKind::Supporter, 4
};
inline constexpr RLBadgeInfo g_InfoBooster{
    "rl-booster"_spr,
    "Layout Booster",
    "<ca>Layout Booster</c> are those who boosted the <cl>Rated Layouts Discord server</c>, "
    "they also have the same benefits as <cp>Layout Supporter</c>.",
    "RL_badgeBooster.png"_spr,
    RLBadgeInfo::DEFAULT,
    RLBadgeKind::Booster, 4
};

inline constexpr RLBadgeInfo g_InfoUnknown{
    "", "Invalid Role", "???", ""
};
// clang-format on

constexpr inline const RLBadgeInfo* getBadgeInfo(RLBadgeKind K) {
    using enum RLBadgeKind;
    switch (K) {
        case Supporter:
            return &g_InfoSupporter;
        case Booster:
            return &g_InfoBooster;
        case ClassicAdmin:
            return &g_InfoClassicAdmin;
        case ClassicMod:
            return &g_InfoClassicMod;
        case PlatAdmin:
            return &g_InfoPlatAdmin;
        case PlatMod:
            return &g_InfoPlatMod;
        case LeaderboardMod:
            return &g_InfoLeaderboardMod;
        case Owner:
            return &g_InfoOwner;
        case LeaderboardAdmin:
            return &g_InfoLeaderboardAdmin;
        case Developer:
            return &g_InfoDeveloper;
        default:
            return &g_InfoUnknown;
    }
}

inline void showRoleInfoPopup(RLBadgeKind tag) {
    if (auto* info = getBadgeInfo(tag); info && info->isValid()) {
        FLAlertLayer::create(info->title, info->desc, "OK")->show();
    }
}
RL_ALWAYS_INLINE void showRoleInfoPopup(int tag) {
    showRoleInfoPopup(static_cast<RLBadgeKind>(tag));
}

}  // namespace rl
