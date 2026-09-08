#include "layer/RLLeaderboardLayer.hpp"
#include "RLAchievements.hpp"
#include "RLLayerBackground.hpp"
#include <Geode/binding/CCSpriteGrayscale.hpp>
#include <cue/RepeatingBackground.hpp>
#include "RLConstants.hpp"
#include "RLNetworkUtils.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/LazyNameplate.hpp"
#include "utils/RLArgon.hpp"
#include "utils/ScopeExit.hpp"

using namespace rl;

namespace {
enum {
    kStaggerAmount = 10,
};
}  // namespace

bool RLLeaderboardLayer::init() {
    if (!CCLayer::init()) return false;
    m_currentAccountID = GJAccountManager::sharedState()->m_accountID;

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    // create if moving bg disabled
    rl::addLayerBackground(this);

    addSideArt(this, SideArt::All, SideArtStyle::Layer, false);

    auto backMenu = CCMenu::create();
    backMenu->setPosition({0, 0});

    addBackButton(this, BackButtonStyle::Pink);
    auto const listWidth = 356.f;
    auto const listHeight = 220.f;

    m_userListNode = cue::ListNode::create(
        {listWidth, listHeight}, {191, 114, 62, 255}, cue::ListBorderStyle::SlimLevels);
    m_userListNode->setAnchorPoint({0.5f, 0.5f});
    m_userListNode->setPosition({winSize.width / 2 - 5, winSize.height / 2 - 5.f});
    m_userListNode->setParent(this);
    m_userListNode->setAutoUpdate(false);
    this->addChild(m_userListNode, 5);
    m_scrollLayer = m_userListNode->getScrollLayer();

    if (!CachedSettings::get()->disableScrollbar) {
        auto scrollBar = Scrollbar::create(m_scrollLayer);
        scrollBar->setPosition({m_userListNode->getContentSize().width + 24.f,
                                m_userListNode->getContentSize().height / 2});
        //scrollBar->setContentHeight(m_userListNode->getContentSize().height - 20);
        m_userListNode->addChild(scrollBar, 10);
    }

    this->addSpinner();

    auto typeMenu = CCMenu::create();
    typeMenu->setPosition({0, -2});
    typeMenu->setContentSize(m_userListNode->getContentSize());

    auto starsTab = TabButton::create(TabBaseColor::Unselected,
                                      TabBaseColor::UnselectedDark,
                                      "Top Sparks",
                                      this,
                                      menu_selector(RLLeaderboardLayer::onLeaderboardTypeButton));
    float centerX = m_userListNode->getContentSize().width / 2.f;
    const float tabY = 247.f;
    const float spacing = 80.f;

    starsTab->setTag(1);
    starsTab->toggle(true);
    starsTab->setScale(0.8f);
    starsTab->setPosition({centerX - 2.f * spacing, tabY});
    typeMenu->addChild(starsTab);
    m_starsTab = starsTab;

    auto planetsTab = TabButton::create(TabBaseColor::Unselected,
                                        TabBaseColor::UnselectedDark,
                                        "Top Planets",
                                        this,
                                        menu_selector(RLLeaderboardLayer::onLeaderboardTypeButton));
    planetsTab->setTag(3);
    planetsTab->toggle(false);
    planetsTab->setScale(0.8f);
    planetsTab->setPosition({centerX - 1.f * spacing, tabY});
    typeMenu->addChild(planetsTab);
    m_planetsTab = planetsTab;

    auto creatorTab = TabButton::create(TabBaseColor::Unselected,
                                        TabBaseColor::UnselectedDark,
                                        "Top Creator",
                                        this,
                                        menu_selector(RLLeaderboardLayer::onLeaderboardTypeButton));
    creatorTab->setTag(2);
    creatorTab->toggle(false);
    creatorTab->setScale(0.8f);
    creatorTab->setPosition({centerX, tabY});
    typeMenu->addChild(creatorTab);
    m_creatorTab = creatorTab;

    // Top Coins tab (type 4)
    auto coinsTab = TabButton::create(TabBaseColor::Unselected,
                                      TabBaseColor::UnselectedDark,
                                      "Top Coins",
                                      this,
                                      menu_selector(RLLeaderboardLayer::onLeaderboardTypeButton));
    coinsTab->setTag(4);
    coinsTab->toggle(false);
    coinsTab->setScale(0.8f);
    coinsTab->setPosition({centerX + 1.f * spacing, tabY});
    typeMenu->addChild(coinsTab);
    m_coinsTab = coinsTab;

    auto votesTab = TabButton::create(TabBaseColor::Unselected,
                                      TabBaseColor::UnselectedDark,
                                      "Top Votes",
                                      this,
                                      menu_selector(RLLeaderboardLayer::onLeaderboardTypeButton));
    votesTab->setTag(5);
    votesTab->toggle(false);
    votesTab->setScale(0.8f);
    votesTab->setPosition({centerX + 2.f * spacing, tabY});
    typeMenu->addChild(votesTab);
    m_votesTab = votesTab;

    if (CachedSettings::get()->disableCreatorPoints == true) {
        if (m_creatorTab) {
            m_creatorTab->setEnabled(false);
            m_creatorTab->setVisible(false);
        }
    }

    m_userListNode->addChild(typeMenu);

    // info button at the bottom left
    auto infoMenu = CCMenu::create();
    infoMenu->setPosition({0, 0});
    auto infoButtonSpr = CCSprite::createWithSpriteFrameName("RL_info01.png"_spr);
    infoButtonSpr->setScale(0.7f);
    auto infoButton = CCMenuItemSpriteExtra::create(
        infoButtonSpr, this, menu_selector(RLLeaderboardLayer::onInfoButton));
    infoButton->setPosition({25, 25});
    infoMenu->addChild(infoButton);
    this->addChild(infoMenu);

    // refresh button at the bottom right
    auto refreshSpr = CCSprite::createWithSpriteFrameName("RL_refresh01.png"_spr);
    m_refreshBtn = CCMenuItemSpriteExtra::create(
        refreshSpr, this, menu_selector(RLLeaderboardLayer::onRefreshButton));
    m_refreshBtn->setPosition({winSize.width - 35, 35});
    infoMenu->addChild(m_refreshBtn);

    // account info refresh button above the refresh button
    auto accountInfoSpr = CCSprite::createWithSpriteFrameName("RL_refresh02.png"_spr);
    accountInfoSpr->setScale(0.7f);
    m_accountRefreshBtn = CCMenuItemSpriteExtra::create(
        accountInfoSpr, this, menu_selector(RLLeaderboardLayer::onAccountRefreshButton));
    m_accountRefreshBtn->setPosition({winSize.width - 35, 85});
    infoMenu->addChild(m_accountRefreshBtn);

    auto creatorTypeIcon =
        CCSpriteGrayscale::createWithSpriteFrameName("RL_blueprintPoint01.png"_spr);
    if (creatorTypeIcon && !CachedSettings::get()->disableCreatorPointsToggle) {
        auto creatorTypeOff = EditorButtonSprite::create(
            CCSpriteGrayscale::createWithSpriteFrameName("RL_blueprintPoint01.png"_spr),
            EditorBaseColor::Gray,
            EditorBaseSize::Normal);
        auto creatorTypeOn = EditorButtonSprite::create(
            creatorTypeIcon, EditorBaseColor::LightBlue, EditorBaseSize::Normal);
        auto creatorTypeBtn = CCMenuItemSpriteExtra::create(
            creatorTypeOff, this, menu_selector(RLLeaderboardLayer::onCreatorTypeToggle));
        creatorTypeBtn->setPosition(
            {infoButton->getPosition().x, infoButton->getPosition().y + 40});
        creatorTypeBtn->setVisible(false);
        creatorTypeBtn->setEnabled(false);
        infoMenu->addChild(creatorTypeBtn);
        m_creatorTypeToggleBtn = creatorTypeBtn;
    }

    this->fetchLeaderboard(1);
    this->scheduleUpdate();
    this->setKeypadEnabled(true);

    return true;
}

