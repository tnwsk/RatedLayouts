#include "layer/RLShopLayer.hpp"
#include "RLDialogIcons.hpp"
#include "custom/RLShopkeeperSprite.hpp"
#include "utils/NoHashHasher.hpp"
#include "utils/RLArgon.hpp"
#include "utils/RLData.hpp"
#include "utils/RandomGen.hpp"
#include "popup/RLNameplateSubmitPopup.hpp"
#include "popup/RLBuyItemPopup.hpp"
#include "RLSecretLayer1.hpp"
#include <Geode/Enums.hpp>
#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <arc/sync/Mutex.hpp>
#include <cue/DropdownNode.hpp>
#include <fmt/format.h>
#include "RLConstants.hpp"
#include "RLRubyUtils.hpp"

using namespace geode::prelude;
using namespace rl;

using PageHasher = NoHashHasher<int>;
using ShopCacheType = std::unordered_map<int, std::vector<RLShopLayer::ShopItem>, PageHasher>;

// ListButtonPage

static arc::Mutex<ShopCacheType> ShopCache;
static std::atomic<int> TotalPages = 19;

static constexpr int kACTION_TAG = 59597;

RLShopLayer* RLShopLayer::create() {
    auto layer = new RLShopLayer();
    if (layer && layer->init()) {
        layer->autorelease();
        return layer;
    }
    delete layer;
    return nullptr;
}

