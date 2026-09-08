#include <Geode/Geode.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/binding/GameToolbox.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/terminate.hpp>
#include <arc/future/Future.hpp>
#include <argon/argon.hpp>
#include "FallbackBadges.hpp"

#include "RLAchievements.hpp"
#include "RLConstants.hpp"
#include "RLNetworkUtils.hpp"
#include "layer/RLLevelBrowserLayer.hpp"
#include "player/RLDifficultyTotalPopup.hpp"
#include "player/RLUserControl.hpp"
#include "player/RLUserLevelControl.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/RLArgon.hpp"
#include "utils/RLData.hpp"

#include "Geode/cocos/label_nodes/CCLabelBMFont.h"
#include "Geode/loader/Mod.hpp"
#include "Geode/ui/BasedButtonSprite.hpp"
#include "Geode/utils/general.hpp"

// TODO: Merge implementation between this and CommentCell

using namespace geode::prelude;
using namespace rl;

class $modify(RLProfilePage, ProfilePage) {
    struct Fields : rl::RLUserInfo {
        async::TaskHolder<Result<RLUserInfo>> m_profileTask;
        //async::TaskHolder<Result<std::string>> m_authTask;
        async::TaskHolder<> m_authTask;
        bool m_profileInFlight = false;
        int m_profileForAccount = -1;

        CCMenu* m_rlButtonsMenu = nullptr;
        CCMenu* m_arrowMenu = nullptr;
        NineSlice* m_rlButtonBg = nullptr;
        CCMenu* m_rlStatsMenu = nullptr;
        CCMenuItemSpriteExtra* m_rlToggleArrow = nullptr;
        bool m_rlMenuVisible = false;

        ~Fields() {
            m_profileTask.cancel();
            m_authTask.cancel();
        }
    };

    Ref<CCLayer> getStatsBase() { return static_cast<CCLayer*>(getChildren()->objectAtIndex(0)); }

    CCMenu* createStatEntry(char const* entryID,
                            char const* labelID,
                            std::string const& text,
                            char const* iconFrameOrPath,
                            SEL_MenuHandler iconCallback) {
        auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        label->setID(labelID);

        constexpr float kLabelScale = 0.60f;
        constexpr float kMaxLabelW = 58.f;
        constexpr float kMinScale = 0.20f;

        label->setScale(kLabelScale);
        label->limitLabelWidth(kMaxLabelW, kLabelScale, kMinScale);

        CCSprite* iconSprite = nullptr;
        if (iconFrameOrPath && iconFrameOrPath[0]) {
            // try sprite frame first
            iconSprite = CCSprite::createWithSpriteFrameName(iconFrameOrPath);
            // if that fails, try loading as a file path
            if (!iconSprite) {
                iconSprite = CCSprite::create(iconFrameOrPath);
            }
        }

        // final fallback to avoid null deref
        if (!iconSprite) {
            iconSprite = CCSprite::create();
        }

        iconSprite->setScale(0.8f);

        auto iconBtn = CCMenuItemSpriteExtra::create(iconSprite, this, iconCallback);
        if (!iconBtn) {
            // ensure we always have some valid child so layout code doesn't crash
            auto fallbackSpr = CCSprite::create();
            fallbackSpr->setVisible(false);
            iconBtn = CCMenuItemSpriteExtra::create(fallbackSpr, this, iconCallback);
        }

        if (!iconCallback) {
            iconBtn->setEnabled(false);
        }

        auto ls = label->getScaledContentSize();
        auto is = iconBtn->getScaledContentSize();

        constexpr float gap = 2.f;
        constexpr float pad = 2.f;

        float h = std::max(ls.height, is.height);
        float w = pad + ls.width + gap + is.width + pad;

        auto entry = CCMenu::create();
        entry->setID(entryID);
        entry->setContentSize({w, h});
        entry->setAnchorPoint({0.f, 0.5f});

        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({pad, h / 2.f});

        iconBtn->setAnchorPoint({0.f, 0.5f});
        iconBtn->setPosition({pad + ls.width + gap, h / 2.f});

        entry->addChild(label);
        entry->addChild(iconBtn);

        return entry;
    }