void RLLeaderboardLayer::addSpinner(bool updateLayout) {
    this->removeSpinner();
    m_spinner = LoadingSpinner::create(100.f);
    m_spinner->setPosition(m_userListNode->getContentSize() / 2);
    if (m_userListNode) {
        m_userListNode->addChild(m_spinner);
        if (updateLayout) m_userListNode->updateLayout();
    }
}
void RLLeaderboardLayer::removeSpinner(bool updateLayout) {
    if (m_spinner) {
        m_spinner->removeFromParent();
        m_spinner = nullptr;
        if (m_userListNode && updateLayout) m_userListNode->updateLayout();
    }
}
void RLLeaderboardLayer::refresh(CCObject* sender, int type) {
    if (m_userListNode) {
        // clear the list and show a spinner
        m_userListNode->clear();
        //m_userListNode->updateLayout();
        this->addSpinner(true);
    }
    /*
    if (m_scrollLayer && m_scrollLayer->m_contentLayer) {
        // clear the list and show a spinner
        if (m_userListNode) {
            m_userListNode->clear();
            //m_userListNode->updateLayout();
        }
        this->addSpinner(true);
    }
    */

    this->fetchLeaderboard(type);
}

void RLLeaderboardLayer::onInfoButton(CCObject* sender) {
    MDPopup::create("Rated Layouts Leaderboard",
                    "The leaderboard shows the top players in <cb>Rated Layouts</c> based "
                    "on <cl>Sparks</c>, <co>Planets</c>, <cb>Blue Coins</c>, <cf>Blueprint "
                    "Points</c> and <cg>Votes</c>. "
                    "You can "
                    "view each category by selecting the tabs.\n\n"
                    "- <cl>Sparks</c> are earned by completing a <cb>Classic Rated Layouts</c> "
                    "level and are only "
                    "counted when "
                    "beaten legitimately.\n"
                    "- <co>Planets</c> are earned by completing a <cb>Platformer Rated Layouts</c> "
                    "level and are only "
                    "counted when "
                    "beaten legitimately.\n"
                    "- <cb>Blue Coins</c> are earned by collecting them while playing in Rated "
                    "Layouts levels.\n"
                    "- <cf>Blueprint Points</c> are earned based on how many rated layout levels "
                    "you have in your "
                    "account, and "
                    "users who are excluded "
                    "won't be affected by this leaderboard.\n\n"
                    "You can toggle between those who interacted with the <cl>Rated Layouts "
                    "Mod</c> and those who never "
                    "use the "
                    "mod in the <cb>Top Creator</c> tab.\n\n"
                    "Getting a <cs>Rated</c> layout earns you 1 point, <cg>Featured</c> levels "
                    "earn you 2 points, "
                    "<cp>Epic</c> "
                    "levels earn you 3 points, and <cd>Legendary</c> levels earn you 4 points.\n\n"
                    "- <cg>Votes</c> are earned by voting in the <cb>Community Votes</c>. Each "
                    "vote is earned per "
                    "level.\n\n"
                    "### Any <cr>unfair</c> means of obtaining these stats <cy>(eg. instant "
                    "complete, noclipping, secret "
                    "way)</c> "
                    "will result in an <cr>exclusion from the leaderboard and there will be NO "
                    "APPEALS!</c> Each "
                    "completion is "
                    "<co>publicly logged</c> for this purpose.\n\n",
                    "OK")
        ->show();
}