bool RLShopLayer::init() {
    if (!CCLayer::init()) return false;

    this->preloadShopPage(0);
    this->preloadShopPage(1);
    this->preloadShopPage(-1);
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    addBackButton(this, BackButtonStyle::Pink);

    // bg
    auto shopBGSpr = CCSprite::createWithSpriteFrameName("RL_shopBG.png"_spr);
    auto bgSize = shopBGSpr->getTextureRect().size;

    shopBGSpr->setAnchorPoint({0.0f, 0.0f});
    shopBGSpr->setScaleX((winSize.width + 10.0f) / bgSize.width);
    shopBGSpr->setScaleY((winSize.height + 10.0f) / bgSize.height);
    shopBGSpr->setPosition({-5.0f, -5.0f});
    this->addChild(shopBGSpr, -3);

    // desk
    auto deckSpr = CCSprite::createWithSpriteFrameName("RL_storeDesk.png"_spr);
    deckSpr->setPosition({winSize.width / 2, 95});
    this->addChild(deckSpr);

    // ruby counter
    auto rubySpr = CCSprite::createWithSpriteFrameName("RL_rubiesIcon.png"_spr);
    rubySpr->setPosition({winSize.width - 20, winSize.height - 20});
    rubySpr->setScale(0.7f);
    this->addChild(rubySpr);

    // layout creator menu
    auto* menu = CCMenu::create();
    menu->setPosition({0, 0});
    this->addChild(menu, -1);

    this->initDropdownMenu();

    // layout creator (clickable)
    auto* gm = GameManager::sharedState();
    //auto* shopkeeperIcon = CCSprite::createWithSpriteFrameName("RL_arcticwoof01.png"_spr);
    auto* shopkeeperIcon = RLShopkeeperSprite::create();
    shopkeeperIcon->setScale(2.f);

    m_shopkeeper = CCMenuItemSpriteExtra::create(shopkeeperIcon, this, menu_selector(RLShopLayer::onShopkeeper));
    m_shopkeeper->setPosition({winSize.width / 2 - 120, deckSpr->getContentHeight()});
    m_shopkeeper->setAnchorPoint({0.5f, .1f});
    m_shopkeeper->m_scaleMultiplier = 1.02;
    menu->addChild(m_shopkeeper);

    int currentRubies = rl::getPlayerRubies();

    // ruby counter label
    auto rubyLabel = CCCounterLabel::create(currentRubies, "bigFont.fnt", FormatterType::Integer);
    rubyLabel->setPosition({rubySpr->getPositionX() - 15, rubySpr->getPositionY()});
    rubyLabel->setAnchorPoint({1.0f, 0.5f});
    rubyLabel->setScale(0.6f);
    m_rubyLabel = rubyLabel;
    this->addChild(rubyLabel);

    // ruby shop sign
    auto shopSignSpr = CCSprite::createWithSpriteFrameName("RL_shopSign_001.png"_spr);
    shopSignSpr->setPosition({winSize.width / 2 + 60, winSize.height - 45});
    shopSignSpr->setScale(1.2f);
    this->addChild(shopSignSpr, -2);

    // PLUSHIESS
    auto plushiesSpr = CCSprite::createWithSpriteFrameName("RL_plushpile.png"_spr);
    plushiesSpr->setPosition({winSize.width / 2 - 10, deckSpr->getContentHeight()});
    plushiesSpr->setAnchorPoint({0.5f, 0.1f});
    this->addChild(plushiesSpr, -2);

    // random sign image
    auto signFrame = rl::selectRandom<std::string>("signImage_00.png"_spr,
                                                   "signImage_01.png"_spr,
                                                   "signImage_02.png"_spr,
                                                   "signImage_03.png"_spr,
                                                   "signImage_04.png"_spr,
                                                   "signImage_05.png"_spr,
                                                   "signImage_06.png"_spr,
                                                   "signImage_07.png"_spr,
                                                   "signImage_08.png"_spr,
                                                   "signImage_09.png"_spr,
                                                   "signImage_10.png"_spr);
    auto* signSpr = CCSprite::createWithSpriteFrameName(signFrame.c_str());
    if (signSpr) {
        signSpr->setPosition({19, 32});
        signSpr->setRotation(-10);
        plushiesSpr->addChild(signSpr, 1);
    }

    // bottom left redeem button
    auto orcaleSpr = CCSprite::createWithSpriteFrameName("RL_observatoryDoor.png"_spr);
    orcaleSpr->setColor({50, 50, 50});
    orcaleSpr->setOpacity(150);
    orcaleSpr->setScale(1.25f);
    auto redeemBtn = CCMenuItemSpriteExtra::create(orcaleSpr, this, menu_selector(RLShopLayer::onRedeemLayer));
    redeemBtn->setPosition({20, 25});
    menu->addChild(redeemBtn);

    // shop item menu
    auto shopMenu = CCMenu::create();
    shopMenu->setPosition({deckSpr->getContentSize().width / 2, deckSpr->getContentSize().height / 2});
    shopMenu->setContentSize({deckSpr->getContentSize().width - 40, deckSpr->getContentSize().height - 30});
    // arrange rows vertically
    shopMenu->setLayout(
        ColumnLayout::create()->setGap(35.f)->setAxisAlignment(AxisAlignment::Center)->setAxisReverse(true));

    // two horizontal row menus
    auto rowH = [&](void) {
        auto menuWithinAMenu = CCMenu::create();
        menuWithinAMenu->setPosition(
            {menuWithinAMenu->getContentSize().width / 2.f, menuWithinAMenu->getContentSize().height / 2.f});
        menuWithinAMenu->setContentSize(
            {shopMenu->getContentSize().width, (shopMenu->getContentSize().height - 8.f) / 2.f});
        menuWithinAMenu->setLayout(
            RowLayout::create()->setGap(50.f)->setAxisAlignment(AxisAlignment::Center)->setAxisReverse(false));
        menuWithinAMenu->updateLayout();
        return menuWithinAMenu;
    };

    m_shopRow1 = rowH();
    m_shopRow2 = rowH();

    m_shopItems.clear();
    m_shopRow1->setAnchorPoint({0.5f, 0.5f});
    m_shopRow2->setAnchorPoint({0.5f, 0.5f});

    // compute vertical positions to match the ColumnLayout that used to be
    auto shopMenuCS = shopMenu->getContentSize();
    float rowHeight = m_shopRow1->getContentSize().height;
    const float gap = 20.f;
    const float centerY = shopMenu->getPositionY();
    const float offset = (rowHeight / 2.f) + (gap / 2.f);

    m_shopRow1->setPosition({shopMenu->getPositionX(), centerY + offset - 16});  // the magic number is for my ocd
    m_shopRow2->setPosition({shopMenu->getPositionX(), centerY - offset + 3});
    shopMenu->addChild(m_shopRow1, 1);
    shopMenu->addChild(m_shopRow2, 1);

    // pagination controls
    m_pageLabel = CCLabelBMFont::create("", "goldFont.fnt");
    if (m_pageLabel) {
        m_pageLabel->setScale(0.5f);
        m_pageLabel->setPosition({shopMenu->getPositionX(), 3.f});
        m_pageLabel->setAnchorPoint({0.5f, 0.f});
        deckSpr->addChild(m_pageLabel, 2);
    }

    // pagination menu thingy
    auto pageMenu = CCMenu::create();
    pageMenu->setPosition({0, 0});
    pageMenu->setContentSize(deckSpr->getContentSize());
    deckSpr->addChild(pageMenu, 2);

    auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    if (prevSpr) {
        m_prevPageBtn = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(RLShopLayer::onPrevPage));
        if (m_prevPageBtn) {
            m_prevPageBtn->setPosition({-10, pageMenu->getContentSize().height / 2});
            pageMenu->addChild(m_prevPageBtn);
        }
    }

    auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    nextSpr->setFlipX(true);
    if (nextSpr) {
        m_nextPageBtn = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(RLShopLayer::onNextPage));
        if (m_nextPageBtn) {
            m_nextPageBtn->setPosition({pageMenu->getContentSize().width + 10, pageMenu->getContentSize().height / 2});
            pageMenu->addChild(m_nextPageBtn);
        }
    }

    loadShopPage(0);

    shopMenu->updateLayout();
    deckSpr->addChild(shopMenu);
    this->setKeypadEnabled(true);
    return true;
}