    // TODO: Update to use getStatsBase()
    void updateStatLabel(char const* labelID, std::string const& text) {
        auto rlStatsMenu = getChildByIDRecursive("rl-stats-menu");
        if (!rlStatsMenu) return;

        auto label = typeinfo_cast<CCLabelBMFont*>(rlStatsMenu->getChildByIDRecursive(labelID));
        if (!label) return;

        label->setString(text.c_str());

        constexpr float kLabelScale = 0.60f;
        constexpr float kMaxLabelW = 58.f;
        constexpr float kMinScale = 0.20f;
        label->limitLabelWidth(kMaxLabelW, kLabelScale, kMinScale);

        // Recompute parent entry size and reposition children so layout stays
        // correct
        if (auto entry = label->getParent()) {
            auto ls = label->getScaledContentSize();

            CCNode* iconBtn = nullptr;
            for (auto child : CCArrayExt<CCNode>(entry->getChildren())) {
                if (auto btn = typeinfo_cast<CCMenuItemSpriteExtra*>(child)) {
                    iconBtn = btn;
                    break;
                }
            }

            CCSize is = {0.f, 0.f};
            if (iconBtn) {
                is = iconBtn->getScaledContentSize();
            }

            constexpr float gap = 2.f;
            constexpr float pad = 2.f;

            float h = std::max(ls.height, is.height);
            float w = pad + ls.width + gap + is.width + pad;

            entry->setContentSize({w, h});

            label->setAnchorPoint({0.f, 0.5f});
            label->setPosition({pad, h / 2.f});

            if (iconBtn) {
                iconBtn->setAnchorPoint({0.f, 0.5f});
                iconBtn->setPosition({pad + ls.width + gap, h / 2.f});
            }
        }

        // Ensure the menu and surrounding layout are updated when data changes
        if (auto rlStatsMenu = getChildByIDRecursive("rl-stats-menu")) rlStatsMenu->updateLayout();
        if (auto rlButtonsMenu = getChildByIDRecursive("rl-buttons-menu")) rlButtonsMenu->updateLayout();
    }

