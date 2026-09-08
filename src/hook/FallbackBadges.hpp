#pragma once

#include <string_view>
#include <Geode/utils/cocos.hpp>
#include "RLConstants.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/RLData.hpp"

namespace rl {

template <class Self>
inline void setupFallbackBadgesImpl(geode::Ref<Self> self, RLUserInfo const& info,
                                    std::string_view name, float spriteScale = 1.0f) {
    if (CachedSettings::get()->isBadgeAPILoaded) return;
    if (!self->m_mainLayer) {
        log::warn("main layer is null, cannot load badge for {}", name);
        return;
    }
    
    geode::Ref<CCMenu> usernameMenu = typeinfo_cast<CCMenu*>(
        self->m_mainLayer->getChildByIDRecursive("username-menu"));
    if (!usernameMenu) {
        log::warn("username-menu not found in {}", name);
        return;
    }

    // Update layout!
    auto addBadgeItem = [=, &info](RLBadgeKind K) {
        if (!rl::doesUserHaveBadge(K, info)) return;
        auto* badgeInfo = rl::getBadgeInfo(K);
        // Check if the badge already exists
        std::string id = fmt::format("{}-{}:{}", badgeInfo->id, name, badgeInfo->prec);
        if (usernameMenu->getChildByID(id)) return;
        // Create the sprite
        auto* sprite = CCSprite::createWithSpriteFrameName(badgeInfo->sprite);
        if (!sprite) return;
        sprite->setScale(spriteScale);
        // Set up the button
        auto* btn = CCMenuItemSpriteExtra::create(
            sprite, self, menu_selector(Self::onBadgeClicked));
        btn->setTag(static_cast<int>(K));
        btn->setID(std::move(id));
        usernameMenu->addChild(btn);
    };

    addBadgeItem(RLBadgeKind::Owner);
    addBadgeItem(RLBadgeKind::Developer);
    addBadgeItem(RLBadgeKind::ClassicAdmin);
    addBadgeItem(RLBadgeKind::PlatAdmin);
    addBadgeItem(RLBadgeKind::LeaderboardAdmin);
    addBadgeItem(RLBadgeKind::ClassicMod);
    addBadgeItem(RLBadgeKind::PlatMod);
    addBadgeItem(RLBadgeKind::LeaderboardMod);
    addBadgeItem(RLBadgeKind::Supporter);
    addBadgeItem(RLBadgeKind::Booster);
    usernameMenu->updateLayout();
}

template <class Self>
inline void clearFallbackBadgesImpl(geode::Ref<Self> self, std::string_view name) {
    if (CachedSettings::get()->isBadgeAPILoaded) return;
    if (!self->m_mainLayer) return;

    geode::Ref<CCMenu> usernameMenu = typeinfo_cast<CCMenu*>(
        self->m_mainLayer->getChildByIDRecursive("username-menu"));
    if (!usernameMenu) {
        log::warn("username-menu not found in {}", name);
        return;
    }

    // Update layout!
    auto removeBadgeItem = [=](RLBadgeKind K) {
        auto* badgeInfo = rl::getBadgeInfo(K);
        // Check if the badge already exists
        std::string id = fmt::format("{}-{}", badgeInfo->id, name);
        if (auto* badge = usernameMenu->getChildByID(id))
            badge->removeFromParent();
    };

    removeBadgeItem(RLBadgeKind::Owner);
    removeBadgeItem(RLBadgeKind::Developer);
    removeBadgeItem(RLBadgeKind::ClassicAdmin);
    removeBadgeItem(RLBadgeKind::PlatAdmin);
    removeBadgeItem(RLBadgeKind::LeaderboardAdmin);
    removeBadgeItem(RLBadgeKind::ClassicMod);
    removeBadgeItem(RLBadgeKind::PlatMod);
    removeBadgeItem(RLBadgeKind::LeaderboardMod);
    removeBadgeItem(RLBadgeKind::Supporter);
    removeBadgeItem(RLBadgeKind::Booster);
    usernameMenu->updateLayout();
}

}  // namespace rl