void RLShopLayer::performDropdownAction() {
    m_pendingDropdownAction = 0;
    this->initDropdownMenu();
}

void RLShopLayer::initDropdownMenu() {
    if (m_dropdownMenu) {
        m_dropdownMenu->removeFromParent();
        m_dropdownMenu = nullptr;
    }

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    m_dropdownMenu = cue::DropdownNode::create({0, 0, 0, 150}, 130.f, 25.f, 120.f);
    m_dropdownMenu->setPosition({60, winSize.height - 15});
    m_dropdownMenu->setAnchorPoint({0.f, 1.f});
    this->addChild(m_dropdownMenu, 2);

    // dropdown menu options (reset rubies, unequip nameplate, submission form)
    auto dropdownLabel = CCLabelBMFont::create("Shop Options", "goldFont.fnt");
    dropdownLabel->limitLabelWidth(100, .7, .3);
    m_dropdownMenu->addCell(dropdownLabel);
    dropdownLabel->setPositionX(5.f);

    auto resetBtn = CCLabelBMFont::create("Clear Rubies", "bigFont.fnt");
    resetBtn->limitLabelWidth(100, .7, .3);
    m_dropdownMenu->addCell(resetBtn);
    resetBtn->setPositionX(15.f);

    auto unequipBtn = CCLabelBMFont::create("Unequip Nameplate", "bigFont.fnt");
    unequipBtn->limitLabelWidth(100, .7, .3);
    m_dropdownMenu->addCell(unequipBtn);
    unequipBtn->setPositionX(15.f);

    auto nameplateTestBtn = CCLabelBMFont::create("Submission", "bigFont.fnt");
    nameplateTestBtn->limitLabelWidth(100, .7, .3);
    m_dropdownMenu->addCell(nameplateTestBtn);
    nameplateTestBtn->setPositionX(15.f);

    //auto submitBtn = CCLabelBMFont::create("Submission Form", "bigFont.fnt");
    //submitBtn->limitLabelWidth(100, .7, .3);
    //m_dropdownMenu->addCell(submitBtn);
    //submitBtn->setPositionX(15.f);

    m_dropdownMenu->setCallback([this](size_t index, CCNode*) {
        switch (index) {
            case 1: this->onResetRubies(); break;
            case 2: this->onUnequipNameplate(); break;
            case 3: this->onSubmitNameplate(); break;
            case 4: this->onForm(); break;
        }

        // the most hacky way to create a dropdown menu
        // honestly i could just copy the cue::DropdownNode code and modify it
        // but... thats just lots of overhead and i could just use the existing dropdown node
        // and add hacky functions to make it work the way i wanted :)
        if (index > 0) {
            this->m_dropdownMenu->setExpanded(false);
            this->runAction(
                CCSequence::create(CCDelayTime::create(0.01f),
                                   CCCallFunc::create(this, callfunc_selector(RLShopLayer::performDropdownAction)),
                                   nullptr));
        }
    });
}