void RLLeaderboardLayer::onAccountClicked(CCObject* sender) {
    auto button = static_cast<CCMenuItem*>(sender);
    int accountId = button->getTag();
    ProfilePage::create(accountId, false)->show();
}

void RLLeaderboardLayer::onAccountRefreshButton(CCObject* sender) {
    createQuickPopup(
        "Update Account Info",
        "Are you sure you want to <cg>update your account information</c> to <cl>Rated "
        "Layouts</c>?\n<cy>Only use this "
        "if you changed your username or icons recently and need to show the updated "
        "information.</c>",
        "No",
        "Yes",
        [this](FLAlertLayer*, bool yes) {
        if (!yes) return;

        auto* upopup = UploadActionPopup::create(nullptr, "Updating Account...");
        Ref<UploadActionPopup> popupRef = upopup;
        upopup->show();

        if (GJAccountManager::get()->m_accountID == 0) {
            upopup->showFailMessage("You are not logged in.");
            return;
        }

        auto token = RLArgon::token();
        if (token.empty()) {
            upopup->showFailMessage("Argon token missing.");
            return;
        }

        matjson::Value jsonBody = matjson::Value::object();
        jsonBody["accountId"] = GJAccountManager::get()->m_accountID;
        jsonBody["argonToken"] = token;

        auto req = web::WebRequest();
        req.bodyJSON(jsonBody);

        Ref<RLLeaderboardLayer> self = this;
        async::spawn(req.post(std::string(rl::BASE_API_URL) + "/resetAccountInfo"),
                     [self, popupRef](web::WebResponse res) {
            if (!popupRef) return;
            if (!res.ok()) {
                std::string err = rl::getResponseFailMessage(res, "Failed to update account.");
                popupRef->showFailMessage(err);
                return;
            }
            auto jsonRes = res.json();
            if (!jsonRes) {
                std::string err = rl::getResponseFailMessage(res, "Failed to update account.");
                popupRef->showFailMessage(err);
                return;
            }
            auto json = jsonRes.unwrap();
            bool success = json["success"].asBool().unwrapOr(false);
            if (success) {
                popupRef->showSuccessMessage("Account updated successfully.");
                self->onRefreshButton(nullptr);
            } else {
                std::string message = rl::getResponseFailMessage(res, "Failed to update account.");
                popupRef->showFailMessage(message);
            }
        });
    });
}