    void loadPageFromUserInfo(GJUserScore* score) {
        // TODO: Restore previous state
        ProfilePage::loadPageFromUserInfo(score);

        // recreate the rl button menu every time we load a profile to ensure it's
        // in a clean state and
        if (m_fields->m_rlButtonBg != nullptr) {
            m_fields->m_rlButtonBg->removeFromParent();
            m_fields->m_arrowMenu->removeFromParent();
            m_fields->m_arrowMenu = nullptr;
            m_fields->m_rlButtonBg = nullptr;
            m_fields->m_rlMenuVisible = false;
            m_fields->m_rlStatsMenu->removeFromParent();
            m_fields->m_rlStatsMenu = nullptr;
        }

        if (!CachedSettings::get()->disableRLMenu) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            m_fields->m_rlButtonBg = NineSlice::create("GJ_square02.png");
            m_fields->m_rlButtonBg->setContentSize({42.f, 110.f});
            m_fields->m_rlButtonBg->setPosition(
                {winSize.width + m_fields->m_rlButtonBg->getContentSize().width + 8.f / 2.f, winSize.height / 2.f});
            m_fields->m_rlButtonBg->setID("rl-button-bg");

            m_fields->m_rlButtonsMenu = CCMenu::create();
            m_fields->m_rlButtonsMenu->setID("rl-buttons-menu");
            m_fields->m_rlButtonsMenu->setContentSize({32.f, 100.f});
            m_fields->m_rlButtonsMenu->setPosition({m_fields->m_rlButtonBg->getContentSize().width / 2.f,
                                                    m_fields->m_rlButtonBg->getContentSize().height / 2.f});
            m_fields->m_rlButtonsMenu->setLayout(
                ColumnLayout::create()->setGap(6.f)->setAxisAlignment(AxisAlignment::Center)->setGrowCrossAxis(false));

            m_mainLayer->addChild(m_fields->m_rlButtonBg, 20);
            m_fields->m_rlButtonBg->addChild(m_fields->m_rlButtonsMenu, 1);

            // create toggle arrow button

            auto arrowSpr = CCSprite::createWithSpriteFrameName("RL_arrow_01.png"_spr);
            auto arrowBtn = CCMenuItemSpriteExtra::create(arrowSpr, this, menu_selector(RLProfilePage::onToggleRLMenu));
            m_fields->m_rlToggleArrow = arrowBtn;
            m_fields->m_arrowMenu = CCMenu::create(arrowBtn, nullptr);
            m_fields->m_arrowMenu->setPosition({0, 0});
            m_fields->m_arrowMenu->setID("rl-toggle-menu");
            m_mainLayer->addChild(m_fields->m_arrowMenu, 20);
            // position at right edge
            float arrowW = arrowSpr->getContentSize().width;
            arrowBtn->setPosition({winSize.width - arrowW / 2 - 4, winSize.height / 2});

            // view stats
            if (GJAccountManager::sharedState()->m_accountID != 0) {
                auto rlViewSpr = CCSprite::createWithSpriteFrameName("RL_starBig.png"_spr);
                auto rlStatsSprOff =
                    EditorButtonSprite::create(rlViewSpr, EditorBaseColor::Gray, EditorBaseSize::Normal);
                auto rlStatsSprOn =
                    EditorButtonSprite::create(rlViewSpr, EditorBaseColor::LightBlue, EditorBaseSize::Normal);

                auto rlStatsBtn = CCMenuItemToggler::create(
                    rlStatsSprOff, rlStatsSprOn, this, menu_selector(RLProfilePage::onStatsSwitcher));
                rlStatsBtn->setID("rl-stats-btn");
                m_fields->m_rlButtonsMenu->addChild(rlStatsBtn);
            }

            // if u are leaderboard mod show the manage button to manage your
            // leaderboard entries
            if (!m_fields->m_rlButtonsMenu->getChildByID("rl-manage-btn")) {
                if (rl::isUserAdmin() || rl::isUserLeaderboardMod() || rl::isUserOwner()) {
                    auto modUserSpr = CCSprite::createWithSpriteFrameName("RL_userPanel.png"_spr);
                    auto modUserButton =
                        EditorButtonSprite::create(modUserSpr, EditorBaseColor::LightBlue, EditorBaseSize::Normal);
                    auto modUserBtnItem =
                        CCMenuItemSpriteExtra::create(modUserButton, this, menu_selector(RLProfilePage::onUserManage));
                    modUserBtnItem->setID("rl-manage-btn");
                    m_fields->m_rlButtonsMenu->addChild(modUserBtnItem);
                }
                if (rl::isUserLeaderboardRole() || rl::isUserOwner()) {
                    auto manageLevelSpr = CCSprite::createWithSpriteFrameName("RL_userHammer.png"_spr);
                    auto manageLevelButton =
                        EditorButtonSprite::create(manageLevelSpr, EditorBaseColor::LightBlue, EditorBaseSize::Normal);
                    auto manageLevelBtnItem = CCMenuItemSpriteExtra::create(
                        manageLevelButton, this, menu_selector(RLProfilePage::onUserManageLevel));
                    manageLevelBtnItem->setID("rl-manage-level-btn");
                    m_fields->m_rlButtonsMenu->addChild(manageLevelBtnItem);
                }
            }
        }

        auto statsMenu = m_mainLayer->getChildByID("stats-menu");
        if (!statsMenu) {
            log::warn("stats-menu not found");
            return;
        }

        m_fields->m_rlStatsMenu = CCMenu::create();
        m_fields->m_rlStatsMenu->setID("rl-stats-menu");

        auto row = RowLayout::create();
        row->setAxisAlignment(AxisAlignment::Center);
        row->setCrossAxisAlignment(AxisAlignment::Center);
        row->setGap(4.f);
        m_fields->m_rlStatsMenu->setLayout(row);

        auto starsText = GameToolbox::pointsToString(m_fields->stars);
        auto planetsText = GameToolbox::pointsToString(m_fields->planets);

        auto starsEntry = createStatEntry("rl-stars-entry",
                                          "rl-stars-label",
                                          starsText,
                                          "RL_starMed.png"_spr,
                                          menu_selector(RLProfilePage::onBlueprintStars));

        auto planetsEntry = createStatEntry("rl-planets-entry",
                                            "rl-planets-label",
                                            planetsText,
                                            "RL_planetMed.png"_spr,
                                            menu_selector(RLProfilePage::onPlanetsClicked));

