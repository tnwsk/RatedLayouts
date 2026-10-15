#include <Geode/Geode.hpp>
#include <Geode/modify/LevelSearchLayer.hpp>

#include "RLNetworkUtils.hpp"
#include "layer/RLSearchLayer.hpp"
#include "layer/RLMenuLayer.hpp"
#include "utils/CachedSettings.hpp"

using namespace geode::prelude;
using namespace rl;

class $modify(RLLevelSearchLayer, LevelSearchLayer) {
    bool init(int type) {
        if (!LevelSearchLayer::init(type))
            return false;

        // add a menu on the left side of the layer
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto bottomLeftMenu =
            static_cast<CCMenu*>(this->getChildByID("bottom-left-menu"));
        auto lbButtonSpr = CCSprite::createWithSpriteFrameName("RL_btn01.png"_spr);
        lbButtonSpr->setScale(1.2f);
        auto lbButton = CCMenuItemSpriteExtra::create(
            lbButtonSpr, this, menu_selector(RLLevelSearchLayer::onRatedLayoutLayer));
        bottomLeftMenu->addChild(lbButton);
        bottomLeftMenu->updateLayout();
        return true;
    }
    void onRatedLayoutLayer(CCObject* sender) {
        if (GJAccountManager::sharedState()->m_accountID == 0) {
            FLAlertLayer::create(
                "Rated Maps",
                "You must be <cg>logged in</c> to access this feature in <cl>Rated Maps.</c>",
                "OK")
                ->show();
            return;
        }

        if (rl::isGDPS()) {
            FLAlertLayer::create(
                "Rated Maps",
                "This feature is not available on <cy>GDPS servers.</c>",
                "OK")
                ->show();
            return;
        }
        bool disableSearch = CachedSettings::get()->disableRLSearchLayer;  // please work no. 2
        CCLayer* layer = nullptr;
        if (disableSearch) {
            layer = RLMenuLayer::create();
        } else {
            layer = RLSearchLayer::create();
        }
        if (layer) {
            auto scene = CCScene::create();
            scene->addChild(layer);
            auto transitionFade = CCTransitionFade::create(0.5f, scene);
            CCDirector::sharedDirector()->pushScene(transitionFade);
        }
    }
};