void RLLeaderboardLayer::onRefreshButton(CCObject* sender) {
    if (m_refreshFn) {
        m_refreshFn();
        return;
    }

    // determine active tab type
    int type = 1;
    if (m_starsTab && m_starsTab->isToggled()) {
        type = 1;
    } else if (m_planetsTab && m_planetsTab->isToggled()) {
        type = 3;
    } else if (m_creatorTab && m_creatorTab->isToggled()) {
        type = m_creatorType6 ? 6 : 2;
    } else if (m_coinsTab && m_coinsTab->isToggled()) {
        type = 4;
    } else if (m_votesTab && m_votesTab->isToggled()) {
        type = 5;
    }

    this->refresh(sender, type);
}

void RLLeaderboardLayer::onLeaderboardTypeButton(CCObject* sender) {
    auto button = static_cast<TabButton*>(sender);
    int type = button->getTag();

    if (type == 1 && !m_starsTab->isToggled()) {
        m_starsTab->toggle(true);
        m_planetsTab->toggle(false);
        m_creatorTab->toggle(false);
        m_coinsTab->toggle(false);
        m_votesTab->toggle(false);
    } else if (type == 3 && !m_planetsTab->isToggled()) {
        m_starsTab->toggle(false);
        m_planetsTab->toggle(true);
        m_creatorTab->toggle(false);
        m_coinsTab->toggle(false);
        m_votesTab->toggle(false);
    } else if (type == 2 && !m_creatorTab->isToggled()) {
        m_starsTab->toggle(false);
        m_planetsTab->toggle(false);
        m_creatorTab->toggle(true);
        m_coinsTab->toggle(false);
        m_votesTab->toggle(false);
    } else if (type == 4 && m_coinsTab && !m_coinsTab->isToggled()) {
        m_starsTab->toggle(false);
        m_planetsTab->toggle(false);
        m_creatorTab->toggle(false);
        m_coinsTab->toggle(true);
        m_votesTab->toggle(false);
    } else if (type == 5 && m_votesTab && !m_votesTab->isToggled()) {
        m_starsTab->toggle(false);
        m_planetsTab->toggle(false);
        m_creatorTab->toggle(false);
        m_coinsTab->toggle(false);
        m_votesTab->toggle(true);
    }

    if (type == 2 && m_creatorTab && m_creatorTab->isToggled() && m_creatorType6) {
        type = 6;
    }

    if (m_creatorTypeToggleBtn) {
        bool creatorVisible = m_creatorTab && m_creatorTab->isToggled();
        m_creatorTypeToggleBtn->setVisible(creatorVisible);
        m_creatorTypeToggleBtn->setEnabled(creatorVisible);
        if (creatorVisible) {
            CCSprite* icon =
                m_creatorType6
                    ? CCSprite::createWithSpriteFrameName("RL_blueprintPoint01.png"_spr)
                    : CCSpriteGrayscale::createWithSpriteFrameName("RL_blueprintPoint01.png"_spr);
            if (icon) {
                auto creatorTypeSpr = EditorButtonSprite::create(
                    icon,
                    m_creatorType6 ? EditorBaseColor::LightBlue : EditorBaseColor::Gray,
                    EditorBaseSize::Normal);
                m_creatorTypeToggleBtn->setNormalImage(creatorTypeSpr);
            }
        }
    }

    if (type == 2 && m_creatorTab && m_creatorTab->isToggled() && m_creatorType6) {
        type = 6;
    }

    if (m_creatorTypeToggleBtn) {
        bool creatorVisible = m_creatorTab && m_creatorTab->isToggled();
        m_creatorTypeToggleBtn->setVisible(creatorVisible);
        m_creatorTypeToggleBtn->setEnabled(creatorVisible);
        if (creatorVisible) {
            CCSprite* icon =
                m_creatorType6
                    ? CCSprite::createWithSpriteFrameName("RL_blueprintPoint01.png"_spr)
                    : CCSpriteGrayscale::createWithSpriteFrameName("RL_blueprintPoint01.png"_spr);
            if (icon) {
                auto creatorTypeSpr = EditorButtonSprite::create(
                    icon,
                    m_creatorType6 ? EditorBaseColor::LightBlue : EditorBaseColor::Gray,
                    EditorBaseSize::Normal);
                m_creatorTypeToggleBtn->setNormalImage(creatorTypeSpr);
            }
        }
    }

    this->refresh(sender, type);
}

