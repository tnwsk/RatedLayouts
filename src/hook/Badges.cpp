#include <Geode/Geode.hpp>
#include "RLConfig.hpp"
#include "RLConstants.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/RandomGen.hpp"
#include "utils/RLData.hpp"
#include "utils/StartupFunctions.hpp"

#if __has_include(<dasshu.badgified/include/Badgified.hpp>)
# include <dasshu.badgified/include/Badgified.hpp>
# undef MY_MOD_ID
# define HAS_BADGIFIED 1
#endif
#if __has_include(<alphalaneous.badgify/include/Badgify.hpp>)
# include <alphalaneous.badgify/include/Badgify.hpp>
# undef MY_MOD_ID
# define HAS_BADGIFY 1
#endif

using namespace geode::prelude;
using namespace rl;

namespace {
#if HAS_BADGIFIED
struct BadgifiedAPI {
    static constexpr std::string_view id = "dasshu.badgified";
    using Badge = dasshu::badgified::Badge;
    using Location = dasshu::badgified::Location;
    static constexpr auto& showBadge = dasshu::badgified::showBadge;
    static constexpr auto& registerBadge = dasshu::badgified::registerBadge;
};
#endif
#if HAS_BADGIFY
struct BadgifyAPI {
    static constexpr std::string_view id = "alphalaneous.badgify";
    using Badge = alpha::badgify::Badge;
    using Location = alpha::badgify::Location;
    static constexpr auto& showBadge = alpha::badgify::showBadge;
    static constexpr auto& registerBadge = alpha::badgify::registerBadge;
};
#endif
}  // namespace

#if HAS_BADGIFY || HAS_BADGIFIED
template <class API>
static void isUserInBadge(RLBadgeKind K, typename API::Badge badge) {
    const RLUserId accountId = badge.user->m_accountID;
    async::spawn(RLUserInfo::get(accountId),
                 [K, badge = std::move(badge)](Result<RLUserInfo> info) {
        if (info.isErr()) [[unlikely]] {
            log::warn("Could not load user info: {}", info.unwrapErr());
            return;
        }
        if (!rl::doesUserHaveBadge(K, info.unwrap())) return;
        // Show badge
        Loader::get()->queueInMainThread([K, badge = std::move(badge)]() {
            const char* spriteName = getBadgeInfo(K)->sprite;
            API::showBadge(badge, CCSprite::createWithSpriteFrameName(spriteName));
        });
    });
}

template <class API>
static void userBadgePopup(RLBadgeKind K, typename API::Badge badge) {
    const RLUserId accountId = badge.user->m_accountID;
    async::spawn(RLUserInfo::get(accountId),
                 [K, badge = std::move(badge)](Result<RLUserInfo> info) {
        if (info.isErr()) [[unlikely]] {
            log::warn("Could not load user info: {}", info.unwrapErr());
            return;
        }
        if (!rl::doesUserHaveBadge(K, info.unwrap())) return;
        // Show badge
        Loader::get()->queueInMainThread([K, badge = std::move(badge)]() {
            // TODO: Add bigSprite
            auto [spriteName, isCustom] = getBadgeInfo(K)->getBigSprite();
            if (isCustom) {
                auto* sprite = CCSprite::createWithSpriteFrameName(spriteName);
                sprite->setScale(1.2);
                API::showBadge(badge, sprite);
            } else {
                auto* sprite = CCSpriteGrayscale::createWithSpriteFrameName(spriteName);
                sprite->setScale(1.2);
                CCSize spriteSize = sprite->getContentSize();

                //auto* cross = CCSprite::createWithSpriteFrameName("RL_cross_no_box.png"_spr);
                //cross->setScale((spriteSize.height / cross->getContentHeight()) * 0.9f);
                //cross->setAnchorPoint({0.5f, 0.5f});
                //cross->setPosition(spriteSize / 2);
                //cross->setZOrder(sprite->getZOrder() + 1);
                //sprite->addChild(cross);

                CCLabelBMFont* lbl = CCLabelBMFont::create("Temp", "gjFont14.fnt");
                lbl->setAnchorPoint({0.5f, 0.5f});
                lbl->setRotation(30.f + globalRNG()->generate<float>(-10.f, +10.f));
                lbl->setColor(ccc3(227, 42, 12));
                lbl->setScale((spriteSize.width / lbl->getContentWidth()) * 0.9f);
                lbl->setPosition(spriteSize / 2);
                lbl->setZOrder(sprite->getZOrder() + 2);
                sprite->addChild(lbl);

                API::showBadge(badge, sprite);
            }
        });
    });
}

template <class API, RLBadgeKind K>
static void badgeCallback(const typename API::Badge& badge) {
    using Location = typename API::Location;
    if (badge.location == Location::Profile || badge.location == Location::Comment)
        isUserInBadge<API>(K, badge);
    else if (badge.location == Location::InfoPopup)
        userBadgePopup<API>(K, badge);
}

static constexpr std::string_view kBigBadgeMessage = "If you have the original icon for this badge, let us know.";

template <class API, RLBadgeKind K>
RL_NO_INLINE static void registerRLBadge() {
    constexpr const RLBadgeInfo* info = rl::getBadgeInfo(K);
    static_assert(info && info->isValid(), "Invalid badge type!");
    if (!info->isDefaultBigSprite())
        API::registerBadge(info->id, info->title, info->desc, &badgeCallback<API, K>);
    else {
        std::string desc = fmt::format("{}\n\n<cr>{}</c>", info->desc, kBigBadgeMessage);
        API::registerBadge(info->id, info->title, desc, &badgeCallback<API, K>);
    }
}

// TODO: Add larger, high quality versions of each badge.
template <class API>
static void initRLBadges() {
    registerRLBadge<API, RLBadgeKind::Owner>();
    registerRLBadge<API, RLBadgeKind::Developer>();
    registerRLBadge<API, RLBadgeKind::ClassicAdmin>();
    registerRLBadge<API, RLBadgeKind::PlatAdmin>();
    registerRLBadge<API, RLBadgeKind::LeaderboardAdmin>();
    registerRLBadge<API, RLBadgeKind::ClassicMod>();
    registerRLBadge<API, RLBadgeKind::PlatMod>();
    registerRLBadge<API, RLBadgeKind::LeaderboardMod>();
    registerRLBadge<API, RLBadgeKind::Supporter>();
    registerRLBadge<API, RLBadgeKind::Booster>();
    log::info("Set up badges with {}!", API::id);
    CachedSettings::get()->isBadgeAPILoaded = true;
}
#endif  // HAS_BADGIFY || HAS_BADGIFIED

bool rl::Badges_init() {
#if HAS_BADGIFIED
    if (CachedSettings::mods()->badgified) {
        // Should be loaded, buuuut just in case...
        dasshu::badgified::waitForBadgified(&initRLBadges<BadgifiedAPI>);
        return true;
    }
#endif
    // Badgified not installed or unavailable
    log::info("dasshu.badgified not installed, trying badgify...");
#if HAS_BADGIFY
    if (CachedSettings::mods()->badgify) {
        // Should be loaded, buuuut just in case...
        alpha::badgify::waitForBadgify(&initRLBadges<BadgifyAPI>);
        return true;
    }
#endif
    // Badgify not installed or unavailable
    log::warn("alphalaneous.badgify not installed, using defaults");
    return false;
}