        auto coinsEntry = createStatEntry("rl-coins-entry",
                                          "rl-coins-label",
                                          GameToolbox::pointsToString(m_fields->coins),
                                          "RL_BlueCoinSmall.png"_spr,
                                          nullptr);
        auto votesEntry = createStatEntry("rl-votes-entry",
                                          "rl-votes-label",
                                          GameToolbox::pointsToString(m_fields->votes),
                                          "RL_commVote01.png"_spr,
                                          nullptr);

        m_fields->m_rlStatsMenu->addChild(starsEntry);
        m_fields->m_rlStatsMenu->addChild(planetsEntry);
        m_fields->m_rlStatsMenu->addChild(coinsEntry);
        m_fields->m_rlStatsMenu->addChild(votesEntry);

        if (m_fields->points > 0) {
            auto pointsEntry = createStatEntry("rl-points-entry",
                                               "rl-points-label",
                                               GameToolbox::pointsToString(m_fields->points),
                                               "RL_blueprintPoint01.png"_spr,
                                               menu_selector(RLProfilePage::onLayoutPointsClicked));
            m_fields->m_rlStatsMenu->addChild(pointsEntry);
        }

        m_fields->m_rlStatsMenu->setAnchorPoint(statsMenu->getAnchorPoint());
        m_fields->m_rlStatsMenu->setPosition(statsMenu->getPosition());
        m_fields->m_rlStatsMenu->setContentSize(statsMenu->getContentSize());
        m_fields->m_rlStatsMenu->setScale(statsMenu->getScale());

        if (CachedSettings::mods()->better_progression) {  // me when hardcoding position because of this stupid mod be like:
            m_fields->m_rlStatsMenu->setPosition({309.5f, 248.0f});
            m_fields->m_rlStatsMenu->setScale(0.845f);
            m_fields->m_rlStatsMenu->setContentWidth(statsMenu->getContentSize().width - 100.f);
        }
        m_fields->m_rlStatsMenu->setVisible(false);
        statsMenu->setVisible(true);

        m_mainLayer->addChild(m_fields->m_rlStatsMenu);

        if (score) {
            this->fetchProfileData(score->m_accountID);
        }

        if (auto rlStatsMenu = getChildByIDRecursive("rl-stats-menu")) rlStatsMenu->updateLayout();