void RLLeaderboardLayer::onCreatorTypeToggle(CCObject* sender) {
    m_creatorType6 = !m_creatorType6;
    if (m_creatorTypeToggleBtn) {
        CCSprite* icon =
            m_creatorType6
                ? CCSprite::createWithSpriteFrameName("RL_blueprintPoint01.png"_spr)
                : CCSpriteGrayscale::createWithSpriteFrameName("RL_blueprintPoint01.png"_spr);
        if (icon) {
            auto creatorTypeSpr = EditorButtonSprite::create(
                icon,
                m_creatorType6 ? EditorBaseColor::LightBlue : EditorBaseColor::Gray,
                EditorBaseSize::Normal);
            m_creatorTypeToggleBtn->setNormalImage(creatorTypeSpr);
        }
    }
    this->onRefreshButton(sender);
}

void RLLeaderboardLayer::fetchLeaderboard(int type, int) {
    Ref<RLLeaderboardLayer> self = this;
    async::spawn(LocalEndpoint::build("getScore")
                     .param("type", type)
                     .nkparam("amount=100")
                     .expires(30_mins)
                     .verifySuccess("fetchLeaderboard")
                     .get(),
                 [self, type](Result<matjson::Value> res) {
        if (res.isErr()) {
            log::warn("{}", res.unwrapErr());
            Notification::create(res.unwrapErr(), NotificationIcon::Error)->show();
            return;
        }

        auto json = std::move(res).unwrap();
        if (json.contains("users") && json["users"].isArray()) {
            bool& alreadyFetched = self->m_alreadyFetched[type - 1];
            auto& users = json["users"].asArray().unwrap();
            if (CachedSettings::get()->enableExperimentalFeatures && !alreadyFetched)
                self->populateLeaderboardStaggered(std::move(users), kStaggerAmount);
            else {
                self->populateLeaderboard(users);
                if (self->m_scrollLayer) self->m_scrollLayer->scrollToTop();
            }
            alreadyFetched = true;
        } else {
            log::warn("No users array in response");
        }
    });
}

static std::span<matjson::Value> takeFront(std::span<matjson::Value> users, unsigned by) {
    if (users.size() > by) [[likely]] {
        return users.subspan(0, by);
    } else [[unlikely]] {
        return users;
    }
}
static std::span<matjson::Value> dropFront(std::span<matjson::Value> users, unsigned by) {
    if (users.size() > by) [[likely]] {
        return users.subspan(by);
    } else [[unlikely]] {
        return std::span<matjson::Value>();
    }
}