void RLShopLayer::onForm() {
    createQuickPopup("Nameplate Submission Form",
                     "You will be redirected to the <cl>Nameplate Submission "
                     "Form</c> in your web browser.\n<cy>Continue?</c>",
                     "No",
                     "Yes",
                     [](auto, bool yes) {
                         if (!yes) return;
                         Notification::create("Opening a new link to the browser", NotificationIcon::Info)->show();
                         utils::web::openLinkInBrowser("https://forms.gle/3UU5JJE1XrfwPK5u7");
                     });
}

// play the dum audio lol
void RLShopLayer::onEnterTransitionDidFinish() {
    CCLayer::onEnterTransitionDidFinish();
    FMODAudioEngine::sharedEngine()->playMusic("rubyShop.mp3"_spr, true, 0.f, 0);
    refreshRubyLabel();
}

void RLShopLayer::onExitTransitionDidStart() {
    CCLayer::onExitTransitionDidStart();
    GameManager::sharedState()->playMenuMusic();
}

void RLShopLayer::onSubmitNameplate() {
    auto popup = RLNameplateSubmitPopup::create();
    popup->show();
}

static void StopAllActions(CCNode* node) {
    if (!node) return;
    CCAction* action = nullptr;
    while ((action = node->getActionByTag(kACTION_TAG))) {
        action->update(1.f);
        node->stopAction(action);
    }
}

void RLShopLayer::onShopkeeper(CCObject* sender) {
    if (rl::globalRNG()->generate<int>(0, 3) != 0)
        return this->onShopkeeperDialog(sender);
    // Make the shopkeeper jig
    constexpr float kTime = 0.06;
    constexpr float kFactor = 2.f;
    auto* action = CCSequence::create(
        CCSkewTo::create(kTime,     1.f + kFactor, 1.f),
        CCSkewTo::create(kTime * 2, 1.f - kFactor, 1.f),
        CCEaseSineOut::create(CCSkewTo::create(kTime * 2, 1.f, 1.f)),
        nullptr
    );
    action->setTag(kACTION_TAG);
    // Add the action.
    StopAllActions(m_shopkeeper);
    m_shopkeeper->runAction(action);

    auto sfx = fmt::format("grunt{:02}.ogg", rl::globalRNG()->generate<int>(1, 4));
    FMODAudioEngine::sharedEngine()->playEffect(sfx);
}

void RLShopLayer::onShopkeeperDialog(CCObject* sender) {
    static int lastV = 0;
    // gen random
    int v = rl::globalRNG()->generate<int>(1, 21);
    for (int iters = 0; v == lastV && iters < 3; ++iters) {
        v = rl::globalRNG()->generate<int>(1, 21);
    }
    lastV = v;
    log::debug("Random response id: {}", v);

    DialogObject* dialogObj = nullptr;
    std::string response = "Can I help you?";
    std::string voiceline;
    switch (v) {
        case 1:
            response = "I got all of the <cg>nameplates</c> in stock!"; break;
        case 2:
            response =
                "<cg>Layout Creator</c>? <cl>Well, he kind of ran away when I arrived</c>"
                ", odd fella but oh well...";
            break;
        case 3:
            response =
                "The plushies are <cr>NOT FOR SALE</c>. I just keep them here "
                "because they are <cp>cute.</c>";
            break;
        case 4:
            response =
                "<cl>Darkore</c>, that weird kid that put this <cg>awesome music</c>"
                " in the shop? Truly peak bud :)";
            break;
        case 5:
            response =
                "Someone must have broken into the <cg>front door</c> while "
                "<cl>I was away...</c>";
            break;
        case 6:
        case 7:
            response = "Are you going to buy something? <cy>Or just keep annoying me?</c>";
            voiceline = "RL_sigh01.ogg"_spr;
            break;
        case 8:
        case 9:
            response = "Cooking some <cg>new nameplates</c> for you all! <cl>Can't wait for you to see them</c>!";
            break;
        case 10:
            response = "I'm <cr>lurking</c> on <cl>every move</c> you do..."; break;
        case 11:
            response = "<cg>Fun fact about me!</c> I actually <co>suck at making gameplay</c>."; break;
        case 12:
        case 13:
            response = "Ask <cp>The Oracle</c> about <cf>me</c>! That would be funny."; break;
        case 14:
            response =
                "Would you like to buy my entire shop for <cr>100k Rubies?</c> I know someone is <cy>interested</c> :P";
            break;
        case 15:
        case 16:
        case 17:
            response = "I heard there's <cf>The Spire</c> nearby, but I don't know how to get in there..."; break;
        case 18:
            response =
                "I'm thinking of <cr><s100>burning</s></c> down this shop... <d100> <cy>just kidding!</c> <d100> "
                "<co>maybe...</c>";
            voiceline = selectRandom<const char*>("RL_laugh01.ogg"_spr, "RL_fire01.ogg"_spr);
            break;
        case 19:
        case 20:
            response = "Wow <cl>Rated Layouts</c> updated after months... thats <co>shocking</c>..."; break;
        default:
            response = "<cg>Weh!</c>";
            voiceline = "RL_huh01.ogg"_spr;
            break;
    }
    dialogObj = DialogObject::create("ArcticWoof", response.c_str(), 1, 1.f, false, ccWHITE);

    auto dialog = DialogLayer::createDialogLayer(dialogObj, nullptr, 2);
    dialog->addToMainScene();
    dialog->animateInRandomSide();

    rl::setDialogObjectCustomIcon(dialog, "RL_dialogIconAW.png"_spr);

    if (!voiceline.empty())
        FMODAudioEngine::sharedEngine()->playEffect(voiceline);
}