        statsMenu->updateLayout();
        if (m_fields->m_rlButtonsMenu) {
            m_fields->m_rlButtonsMenu->updateLayout();
        }
    }

    void onInfo(CCObject* sender) {
        if (m_fields->m_rlStatsMenu->isVisible()) {
            auto morePoints = m_fields->points > 0 ? fmt::format("\n<cf>Blueprint Points:</c> {}",
                                                                 GameToolbox::pointsToString(m_fields->points))
                                                   : "";
            auto statsInfo =
                fmt::format("<cl>Sparks: </c>{}\n<co>Planets:</c> {}\n<cb>Blue Coins:</c> {}\n<cg>Votes:</c> {}{}",
                            GameToolbox::pointsToString(m_fields->stars),
                            GameToolbox::pointsToString(m_fields->planets),
                            GameToolbox::pointsToString(m_fields->coins),
                            GameToolbox::pointsToString(m_fields->votes),
                            morePoints);

            FLAlertLayer::create(m_score->m_userName.c_str(), statsInfo, "OK")->show();
            return;
        }
        ProfilePage::onInfo(sender);
    }

    // rl hooks below
    void onStatsSwitcher(CCObject* sender) {
        auto* switcher = typeinfo_cast<CCMenuItemToggler*>(sender);
        auto statsBase = getStatsBase();
        if (!switcher || !statsBase) return;

        auto* statsMenu = statsBase->getChildByID("stats-menu");
        auto* rlStatsMenu = statsBase->getChildByID("rl-stats-menu");
        if (!switcher || !statsMenu || !rlStatsMenu) return;

        // Handle normal stats menu
        const bool isToggled = switcher->isToggled();
        statsMenu->setVisible(isToggled);
        if (auto m = typeinfo_cast<CCMenu*>(statsMenu)) m->setEnabled(isToggled);

        // Handle RL stats menu
        const bool isRLToggled = !isToggled;
        rlStatsMenu->setVisible(isRLToggled);
        if (auto m = typeinfo_cast<CCMenu*>(rlStatsMenu)) m->setEnabled(isRLToggled);

        auto* mainMenu = typeinfo_cast<CCMenu*>(statsBase->getChildByID("main-menu"));
        if (!mainMenu) return;
        auto* refreshButton = mainMenu->getChildByID("refresh-button");
        auto* infoButton = mainMenu->getChildByID("info-button");
        if (!refreshButton || !infoButton) return;

        // Change info button color
        if (auto* info = typeinfo_cast<CCRGBAProtocol*>(infoButton)) {
            if (isRLToggled)
                // info button blue
                info->setColor(ccColor3B{0, 180, 255});
            else
                // normal color
                info->setColor(ccColor3B{255, 255, 255});
        }
    }

    void onToggleRLMenu(CCObject* sender) {
        if (!m_fields->m_rlButtonBg) return;

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        float bgW = m_fields->m_rlButtonBg->getContentSize().width + 8.f;
        float arrowW = 0.f;
        if (m_fields->m_rlToggleArrow) {
            arrowW = m_fields->m_rlToggleArrow->getNormalImage()->getContentSize().width;
        }

        bool showing = m_fields->m_rlMenuVisible;
        float targetBgX = showing ? winSize.width + bgW / 2 : winSize.width - bgW / 2;
        float targetArrowX = showing ? (winSize.width - arrowW / 2 - 4) : (winSize.width - bgW - arrowW / 2 - 8);

        // exefm wants fast transition ig
        bool disableAnim = CachedSettings::get()->disableMenuAnimation;
        if (disableAnim) {
            // move instantly
            m_fields->m_rlButtonBg->setPosition({targetBgX, winSize.height / 2});
            if (CCMenuItemSpriteExtra* arrow = m_fields->m_rlToggleArrow) {
                arrow->setPosition({targetArrowX, winSize.height / 2});
                if (auto sprite = static_cast<CCSprite*>(arrow->getNormalImage())) {
                    sprite->setFlipX(!showing);
                }
            }
        } else {
            auto moveBg = CCEaseBackOut::create(CCMoveTo::create(0.3f, {targetBgX, winSize.height / 2}));
            m_fields->m_rlButtonBg->runAction(moveBg);
            if (CCMenuItemSpriteExtra* arrow = m_fields->m_rlToggleArrow) {
                auto moveArrow = CCEaseBackOut::create(CCMoveTo::create(0.3f, {targetArrowX, winSize.height / 2}));
                arrow->runAction(moveArrow);
                if (auto sprite = static_cast<CCSprite*>(arrow->getNormalImage())) {
                    sprite->setFlipX(!showing);
                }
            }
        }
        m_fields->m_rlMenuVisible = !showing;
    }

    void fetchProfileData(RLUserId accountId) {
        if (m_fields->m_authTask.isPending()) {
            if (m_fields->accountId == accountId) {
                // Avoid repeating a bunch of work lol
                return;
            }
            m_fields->m_authTask.cancel();
        }

        log::info("Fetching profile data for account ID: {}", accountId);
        m_fields->accountId = accountId;

        if (accountId == rl::ARCTICWOOF_ACCOUNT_ID) {
            RLAchievements::onReward("misc_arcticwoof");  // bub
        }
        if (RLArgon::hasToken()) {
            return continueProfileFetch(accountId);
        }

        m_fields->m_authTask.spawn(RLArgon::waitAsync(), [self = WeakRef(this), accountId]() {
            if (RLArgon::notifyFailed()) return;
            if (auto lockedSelf = self.lock()) {
                return lockedSelf->continueProfileFetch(accountId);
            }

            /*
            auto err = res.unwrapErr();
            log::warn("Auth failed: {}", err);

            if (err.find("Invalid account data") == std::string::npos) {
                Notification::create(err, NotificationIcon::Error)->show();
                argon::clearToken();
                return;
            }

            // Account data invalid, interactive auth fallback
            log::info("Falling back to interactive auth due to invalid account data");
            argon::AuthOptions options;
            options.progress = [](argon::AuthProgress progress) {
                log::debug("auth progress: {}",
                           argon::authProgressToString(progress));
            };

            m_fields->m_authTask.spawn(
                argon::startAuth(std::move(options)),
                [this, accountId](Result<std::string> res2) {
                    if (res2.isOk()) {
                        auto token = std::move(res2).unwrap();
                        log::debug("token obtained (fallback): {}", token);
                        Mod::get()->setSavedValue("argon_token", token);
                        this->continueProfileFetch(accountId);
                    } else {
                        log::warn("Interactive auth also failed: {}",
                                  res2.unwrapErr());
                        Notification::create(res2.unwrapErr(),
                                             NotificationIcon::Error)
                            ->show();
                        argon::clearToken();
                    }
                });
            */
        });
    }

    // TODO: Make return arc::Future<>?
    void continueProfileFetch(RLUserId accountId) {
        if (m_fields->m_profileInFlight && m_fields->m_profileForAccount == accountId) {
            log::debug("Profile request already in-flight for account {}", accountId);
            return;
        }

        m_fields->m_profileTask.cancel();
        m_fields->m_profileInFlight = true;
        m_fields->m_profileForAccount = accountId;

        Ref<RLProfilePage> pageRef = this;
        m_fields->m_profileTask.spawn(RLUserInfo::get(accountId), [pageRef, accountId](Result<RLUserInfo> infoOrErr) {
            if (!pageRef) return;
            pageRef->m_fields->m_profileInFlight = false;

            if (infoOrErr.isErr()) {
                log::warn("Profile load failed: {}", infoOrErr.unwrapErr());
                // FIXME: Only do this with a non ok server response!
                if (auto* toggleArrow = pageRef->m_fields->m_rlToggleArrow) {
                    toggleArrow->setEnabled(false);
                    toggleArrow->setOpacity(100);
                }
                return;
            }

            RLUserInfo info = std::move(infoOrErr).unwrap();
            pageRef->m_fields->init(info);

            // create the user buttons manage
            if (!CachedSettings::get()->disableRLMenu) {
                auto rlButtonsMenu = pageRef->getChildByIDRecursive("rl-buttons-menu");
                if (rlButtonsMenu &&
                    (rl::isUserAdmin() || rl::isUserLeaderboardMod() || rl::isUserOwner() || rl::isUserDeveloper())) {
                    if (!rlButtonsMenu->getChildByID("rl-manage-btn")) {
                        auto modUserSpr = CCSprite::createWithSpriteFrameName("RL_userPanel.png"_spr);
                        auto modUserButton =
                            EditorButtonSprite::create(modUserSpr, EditorBaseColor::LightBlue, EditorBaseSize::Normal);
                        auto modUserBtnItem = CCMenuItemSpriteExtra::create(
                            modUserButton, pageRef, menu_selector(RLProfilePage::onUserManage));
                        modUserBtnItem->setID("rl-manage-btn");
                        rlButtonsMenu->addChild(modUserBtnItem);
                        rlButtonsMenu->updateLayout();
                    }

                    if (rlButtonsMenu && (rl::isUserLeaderboardAdmin() || rl::isUserLeaderboardMod() ||
                                          rl::isUserOwner() || rl::isUserDeveloper())) {
                        if (!rlButtonsMenu->getChildByID("rl-manage-level-btn")) {
                            auto manageLevelSpr = CCSprite::createWithSpriteFrameName("RL_userHammer.png"_spr);
                            auto manageLevelButton = EditorButtonSprite::create(
                                manageLevelSpr, EditorBaseColor::LightBlue, EditorBaseSize::Normal);
                            auto manageLevelBtnItem = CCMenuItemSpriteExtra::create(
                                manageLevelButton, pageRef, menu_selector(RLProfilePage::onUserManageLevel));
                            manageLevelBtnItem->setID("rl-manage-level-btn");
                            rlButtonsMenu->addChild(manageLevelBtnItem);
                            rlButtonsMenu->updateLayout();
                        }
                    }
                }
            }

            rl::setupFallbackBadgesImpl(pageRef, info, "profile");

            pageRef->updateStatLabel("rl-stars-label", GameToolbox::pointsToString(info.stars));
            pageRef->updateStatLabel("rl-planets-label", GameToolbox::pointsToString(info.planets));
            pageRef->updateStatLabel("rl-coins-label", GameToolbox::pointsToString(info.coins));
            pageRef->updateStatLabel("rl-votes-label", GameToolbox::pointsToString(info.votes));

            // If this is the player's own profile, check achievements for Sparks
            // and Planets
            if (pageRef->m_ownProfile) {
                log::debug("checking Sparks/Planets achievements (stars={}, planets={})", info.stars, info.planets);
                RLAchievements::checkAll(RLAchievements::Collectable::Sparks, info.stars);
                RLAchievements::checkAll(RLAchievements::Collectable::Planets, info.planets);
                RLAchievements::checkAll(RLAchievements::Collectable::Points, info.points);
                RLAchievements::checkAll(RLAchievements::Collectable::Coins, info.coins);
                RLAchievements::checkAll(RLAchievements::Collectable::Votes, info.votes);
            }

            // Handle creator points
            if (auto rlStatsMenu = pageRef->getChildByIDRecursive("rl-stats-menu")) {
                if (pageRef->m_fields->points > 0 && !CachedSettings::get()->disableCreatorPoints) {
                    if (!rlStatsMenu->getChildByIDRecursive("rl-points-entry")) {
                        auto pointsEntry =
                            pageRef->createStatEntry("rl-points-entry",
                                                     "rl-points-label",
                                                     GameToolbox::pointsToString(info.points),
                                                     "RL_blueprintPoint01.png"_spr,
                                                     menu_selector(RLProfilePage::onLayoutPointsClicked));
                        rlStatsMenu->addChild(pointsEntry);
                    } else {
                        pageRef->updateStatLabel("rl-points-label", GameToolbox::pointsToString(info.points));
                    }
                } else {
                    if (auto creatorPoint = rlStatsMenu->getChildByIDRecursive("rl-points-entry")) {
                        creatorPoint->removeFromParent();
                    }
                }

                rlStatsMenu->updateLayout();
            }
        });
    }

    void onBadgeClicked(CCObject* sender) {
        if (auto* btn = static_cast<CCMenuItemSpriteExtra*>(sender)) rl::showRoleInfoPopup(btn->getTag());
    }

    void onUserManage(CCObject* sender) {
        // only leaderboard moderators may manage users
        if (!(rl::isUserHasPerms() || rl::isUserOwner() || rl::isUserDeveloper())) {
            Notification::create("You don't have permission to manage users.", NotificationIcon::Error)->show();
            return;
        }
        int accountId = m_fields->accountId;
        auto userControl = RLUserControl::create(accountId);
        userControl->show();
    }

    void onUserManageLevel(CCObject* sender) {
        // only leaderboard moderators may manage levels
        if (!(rl::isUserLeaderboardMod() || rl::isUserLeaderboardAdmin() || rl::isUserOwner() ||
              rl::isUserDeveloper())) {
            Notification::create("You don't have permission to manage levels.", NotificationIcon::Error)->show();
            return;
        }
        int accountId = m_fields->accountId;
        auto levelControl = RLUserLevelControl::create(accountId);
        levelControl->show();
    }

    void onPlanetsClicked(CCObject* sender) {
        int accountId = m_fields->accountId;
        auto popup = RLDifficultyTotalPopup::create(accountId, RLDifficultyTotalPopup::Mode::Planets);
        if (popup) popup->show();
    }

    void onLayoutPointsClicked(CCObject* sender) {
        int accountId = m_fields->accountId;
        auto username = GameLevelManager::sharedState()->tryGetUsername(accountId);
        if (username.empty()) username = "User";
        std::string title = fmt::format("{}'s Layouts", username);

        RLLevelBrowserLayer::ParamList params;
        params.emplace_back("accountId", numToString(accountId));
        auto browserLayer = RLLevelBrowserLayer::create(RLLevelBrowserLayer::Mode::Account, params, title);
        auto scene = CCScene::create();
        scene->addChild(browserLayer);
        auto transitionFade = CCTransitionFade::create(0.5f, scene);
        CCDirector::sharedDirector()->pushScene(transitionFade);
    }

    void onBlueprintStars(CCObject* sender) {
        int accountId = m_fields->accountId;
        auto popup = RLDifficultyTotalPopup::create(accountId, RLDifficultyTotalPopup::Mode::Stars);
        if (popup) popup->show();
    }
};