void RLLeaderboardLayer::populateLeaderboardStaggered(std::vector<matjson::Value> users,
                                                      unsigned by) {
    static constexpr unsigned kFirstStagger = 5;
    this->setUpdates(false);
    bool firstRun = populateLeaderboardImpl<true>(takeFront(users, kFirstStagger));
    if (!firstRun) {
        this->setUpdates(true);
        return;
    }

    WeakRef<RLLeaderboardLayer> self = this;
    async::spawn([self = std::move(self), users = std::move(users), by]() mutable -> arc::Future<> {
        auto usersLeft = dropFront(users, kFirstStagger);
        int rank = 1 + int(kFirstStagger);
        arc::Notify notify;
        bool lastResult = true;

        while (!usersLeft.empty() && lastResult) {
            if (!self.valid()) co_return;
            auto currUsers = takeFront(usersLeft, by);
            Loader::get()->queueInMainThread([self, &lastResult, currUsers, rank, notify]() {
                if (auto lb = self.lock()) {
                    lastResult = lb->populateLeaderboardImpl<false>(currUsers, rank);
                    if (auto* scrollLayer = lb->m_scrollLayer) scrollLayer->scrollToTop();
                } else
                    lastResult = false;
                notify.notifyOne(true);
            });
            // TODO: Use arc::select?
            co_await notify.notified();
            usersLeft = dropFront(usersLeft, by);
            rank += int(by);
        }

        Loader::get()->queueInMainThread([self = std::move(self)]() {
            if (auto lb = self.lock())
                // DONT TOUCH!! If we call this more than once it messes up ordering
                lb->setUpdates(true);
        });
        co_return;
    });
}

bool RLLeaderboardLayer::populateLeaderboard(std::span<matjson::Value> users, int rank) {
    this->setUpdates(false);
    bool out = this->populateLeaderboardImpl<true>(users, rank);
    this->setUpdates(true);
    return out;
}