void RLShopLayer::onBuyItem(CCObject* sender) {
    auto item = static_cast<CCMenuItemSpriteExtra*>(sender);
    int idx = item->getTag();
    RLNameplateInfo info;
    if (!RLNameplateItem::getInfo(idx, &info)) {
        log::warn("RLShopLayer: no nameplate info for index {}", idx);
        return;
    }

    // open buy popup with creator/price information
    RLBuyItemPopup::create(info, this)->show();
}

void RLShopLayer::onUnequipNameplate() {
    createQuickPopup(
        "Unequip Nameplate",
        "Are you sure you want to <cr>unequip your current nameplate</c>?"
        "\n<cy>You can re-equip it later from this shop page.</c>",
        "No",
        "Yes",
        [this](FLAlertLayer*, bool yes) {
            if (!yes) return;

            // show a spinner/popup while we call the backend
            auto upopup = UploadActionPopup::create(nullptr, "Unequipping nameplate...");
            upopup->show();

            // validate token
            auto token = RLArgon::token();
            if (token.empty()) {
                upopup->showFailMessage("Argon auth missing");
                return;
            }

            // build JSON body (same format as RLBuyItemPopup::onApply)
            matjson::Value jsonBody = matjson::Value::object();
            jsonBody["accountId"] = GJAccountManager::get()->m_accountID;
            jsonBody["argonToken"] = token;
            jsonBody["index"] = 0;

            auto req = web::WebRequest();
            req.bodyJSON(jsonBody);

            Ref<UploadActionPopup> popupRef = upopup;
            Ref<RLShopLayer> self = this;
            async::spawn(
                req.post(std::string(rl::BASE_API_URL) + "/setNameplate"), [self, popupRef](web::WebResponse res) {
                    if (!popupRef) return;
                    if (!res.ok()) {
                        log::warn("Failed to unequip nameplate on server: {}", res.code());
                        popupRef->showFailMessage("Failed to unequip nameplate on server.");
                        return;
                    }
                    auto jsonRes = res.json();
                    if (!jsonRes) {
                        popupRef->showFailMessage("Invalid server response.");
                        return;
                    }
                    auto json = jsonRes.unwrap();
                    bool success = json["success"].asBool().unwrapOrDefault();
                    if (!success) {
                        popupRef->showFailMessage(json["message"].asString().unwrapOr("Failed to unequip nameplate."));
                        return;
                    }

                    Mod::get()->setSavedValue<int>("selected_nameplate", 0);
                    popupRef->showSuccessMessage("Nameplate unequipped!");

                    if (self) {
                        self->updateShopPage();
                    }
                });
        });
}

