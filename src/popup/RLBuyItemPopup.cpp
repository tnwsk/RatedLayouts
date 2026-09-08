#include "popup/RLBuyItemPopup.hpp"
#include "RLDialogIcons.hpp"
#include "RLAchievements.hpp"
#include "RLConstants.hpp"
#include "layer/RLShopLayer.hpp"
#include "utils/Cast.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/LazyNameplate.hpp"
#include "utils/RLArgon.hpp"
#include "custom/RLNameplateItem.hpp"
#include "utils/RandomGen.hpp"
#include "RLRubyUtils.hpp"
#include "ccTypes.h"
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <fmt/format.h>
#include <string>

using namespace geode::prelude;
using namespace rl;

RLBuyItemPopup* RLBuyItemPopup::create(RLNameplateInfo const& info) {
    auto ret = new RLBuyItemPopup();
    if (!ret) return nullptr;
    ret->m_itemId = info.index;
    ret->m_creatorId = info.creatorId;
    ret->m_creatorUsername = info.creatorUsername;
    ret->m_iconUrl = info.iconUrl;
    ret->m_value = info.price;
    ret->m_owner = nullptr;
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

RLBuyItemPopup* RLBuyItemPopup::create(RLNameplateInfo const& info, RLShopLayer* owner) {
    auto ret = new RLBuyItemPopup();
    if (!ret) return nullptr;
    ret->m_itemId = info.index;
    ret->m_creatorId = info.creatorId;
    ret->m_creatorUsername = info.creatorUsername;
    ret->m_iconUrl = info.iconUrl;
    ret->m_value = info.price;
    ret->m_owner = owner;
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

RLBuyItemPopup* RLBuyItemPopup::create(RLNameplateInfo const& info, RLShopLayer2* owner) {
    auto ret = new RLBuyItemPopup();
    if (!ret) return nullptr;
    ret->m_itemId = info.index;
    ret->m_creatorId = info.creatorId;
    ret->m_creatorUsername = info.creatorUsername;
    ret->m_iconUrl = info.iconUrl;
    ret->m_value = info.price;
    ret->m_owner = owner;
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

//RLBuyItemPopup* RLBuyItemPopup::create(int itemId,
//                                       int creatorId,
//                                       const std::string& creatorUsername,
//                                       const std::string& iconUrl,
//                                       int value,
//                                       RLShopLayer* owner) {
//    auto ret = new RLBuyItemPopup();
//    if (!ret) return nullptr;
//    ret->m_itemId = itemId;
//    ret->m_creatorId = creatorId;
//    ret->m_creatorUsername = creatorUsername;
//    ret->m_iconUrl = iconUrl;
//    ret->m_value = value;
//    ret->m_owner = owner;
//    if (ret->init()) {
//        ret->autorelease();
//        return ret;
//    }
//    delete ret;
//    return nullptr;
//}

bool RLBuyItemPopup::init() {
    if (!Popup::init(260.f, 190.f, "GJ_square02.png")) return false;

    bool owned = RLNameplateItem::isOwned(m_itemId);
    setTitle(owned ? "Nameplate Owned" : fmt::format("Buy Nameplate for {} Rubies?", m_value).c_str());

    // kill the close button
    if (auto closeBtn = this->m_closeBtn) {
        closeBtn->removeFromParent();
    }

    // status (top of popup)
    m_statusLabel = CCLabelBMFont::create(owned ? "Owned" : "Not owned", "bigFont.fnt");
    m_statusLabel->setScale(0.45f);
    m_statusLabel->setPosition(
        {m_mainLayer->getScaledContentSize().width / 2, m_mainLayer->getScaledContentSize().height / 2 + 30});
    m_statusLabel->setColor(owned ? ccGREEN : ccRED);
    m_mainLayer->addChild(m_statusLabel);

    // preview icon (banner from server)
    auto* lazy = LazyNameplate::create({m_mainLayer->getScaledContentSize() - CCSize{10, 10}}, m_itemId, true);
    lazy->setAutoResize(true);
    lazy->setPosition(
        {m_mainLayer->getScaledContentSize().width / 2.f, m_mainLayer->getScaledContentSize().height / 2.f + 6.f});
    m_mainLayer->addChild(lazy);

    // creator username (clickable if creatorId > 0)
    auto creatorLabel = CCLabelBMFont::create(fmt::format("By {}", m_creatorUsername).c_str(), "goldFont.fnt");
    creatorLabel->setScale(0.5f);

    if (m_creatorId > 0) {
        auto creatorMenu = CCMenu::create();
        creatorMenu->setPosition({0, 0});

        auto creatorBtn = CCMenuItemSpriteExtra::create(creatorLabel, this, menu_selector(RLBuyItemPopup::onProfile));
        creatorBtn->setPosition(
            {m_mainLayer->getScaledContentSize().width / 2, m_mainLayer->getScaledContentSize().height / 2.f - 30.f});
        creatorBtn->setTag(m_creatorId);

        creatorMenu->addChild(creatorBtn);
        m_mainLayer->addChild(creatorMenu);
    }

    // Buy / Cancel buttons
    auto buyText = owned ? "Apply" : "Buy";
    auto buySpr = ButtonSprite::create(buyText, 60, true, "goldFont.fnt", "GJ_button_01.png", 30.f, 1.f);
    // when owned the button should call onApply, otherwise onBuy
    m_buyBtn = CCMenuItemSpriteExtra::create(
        buySpr, this, owned ? menu_selector(RLBuyItemPopup::onApply) : menu_selector(RLBuyItemPopup::onBuy));
    m_buyBtn->setPosition({m_mainLayer->getContentSize().width / 2.f + 60.f, 25.f});
    m_buttonMenu->addChild(m_buyBtn);

    auto cancelSpr = ButtonSprite::create("Cancel", 60, true, "goldFont.fnt", "GJ_button_06.png", 30.f, 1.f);
    m_cancelBtn = CCMenuItemSpriteExtra::create(cancelSpr, this, menu_selector(RLBuyItemPopup::onCancel));
    m_cancelBtn->setPosition({m_mainLayer->getContentSize().width / 2.f - 60.f, 25.f});
    m_buttonMenu->addChild(m_cancelBtn);

    return true;
}

void RLBuyItemPopup::onApply(CCObject* sender) {
    // validate token/account
    auto token = RLArgon::token();
    if (token.empty()) {
        Notification::create("Argon auth missing", NotificationIcon::Warning)->show();
        return;
    }

    auto upopup = UploadActionPopup::create(nullptr, "Applying Nameplate...");
    upopup->show();

    // build request body
    matjson::Value jsonBody = matjson::Value::object();
    jsonBody["accountId"] = GJAccountManager::get()->m_accountID;
    jsonBody["argonToken"] = token;
    jsonBody["index"] = m_itemId;

    auto req = web::WebRequest();
    req.bodyJSON(jsonBody);

    Ref<RLBuyItemPopup> self = this;
    Ref<UploadActionPopup> popupRef = upopup;
    async::spawn(req.post(std::string(rl::BASE_API_URL) + "/setNameplate"), [self, popupRef](web::WebResponse res) {
        if (!self || !popupRef) return;
        if (!res.ok()) {
            popupRef->showFailMessage("Failed to apply nameplate.");
            return;
        }
        auto jsonRes = res.json();
        if (!jsonRes) {
            popupRef->showFailMessage("Invalid server response.");
            return;
        }
        auto json = jsonRes.unwrap();
        bool success = json["success"].asBool().unwrapOrDefault();
        if (success) {
            // persist selected nameplate locally
            auto CS = CachedSettings::update();
            Mod::get()->setSavedValue<int>("selected_nameplate", self->m_itemId);
            CS->userData.nameplate = self->m_itemId;
            popupRef->showSuccessMessage("Nameplate applied!");

            if (auto* owner = self->m_owner) {
                if (auto* shop2 = dyn_cast<RLShopLayer2>(owner))
                    shop2->updateShopPage();
                else
                    rl::cast<RLShopLayer>(owner)->updateShopPage();
            }
            self->onClose(nullptr);

        } else {
            popupRef->showFailMessage("Failed to apply nameplate.");
        }
    });
}

void RLBuyItemPopup::onBuy(CCObject* sender) {
    int current = rl::getPlayerRubies();
    if (current < m_value) {
        // gen random
        std::string response = "You don't have enough <cr>rubies</c> to buy this item!";
        std::string voiceline = selectRandom("RL_no01.ogg"_spr, "RL_no02.ogg"_spr, "RL_no03.ogg"_spr);
        int v = rl::globalRNG()->generate<int>(1, 9);
        switch (v) {
            default: break;
            case 1: response = "You can't <cg>afford</c> that <cl>item</c>!"; break;
            case 2: response = "You need more <cr>rubies</c> to get this!"; break;
            case 3:
                response = fmt::format("<cg>{}</c>, you need more <cr>rubies</c> to buy this item!",
                                       GJAccountManager::get()->m_username);
                break;
            case 4:
                response =
                    "You don't have enough <cr>rubies</c>! <d150><co>hah you "
                    "broke bud.</c>";
                break;
            case 5:
                response = fmt::format("Yikes! Your just <cr>{} rubies short</c>, can't let you have this.",
                                       m_value - current);
                break;
            case 6: response = "Go play some <cl>layouts</c> to earn more <cr>rubies</c>!"; break;
            case 7: response = "Oh, you short on <cr>rubies</c>? That's unfortunate. womp womp"; break;
            case 8:
                response = fmt::format("<cr>ERROR! INSUFFICIENT RUBIES! GO HOME</c> <cg>{}</c>!",
                                       GJAccountManager::get()->m_username);
        }
        DialogObject* obj = DialogObject::create("ArcticWoof", response.c_str(), 28, 1.f, false, ccWHITE);
        auto dialog = DialogLayer::createDialogLayer(obj, nullptr, 2);
        dialog->addToMainScene();
        dialog->animateInRandomSide();
        rl::setDialogObjectCustomIcon(dialog, "RL_dialogIconAW.png"_spr);

        if (!voiceline.empty())
            FMODAudioEngine::sharedEngine()->playEffect(voiceline);
        return;
    }

    rl::setPlayerRubies(current - m_value);

    // persist ownership to owned_items.json
    if (RLNameplateItem::markOwned(m_itemId)) {
        FMODAudioEngine::sharedEngine()->playEffect("geode.loader/newNotif01.ogg");
        RLAchievements::onReward("misc_ruby");

        // update popup UI to reflect owned state
        if (m_statusLabel) {
            m_statusLabel->setString("Owned");
            m_statusLabel->setColor(ccGREEN);
        }

        // replace Buy button with Apply (now calls onApply)
        if (m_buyBtn) {
            m_buyBtn->removeFromParent();
            auto applySpr = ButtonSprite::create("Apply", 60, true, "goldFont.fnt", "GJ_button_01.png", 30.f, 1.f);
            m_buyBtn = CCMenuItemSpriteExtra::create(applySpr, this, menu_selector(RLBuyItemPopup::onApply));
            m_buyBtn->setPosition({m_mainLayer->getContentSize().width / 2.f + 60.f, 25.f});
            m_buttonMenu->addChild(m_buyBtn);
        }

        if (m_owner) {
            if (auto* shop2 = dyn_cast<RLShopLayer2>(m_owner)) {
                shop2->refreshRubyLabel();
                shop2->updateShopPage();
            } else {
                auto* shop1 = rl::cast<RLShopLayer>(m_owner);
                shop1->refreshRubyLabel();
                shop1->updateShopPage();
            }
        }
    }
}

void RLBuyItemPopup::onCancel(CCObject* sender) { this->onClose(nullptr); }

void RLBuyItemPopup::onProfile(CCObject* sender) {
    // if view profile button pressed
    if (m_creatorId > 0) {
        ProfilePage::create(m_creatorId, false)->show();
    }
}
