#include "popup/RLCreditsPopup.hpp"
#include <Geode/Geode.hpp>
#include <cue/ListNode.hpp>
#include "RLAchievements.hpp"
#include "RLConstants.hpp"
#include "utils/CachedSettings.hpp"

using namespace geode::prelude;
using namespace rl;

// TODO: Cache credits info lol

static bool HasCheckedInThisSession = false;

RLCreditsPopup* RLCreditsPopup::create() {
    auto ret = new RLCreditsPopup();

    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

Ref<cue::ListNode> RLCreditsPopup::makeListNode() {
    Ref listNode = cue::ListNode::create({350.f, 205.f});
    CCSize contentSize = m_mainLayer->getContentSize();
    listNode->setPosition({contentSize.width / 2.f, contentSize.height / 2.f - 10.f});

    auto* scrollLayer = listNode->getScrollLayer();
    if (!scrollLayer) return nullptr;

    // FIXME: code smell
    if (auto* contentLayer = scrollLayer->m_contentLayer) {
        auto* layout = ColumnLayout::create();
        contentLayer->setLayout(layout);
        layout->setGap(0.f);
        layout->setAutoGrowAxis(0.f);
        layout->setAxisReverse(true);
    }

    return listNode;
}

bool RLCreditsPopup::init() {
    if (!Popup::init(440.f, 280.f)) return false;
    setTitle("Rated Layouts Credits");

    m_listNode = this->makeListNode();
    if (!m_listNode) return false;
    m_mainLayer->addChild(m_listNode.data());

    auto scrollLayer = m_listNode->getScrollLayer();
    if (!scrollLayer) return false;

    if (!CachedSettings::get()->disableScrollbar) {
        m_scrollbar = Scrollbar::create(scrollLayer);
        CCSize contentSize = m_mainLayer->getContentSize();
        m_scrollbar->setPosition({contentSize.width - 35.f, (contentSize.height / 2.f) - 5.f});
        m_scrollbar->setScale(0.9f);
        m_mainLayer->addChild(m_scrollbar);
    }

    // info button
    auto infoSpr = CCSprite::createWithSpriteFrameName("RL_info01.png"_spr);
    infoSpr->setScale(0.75f);
    auto infoBtn =
        CCMenuItemSpriteExtra::create(infoSpr, this, menu_selector(RLCreditsPopup::onInfo));
    infoBtn->setPosition(
        {m_mainLayer->getContentSize().width, m_mainLayer->getContentSize().height - 3});
    m_buttonMenu->addChild(infoBtn);

    // create spinner
    if (auto* contentLayer = scrollLayer->m_contentLayer) {
        auto* layout = ColumnLayout::create();
        contentLayer->setLayout(layout);
        layout->setGap(0.f);
        layout->setAutoGrowAxis(0.f);
        layout->setAxisReverse(true);

        auto* spinner = LoadingSpinner::create(48.f);
        spinner->setPosition(contentLayer->getContentSize() / 2);
        contentLayer->addChild(spinner);
        m_spinner = spinner;
    }

    // Fetch mod players
    Ref<RLCreditsPopup> self = this;
    async::spawn(
        LocalEndpoint::build("getCredits").expires(1_hours).verifySuccess().nostale().get(),
        [self](Result<matjson::Value> res) {
        if (res.isErr()) {
            log::warn("{}", res.unwrapErr());
            Notification::create(res.unwrapErr(), NotificationIcon::Error)->show();
            return;
        }

        if (!self) return;
        RLAchievements::onReward("misc_credits");
        self->initCredits(std::move(res).unwrap());
    });

    return true;
}

void RLCreditsPopup::initCredits(matjson::Value json) {
    ::HasCheckedInThisSession = true;
    Ref<cue::ListNode> node = this->makeListNode();
    Ref<RLCreditsPopup> self = this;

    async::spawn([self, node, json = std::move(json)]() -> arc::Future<bool> {
        if (!node) co_return false;

        auto addHeader = [&](RLBadgeKind K) {  // header yesz
            auto* badgeInfo = getBadgeInfo(K);
            //auto* content = self->m_listNode->getScrollLayer()->m_contentLayer;
            auto* tableCell = TableViewCell::create();
            tableCell->setContentSize({340.f, 30.f});

            auto* label = CCLabelBMFont::create(badgeInfo->title, "bigFont.fnt");
            // center the label in the cell
            label->setAnchorPoint({0.5f, 0.5f});
            const float contentW = tableCell->getContentSize().width;
            const float labelX = contentW / 2.f;
            label->limitLabelWidth(contentW - 120.f, 0.5f, 0.4f);
            label->setPosition({labelX, 15.f});

            tableCell->addChild(label);

            const float gap = 8.f;
            float labelWidth = label->getContentSize().width * label->getScale();

            // badge sprite on the left of the label
            CCSprite* headerBadge = CCSprite::createWithSpriteFrameName(badgeInfo->sprite);
            if (headerBadge) {
                headerBadge->setScale(0.9f);
                float badgeWidth = headerBadge->getContentSize().width * headerBadge->getScale();
                float badgeX = labelX - (labelWidth / 2.f) - gap - (badgeWidth / 2.f);
                headerBadge->setPosition({badgeX, 15.f});
                tableCell->addChild(headerBadge);
            }

            auto* headerMenu = CCMenu::create();
            headerMenu->setPosition({0, 0});
            auto* infoSpr = CCSprite::createWithSpriteFrameName("RL_info01.png"_spr);
            infoSpr->setScale(0.4f);
            auto* infoBtn = CCMenuItemSpriteExtra::create(
                infoSpr, self, menu_selector(RLCreditsPopup::onHeaderInfo));
            infoBtn->setTag(int(K));
            // place next to label
            float infoWidth = infoSpr->getContentSize().width * infoSpr->getScale();
            float infoX = labelX + (labelWidth / 2.f) + gap + (infoWidth / 2.f);
            infoBtn->setPosition({infoX, 15.f});
            headerMenu->addChild(infoBtn);
            tableCell->addChild(headerMenu);

            node->addCell(tableCell);
        };
        auto addPlayer = [&](const matjson::Value& userVal, RLBadgeKind K) {
            if (!userVal.isObject()) return;

            int accountId = userVal["accountId"].asInt().unwrapOrDefault();
            std::string username = userVal["username"].asString().unwrapOrDefault();
            int iconId = userVal["iconid"].asInt().unwrapOrDefault();
            int color1 = userVal["color1"].asInt().unwrapOrDefault();
            int color2 = userVal["color2"].asInt().unwrapOrDefault();
            int color3 = userVal["color3"].asInt().unwrapOrDefault();

            auto* cell = TableViewCell::create();
            cell->setContentSize({340.f, 50.f});

            // color background according to role subtype
            auto* bgSprite = CCSprite::create();
            bgSprite->setTextureRect(CCRectMake(0, 0, 360.f, 50.f));
            bgSprite->setPosition({170.f, 25.f});
            bgSprite->setOpacity(120);
            switch (K) {
            case RLBadgeKind::Owner:
                bgSprite->setColor({150, 255, 255});  // cyan(boi) for owner
                break;
            case RLBadgeKind::Developer:
                bgSprite->setColor({175, 255, 0});  // deep blue for developer
                break;
            case RLBadgeKind::ClassicAdmin:
                bgSprite->setColor({180, 180, 255});  // red for classic admin
                break;
            case RLBadgeKind::PlatAdmin:
                bgSprite->setColor({245, 150, 0});  // orange for plat admin
                break;
            case RLBadgeKind::LeaderboardAdmin:
                bgSprite->setColor({0, 245, 200});  // green for leaderboard admin
                break;
            case RLBadgeKind::ClassicMod:
                bgSprite->setColor({135, 190, 255});  // blue for classic mod
                break;
            case RLBadgeKind::PlatMod:
                bgSprite->setColor({255, 200, 150});  // cyan for plat mod
                break;
            case RLBadgeKind::LeaderboardMod:
                bgSprite->setColor({0, 245, 200});  // green for leaderboard mod
                break;
            case RLBadgeKind::Supporter: bgSprite->setColor({255, 125, 200}); break;
            case RLBadgeKind::Booster: bgSprite->setColor({190, 150, 255}); break;
            }
            cell->addChild(bgSprite);

            auto gm = GameManager::sharedState();
            auto player = SimplePlayer::create(iconId);
            player->updatePlayerFrame(iconId, IconType::Cube);
            player->setColors(gm->colorForIdx(color1), gm->colorForIdx(color2));
            if (color3 != 0) player->setGlowOutline(gm->colorForIdx(color3));
            player->setPosition({40.f, 25.f});
            cell->addChild(player);

            auto nameLabel = CCLabelBMFont::create(username.c_str(), "goldFont.fnt");
            nameLabel->setAnchorPoint({0.f, 0.5f});
            nameLabel->setScale(0.8f);
            nameLabel->setPosition({80.f, 25.f});

            auto menu = CCMenu::create();
            menu->setPosition({0, 0});
            auto accountButton = CCMenuItemSpriteExtra::create(
                nameLabel, self, menu_selector(RLCreditsPopup::onAccountClicked));
            accountButton->setTag(accountId);
            accountButton->setPosition({70.f, 25.f});
            accountButton->setAnchorPoint({0.f, 0.5f});
            menu->addChild(accountButton);
            cell->addChild(menu);

            node->addCell(cell);
        };

        auto addSection = [&](std::string_view tag, RLBadgeKind K) {
            if (!json.contains(tag)) return;
            auto arr = json[tag].asArray();
            if (arr.isErr() || arr.unwrap().empty()) return;
            // Create the section
            addHeader(K);
            for (auto& val : arr.unwrap()) addPlayer(val, K);
        };

        arc::Notify notify;

        // I could unnest this but I'm lazy tbh
        Loader::get()->queueInMainThread([&]() {
            addSection("owner", RLBadgeKind::Owner);
            addSection("developer", RLBadgeKind::Developer);
            Loader::get()->queueInMainThread([&]() {
                addSection("classicAdmins", RLBadgeKind::ClassicAdmin);
                addSection("platAdmins", RLBadgeKind::PlatAdmin);
                Loader::get()->queueInMainThread([&]() {
                    addSection("classicModerators", RLBadgeKind::ClassicMod);
                    addSection("platModerators", RLBadgeKind::PlatMod);
                    Loader::get()->queueInMainThread([&]() {
                        addSection("leaderboardAdmins", RLBadgeKind::LeaderboardAdmin);
                        addSection("leaderboardModerators", RLBadgeKind::LeaderboardMod);
                        Loader::get()->queueInMainThread([&]() {
                            addSection("supporters", RLBadgeKind::Supporter);
                            addSection("boosters", RLBadgeKind::Booster);
                            notify.notifyOne(/*store=*/true);
                        });
                    });
                });
            });
        });

        co_await notify.notified();
        co_return true;
    }, [self, node](bool didSucceed) {
        if (!didSucceed) {
            log::warn("Failed to initialize credits");
            Notification::create("Failed to get credits", NotificationIcon::Error)->show();
            return;
        }

        self->m_mainLayer->addChild(node.data());
        node->updateLayout();

        if (auto* scrollLayer = node->getScrollLayer()) {
            scrollLayer->scrollToTop();
            if (auto* scrollbar = self->m_scrollbar) scrollbar->setTarget(scrollLayer);
        } else if (auto* scrollbar = self->m_scrollbar)
            scrollbar->removeFromParent();

        if (auto list = self->m_listNode) list->removeFromParent();

        self->m_listNode = node;
        self->m_spinner = nullptr;
    });
}

void RLCreditsPopup::initCreditsOld(matjson::Value json) {
    if (m_spinner) {
        m_spinner->removeFromParent();
        m_spinner = nullptr;
    }

    // populate players
    if (!m_listNode) return;
    auto* scrollLayer = m_listNode->getScrollLayer();
    if (!scrollLayer || !scrollLayer->m_contentLayer) return;
    m_listNode->clear();

    auto addHeader = [&, this](RLBadgeKind K) {  // header yesz
        auto* badgeInfo = getBadgeInfo(K);
        //auto* content = self->m_listNode->getScrollLayer()->m_contentLayer;
        auto* tableCell = TableViewCell::create();
        tableCell->setContentSize({340.f, 30.f});

        auto* label = CCLabelBMFont::create(badgeInfo->title, "bigFont.fnt");
        // center the label in the cell
        label->setAnchorPoint({0.5f, 0.5f});
        const float contentW = tableCell->getContentSize().width;
        const float labelX = contentW / 2.f;
        label->limitLabelWidth(contentW - 120.f, 0.5f, 0.4f);
        label->setPosition({labelX, 15.f});

        tableCell->addChild(label);

        const float gap = 8.f;
        float labelWidth = label->getContentSize().width * label->getScale();

        // badge sprite on the left of the label
        CCSprite* headerBadge = CCSprite::createWithSpriteFrameName(badgeInfo->sprite);
        if (headerBadge) {
            headerBadge->setScale(0.9f);
            float badgeWidth = headerBadge->getContentSize().width * headerBadge->getScale();
            float badgeX = labelX - (labelWidth / 2.f) - gap - (badgeWidth / 2.f);
            headerBadge->setPosition({badgeX, 15.f});
            tableCell->addChild(headerBadge);
        }

        auto* headerMenu = CCMenu::create();
        headerMenu->setPosition({0, 0});
        auto* infoSpr = CCSprite::createWithSpriteFrameName("RL_info01.png"_spr);
        infoSpr->setScale(0.4f);
        auto* infoBtn = CCMenuItemSpriteExtra::create(
            infoSpr, this, menu_selector(RLCreditsPopup::onHeaderInfo));
        infoBtn->setTag(int(K));
        // place next to label
        float infoWidth = infoSpr->getContentSize().width * infoSpr->getScale();
        float infoX = labelX + (labelWidth / 2.f) + gap + (infoWidth / 2.f);
        infoBtn->setPosition({infoX, 15.f});
        headerMenu->addChild(infoBtn);
        tableCell->addChild(headerMenu);

        if (m_listNode) {
            m_listNode->addCell(tableCell);
        }
    };
    auto addPlayer = [&, this](const matjson::Value& userVal, RLBadgeKind K) {
        if (!userVal.isObject()) return;

        int accountId = userVal["accountId"].asInt().unwrapOrDefault();
        std::string username = userVal["username"].asString().unwrapOrDefault();
        int iconId = userVal["iconid"].asInt().unwrapOrDefault();
        int color1 = userVal["color1"].asInt().unwrapOrDefault();
        int color2 = userVal["color2"].asInt().unwrapOrDefault();
        int color3 = userVal["color3"].asInt().unwrapOrDefault();

        auto* cell = TableViewCell::create();
        cell->setContentSize({340.f, 50.f});

        // color background according to role subtype
        auto* bgSprite = CCSprite::create();
        bgSprite->setTextureRect(CCRectMake(0, 0, 360.f, 50.f));
        bgSprite->setPosition({170.f, 25.f});
        bgSprite->setOpacity(120);
        switch (K) {
        case RLBadgeKind::Owner:
            bgSprite->setColor({150, 255, 255});  // cyan(boi) for owner
            break;
        case RLBadgeKind::Developer:
            bgSprite->setColor({175, 255, 0});  // deep blue for developer
            break;
        case RLBadgeKind::ClassicAdmin:
            bgSprite->setColor({180, 180, 255});  // red for classic admin
            break;
        case RLBadgeKind::PlatAdmin:
            bgSprite->setColor({245, 150, 0});  // orange for plat admin
            break;
        case RLBadgeKind::LeaderboardAdmin:
            bgSprite->setColor({0, 245, 200});  // green for leaderboard admin
            break;
        case RLBadgeKind::ClassicMod:
            bgSprite->setColor({135, 190, 255});  // blue for classic mod
            break;
        case RLBadgeKind::PlatMod:
            bgSprite->setColor({255, 200, 150});  // cyan for plat mod
            break;
        case RLBadgeKind::LeaderboardMod:
            bgSprite->setColor({0, 245, 200});  // green for leaderboard mod
            break;
        case RLBadgeKind::Supporter: bgSprite->setColor({255, 125, 200}); break;
        case RLBadgeKind::Booster: bgSprite->setColor({190, 150, 255}); break;
        }
        cell->addChild(bgSprite);

        auto gm = GameManager::sharedState();
        auto player = SimplePlayer::create(iconId);
        player->updatePlayerFrame(iconId, IconType::Cube);
        player->setColors(gm->colorForIdx(color1), gm->colorForIdx(color2));
        if (color3 != 0) player->setGlowOutline(gm->colorForIdx(color3));
        player->setPosition({40.f, 25.f});
        cell->addChild(player);

        auto nameLabel = CCLabelBMFont::create(username.c_str(), "goldFont.fnt");
        nameLabel->setAnchorPoint({0.f, 0.5f});
        nameLabel->setScale(0.8f);
        nameLabel->setPosition({80.f, 25.f});

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        auto accountButton = CCMenuItemSpriteExtra::create(
            nameLabel, this, menu_selector(RLCreditsPopup::onAccountClicked));
        accountButton->setTag(accountId);
        accountButton->setPosition({70.f, 25.f});
        accountButton->setAnchorPoint({0.f, 0.5f});
        menu->addChild(accountButton);
        cell->addChild(menu);

        if (m_listNode) {
            m_listNode->addCell(cell);
        }
    };

    auto addSection = [&](std::string_view tag, RLBadgeKind K) {
        if (!json.contains(tag)) return;
        auto arr = json[tag].asArray();
        if (arr.isErr() || arr.unwrap().empty()) return;
        // Create the section
        addHeader(K);
        for (auto& val : arr.unwrap()) addPlayer(val, K);
    };

    addSection("owner", RLBadgeKind::Owner);
    addSection("developer", RLBadgeKind::Developer);
    addSection("classicAdmins", RLBadgeKind::ClassicAdmin);
    addSection("platAdmins", RLBadgeKind::PlatAdmin);
    addSection("classicModerators", RLBadgeKind::ClassicMod);
    addSection("platModerators", RLBadgeKind::PlatMod);
    addSection("leaderboardAdmins", RLBadgeKind::LeaderboardAdmin);
    addSection("leaderboardModerators", RLBadgeKind::LeaderboardMod);
    addSection("supporters", RLBadgeKind::Supporter);
    addSection("boosters", RLBadgeKind::Booster);

    if (m_listNode) {
        m_listNode->updateLayout();
        if (auto* listScroll = m_listNode->getScrollLayer()) {
            listScroll->scrollToTop();
        }
    }

    ::HasCheckedInThisSession = true;
}

void RLCreditsPopup::onAccountClicked(CCObject* sender) {
    auto button = static_cast<CCMenuItem*>(sender);
    int accountId = button->getTag();
    ProfilePage::create(accountId, false)->show();
}

void RLCreditsPopup::onInfo(CCObject* sender) {
    MDPopup::create(
        "Becoming a Layout Moderator",
        "To become a **<cl>Classic</c>/<co>Platformer</c>/<cb>Leaderboard</c> Layout "
        "Moderator</c>**, you are required to join the <cl>Rated Layouts Discord Server</c> and "
        "be <cg>active in the community</c>.\n"
        "There's an <cl>application form</c> in the server that you can fill out and the Admins "
        "usually review these applications.\n"
        "### <cr>Begging for Layout Mod to the Layout "
        "Admins will be ignored and lower your chances of becoming a mod.</c>\n"
        "If you have any questions about the application process or the role, feel free to ask "
        "in the <cl>Rated Layouts Discord Server</c>.\n"
        "\r\n\r\n---\r\n\r\n"
        "### Moderator Responsibilities\n"
        "- Moderators are expected to be <cg>active in the community</c> and help maintain the "
        "quality of the <cl>Rated Layouts</c>.\n"
        "- This includes <co>suggesting levels, rating levels</c> and <cy>providing feedback</c> "
        "to level creators.\n"
        "- Moderators may also be asked to help with <cg>managing the community</c>, such as "
        "moderating the leaderboard section or assisting with events.\n"
        "\r\n\r\n---\r\n\r\n"
        "If you are <cg>interested in becoming a layout moderator</c>, make sure to join the "
        "<cl>Rated Layouts Discord Server</c> and apply in the <cl>application form</c>!",
        "OK")
        ->show();
}

void RLCreditsPopup::onHeaderInfo(CCObject* sender) {
    if (auto* btn = static_cast<CCMenuItem*>(sender)) rl::showRoleInfoPopup(btn->getTag());
}