// TODO: Cache the rows and such so we can add an animation
void RLShopLayer::updateShopPage() {
    // clear rows first
    if (m_shopRow1) m_shopRow1->removeAllChildrenWithCleanup(true);
    if (m_shopRow2) m_shopRow2->removeAllChildrenWithCleanup(true);

    // add current items
    int totalItems = static_cast<int>(m_shopItems.size());
    for (int i = 0; i < totalItems; ++i) {
        const auto& s = m_shopItems[i];
        auto* item = RLNameplateItem::create(s, this, menu_selector(RLShopLayer::onBuyItem));
        item->setTag(s.index);
        if (i < 4) {
            m_shopRow1->addChild(item);
        } else {
            m_shopRow2->addChild(item);
        }
    }

    if (m_shopRow1) m_shopRow1->updateLayout();
    if (m_shopRow2) m_shopRow2->updateLayout();

    // update page UI
    if (m_pageLabel) {
        m_pageLabel->setString(fmt::format("{}/{}", m_shopPage + 1, TotalPages).c_str());
    }
    if (m_prevPageBtn) {
        m_prevPageBtn->setEnabled(true);
        m_prevPageBtn->setOpacity(255);
    }
    if (m_nextPageBtn) {
        m_nextPageBtn->setEnabled(true);
        m_nextPageBtn->setOpacity(255);
    }

    // ensure parent recomputes layout
    if (m_shopRow1 && m_shopRow2) {
        if (auto* parent = static_cast<CCNode*>(m_shopRow1->getParent()))
            parent->updateLayout();
    }
}

void RLShopLayer::refreshRubyLabel() {
    if (!m_rubyLabel) return;
    int val = rl::getPlayerRubies();
    m_rubyLabel->setTargetCount(val);
    m_rubyLabel->updateCounter(0.25f);
}

void RLShopLayer::onPrevPage(CCObject* sender) {
    if (TotalPages <= 0) return;
    int prevPage = m_shopPage - 1;
    if (prevPage < 0) {
        prevPage = TotalPages - 1;
    }
    loadShopPage(prevPage);
}

void RLShopLayer::onNextPage(CCObject* sender) {
    if (TotalPages <= 0) return;
    int nextPage = m_shopPage + 1;
    if (nextPage >= TotalPages)
        nextPage = 0;
    loadShopPage(nextPage);
}

void RLShopLayer::keyBackClicked() {
    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
}

static std::vector<RLShopLayer::ShopItem> parseShopItems(std::vector<matjson::Value> const& arr) {
    std::vector<RLShopLayer::ShopItem> items;
    items.reserve(arr.size());
    for (auto& it : arr) {
        // TODO: Add matjson::Serializer
        RLShopLayer::ShopItem si;
        si.index = it["index"].asInt().unwrapOrDefault();
        si.price = it["price"].asInt().unwrapOrDefault();
        auto author = it.contains("author") ? it["author"] : it;
        si.creatorId = author["accountId"].asInt().unwrapOrDefault();
        si.creatorUsername = author["username"].asString().unwrapOrDefault();
        si.iconUrl = std::string(rl::BASE_API_URL) + it["url"].asString().unwrapOrDefault();
        items.push_back(si);
    }
    return items;
}

void RLShopLayer::loadShopPage(int page) {
    this->preloadShopPage(page - 1);
    this->preloadShopPage(page + 1);

    m_shopPage = page;
    bool isCached = false;
    /*Is cached*/ {
        auto cache = ShopCache.blockingLock();
        isCached = cache->contains(page);
        if (isCached) {
            log::info("Got cached shop page '{}'", page);
            this->m_shopItems = (*cache)[page];
        }
    }
    if (isCached) {
        this->updateShopPage();
        return;
    }

    Ref<RLShopLayer> self = this;
    matjson::Value body = matjson::makeObject({{"page", page + 1}, {"amount", 8}});
    async::spawn(LocalEndpoint::get("getNameplates", std::move(body)), [self, page](Result<matjson::Value> res) {
        if (res.isErr()) {
            Notification::create(res.unwrapErr(), NotificationIcon::Warning)->show();
            return;
        }

        auto json = std::move(res).unwrap();
        self->m_shopItems.clear();

        // server may return object with nameplates/items array or raw array
        if (json.isObject()) {
            auto itemsVal = json.contains("nameplates") ? json["nameplates"] : json["items"];
            if (itemsVal.isArray()) {
                auto& arr = itemsVal.asArray().unwrap();
                std::vector<ShopItem> items = parseShopItems(arr);
                self->m_shopItems = items;
                auto cache = ShopCache.blockingLock();
                if (!cache->contains(page))
                    (*cache)[page] = std::move(items);
            }
        }
        self->updateShopPage();
    });
}