template <bool ClearElts>
inline bool RLLeaderboardLayer::populateLeaderboardImpl(std::span<matjson::Value> users, int rank) {
    if (!m_userListNode || !m_scrollLayer) return false;

    auto contentLayer = m_scrollLayer->m_contentLayer;
    if (!contentLayer) return false;

    if constexpr (ClearElts) {
        this->removeSpinner();
        if (m_userListNode) m_userListNode->clear();
    }

    const char* iconName = [this]() {
        if (m_starsTab && m_starsTab->isToggled())
            return "RL_starMed.png"_spr;
        else if (m_planetsTab && m_planetsTab->isToggled())
            return "RL_planetMed.png"_spr;
        else if (m_coinsTab && m_coinsTab->isToggled())
            return "RL_BlueCoinSmall.png"_spr;
        else if (m_votesTab && m_votesTab->isToggled())
            return "RL_commVote01.png"_spr;
        else
            return "RL_blueprintPoint01.png"_spr;
    }();

    constexpr float kRowSizeX = 356.f;
    constexpr float kRowSizeY = 40.f;

    auto* gm = GameManager::sharedState();
    for (const auto& userValue : users) {
        if (!userValue.isObject()) continue;

        int accountId = userValue["accountId"].asInt().unwrapOr(-1);
        if (accountId == -1) continue;

        int score = userValue["score"].asInt().unwrapOrDefault();
        int nameplateId = userValue["nameplate"].asInt().unwrapOr(0);

        const CCSize content = {kRowSizeX, kRowSizeY};
        auto* rowContainer = CCLayer::create();
        rowContainer->setContentSize(content);
        //rowContainer->setPosition(kRowSizeX / 2.f, yPosition);
        //rowContainer->setZOrder(rank - 1);

        CCSprite* bgSprite = CCSprite::create();
        bgSprite->setTextureRect(CCRectMake(0, 0, kRowSizeX, kRowSizeY));
        if (accountId == m_currentAccountID) {
            bgSprite->setColor({230, 150, 10});
        } else if (rank % 2 == 1) {
            bgSprite->setColor({161, 88, 44});
        } else {
            bgSprite->setColor({194, 114, 62});
        }
        bgSprite->setPosition({178.f, 20.f});
        rowContainer->addChild(bgSprite, 0);

        if (nameplateId != 0 && !CachedSettings::get()->disableNameplate) {
            auto* lazy = LazyNameplate::create(
                {bgSprite->getScaledContentSize() + CCSize(25, 25)}, nameplateId, false);
            lazy->setAutoResize(true);
            lazy->setPosition({bgSprite->getPositionX(), bgSprite->getPositionY()});
            bgSprite->setOpacity(50);
            rowContainer->addChild(lazy, -1);
        }

        // glow for top 3
        if (rank == 1) {
            auto glow = CCLayerGradient::create({255, 215, 0, 255}, {255, 140, 0, 0}, {1.f, 1.f});
            glow->changeWidthAndHeight(kRowSizeX, kRowSizeY);
            rowContainer->addChild(glow, 1);
        } else if (rank == 2) {
            auto glow =
                CCLayerGradient::create({192, 192, 192, 255}, {128, 128, 128, 0}, {1.f, 1.f});
            glow->changeWidthAndHeight(kRowSizeX, kRowSizeY);
            rowContainer->addChild(glow, 1);
        } else if (rank == 3) {
            auto glow = CCLayerGradient::create({205, 127, 50, 255}, {139, 69, 19, 0}, {1.f, 1.f});
            glow->changeWidthAndHeight(kRowSizeX, kRowSizeY);
            rowContainer->addChild(glow, 1);
        } else if (accountId == m_currentAccountID) {
            auto glow = CCLayerGradient::create({0, 255, 0, 255}, {0, 255, 255, 0}, {1.f, 1.f});
            glow->changeWidthAndHeight(kRowSizeX, kRowSizeY);
            rowContainer->addChild(glow, 1);
        }

        // Rank label
        auto rankStr = geode::utils::numToString(rank);
        auto* rankLabel = CCLabelBMFont::create(rankStr.c_str(), "goldFont.fnt");
        rankLabel->setScale(0.5f);
        rankLabel->setPosition({15.f, 20.f});
        rankLabel->setAnchorPoint({0.f, 0.5f});
        rowContainer->addChild(rankLabel, 2);

        if (accountId == m_currentAccountID) {
            RLAchievements::onReward("misc_leaderboard");  // gg
            rankLabel->setColor({0, 255, 255});
        }

        auto username = userValue["username"].asString().unwrapOrDefault();
        auto accountLabel = CCLabelBMFont::create(username.c_str(), "goldFont.fnt");
        accountLabel->setAnchorPoint({0.f, 0.5f});
        accountLabel->setScale(0.7f);

        int iconId = userValue["iconid"].asInt().unwrapOrDefault();
        int color1 = userValue["color1"].asInt().unwrapOrDefault();
        int color2 = userValue["color2"].asInt().unwrapOrDefault();
        int color3 = userValue["color3"].asInt().unwrapOrDefault();

        auto* player = SimplePlayer::create(iconId);
        player->updatePlayerFrame(iconId, IconType::Cube);
        player->setColors(gm->colorForIdx(color1), gm->colorForIdx(color2));
        if (color3 != 0) {  // no color 3? no glow
            player->setGlowOutline(gm->colorForIdx(color3));
        }
        player->setPosition({55.f, 20.f});
        player->setScale(0.75f);
        rowContainer->addChild(player, 2);

        auto buttonMenu = CCMenu::create();
        buttonMenu->setPosition({0, 0});

        auto accountButton = CCMenuItemSpriteExtra::create(
            accountLabel, this, menu_selector(RLLeaderboardLayer::onAccountClicked));
        accountButton->setTag(accountId);
        accountButton->setPosition({80.f, 20.f});
        accountButton->setAnchorPoint({0.f, 0.5f});

        buttonMenu->addChild(accountButton);
        rowContainer->addChild(buttonMenu, 2);

        auto scoreLabelText =
            CCLabelBMFont::create(GameToolbox::pointsToString(score).c_str(), "bigFont.fnt");
        scoreLabelText->setScale(0.5f);
        scoreLabelText->setPosition({320.f, 20.f});
        scoreLabelText->setAnchorPoint({1.f, 0.5f});
        rowContainer->addChild(scoreLabelText, 2);

        auto* iconSprite = CCSprite::createWithSpriteFrameName(iconName);
        iconSprite->setScale(0.65f);
        iconSprite->setPosition({325.f, 20.f});
        iconSprite->setAnchorPoint({0.f, 0.5f});
        rowContainer->addChild(iconSprite, 2);

        if (m_userListNode) {
            m_userListNode->addCell(rowContainer);
            m_userListNode->scrollToTop();
        }

        rank++;
    }

    return true;
}

RLLeaderboardLayer* RLLeaderboardLayer::create() {
    auto ret = new RLLeaderboardLayer();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

void RLLeaderboardLayer::keyBackClicked() {
    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
}
