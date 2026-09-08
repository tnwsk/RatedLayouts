#pragma once

#include <vector>
#include "Geode/cocos/cocoa/CCObject.h"
#include <Geode/Geode.hpp>
#include <Geode/binding/CCCounterLabel.hpp>
#include <cue/DropdownNode.hpp>
#include "custom/RLNameplateItem.hpp"
#include "layer/RLShopLayer2.hpp"

using namespace geode::prelude;

class RLShopLayer : public CCLayer {
protected:
    bool init() override;
    void keyBackClicked() override;
    void onExitTransitionDidStart() override;
    void onEnterTransitionDidFinish() override;

public:
    using ShopItem = rl::RLNameplateInfo;
    static RLShopLayer* create();
    void updateShopPage();
    void refreshRubyLabel();

private:
    void onResetRubies();
    void onUnequipNameplate();
    void onSubmitNameplate();
    void onForm();

    void onShopkeeper(CCObject* sender);
    void onShopkeeperDialog(CCObject* sender);
    void onBuyItem(CCObject* sender);
    void onRedeemLayer(CCObject* sender);

    void initDropdownMenu();
    void performDropdownAction();

    // pagination
    void onPrevPage(CCObject* sender);
    void onNextPage(CCObject* sender);

    // UI state
    CCCounterLabel* m_rubyLabel;
    CCMenu* m_shopRow1 = nullptr;
    CCMenu* m_shopRow2 = nullptr;
    CCMenuItemSpriteExtra* m_prevPageBtn = nullptr;
    CCMenuItemSpriteExtra* m_nextPageBtn = nullptr;
    CCLabelBMFont* m_pageLabel = nullptr;
    cue::DropdownNode* m_dropdownMenu = nullptr;
    int m_pendingDropdownAction = 0;

    // Shopkeeper
    CCMenuItemSpriteExtra* m_shopkeeper = nullptr;

    // data
    std::vector<ShopItem> m_shopItems;
    int m_shopPage = 0;  // zero-based

    // banner from server now
    void loadShopPage(int page);
    void preloadShopPage(int page);
};