void RLShopLayer::preloadShopPage(int page) {
    const auto totalPages = TotalPages.load();
    if (page < 0)
        page = totalPages - 1;
    else if (page >= totalPages)
        page = 0;
    /*Is cached*/ {
        auto cache = ShopCache.blockingLock();
        if (cache->contains(page))
            return;
    }
    log::trace("Preloading shop page {}", page);
    // Set up our map without actually changing anything
    async::spawn([page]() -> arc::Future<> {
        matjson::Value body = matjson::makeObject({{"page", page + 1}, {"amount", 8}});
        Result<matjson::Value> res = co_await LocalEndpoint::get("getNameplates", std::move(body));
        // Check the results...
        if (res.isErr()) co_return;
        auto json = std::move(res).unwrap();
        // server may return object with nameplates/items array or raw array
        if (!json.isObject()) co_return;
        if (auto nPages = json["totalPages"].asInt())
            TotalPages = nPages.unwrap();
        auto itemsVal = json.contains("nameplates") ? json["nameplates"] : json["items"];
        if (!itemsVal.isArray()) co_return;
        auto& arr = itemsVal.asArray().unwrap();
        std::vector<ShopItem> items = parseShopItems(arr);
        // Now add to cache
        auto cache = co_await ShopCache.lock();
        if (!cache->contains(page)) {
            (*cache)[page] = std::move(items);
            log::trace("Preloaded shop page {}", page);
        }
        co_return;
    });
}

void RLShopLayer::onResetRubies() {
    //if (rl::getPlayerRubies() <= 0) {
    //    Notification::create("You don't have any rubies to reset!",
    //        NotificationIcon::Warning)
    //        ->show();
    //    return;
    //}
    createQuickPopup("Clear Rubies",
                     "Are you sure you want to <cr>clear your "
                     "rubies</c> and <co>all your brought cosmetics</c>?\n"
                     "<cy>This will clear all your rubies to zero, reset your redeemed codes and all your collected "
                     "rubies but you can reclaim rubies back "
                     "from any completed rated layouts.</c>",
                     "No",
                     "Yes",
                     [this](FLAlertLayer*, bool yes) {
                         if (!yes) return;
                         // clear the data from rubies
                         auto rubyPath = dirs::getModsSaveDir() / Mod::get()->getID() / "rubies_collected.json";

                         if (utils::file::readString(rubyPath)) {
                             auto writeRes = utils::file::writeString(rubyPath, "{}");
                             if (!writeRes) {
                                 log::warn("Failed to clear ruby cache file: {}", rubyPath);
                             }
                         }

                         auto ownedPath = dirs::getModsSaveDir() / Mod::get()->getID() / "owned_items.json";
                         if (utils::file::readString(ownedPath)) {
                             auto writeRes2 = utils::file::writeString(ownedPath, "[]");
                             if (!writeRes2) {
                                 log::warn("Failed to clear owned items file: {}", ownedPath);
                             }
                         }

                         auto redeemedCodesPath = dirs::getModsSaveDir() / Mod::get()->getID() / "redeemed_codes.json";
                         if (utils::file::readString(redeemedCodesPath)) {
                             auto writeRes3 = utils::file::writeString(redeemedCodesPath, "[]");
                             if (!writeRes3) {
                                 log::warn("Failed to clear redeemed codes file: {}", redeemedCodesPath);
                             }
                         }

                         Mod::get()->setSavedValue<int>("selected_nameplate", 0);

                         if (rl::getPlayerRubies() > 0) {
                             rl::setPlayerRubies(0);
                             Notification::create("Rubies have been reset!", NotificationIcon::Info)->show();
                             FMODAudioEngine::sharedEngine()->playEffect("geode.loader/newNotif02.ogg");
                         }
                         m_rubyLabel->setTargetCount(0);
                         m_rubyLabel->updateCounter(0.5f);

                         this->updateShopPage();
                     });
}

void RLShopLayer::onRedeemLayer(CCObject* sender) {
    auto searchLayer = RLSecretLayer1::create();
    auto scene = CCScene::create();
    scene->addChild(searchLayer);
    auto transitionFade = CCTransitionFade::create(0.5f, scene);
    CCDirector::sharedDirector()->pushScene(transitionFade);
}
