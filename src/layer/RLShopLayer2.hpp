#pragma once

#include <vector>
#include "Geode/cocos/cocoa/CCObject.h"
#include <Geode/Geode.hpp>
#include <Geode/binding/BoomScrollLayer.hpp>
#include <Geode/binding/CCCounterLabel.hpp>
#include <Geode/binding/ListButtonPage.hpp>
#include <Geode/utils/async.hpp>
#include <cue/DropdownNode.hpp>
#include <gtl/bit_vector.hpp>
#include "custom/LazyListButtonPageDelegate.hpp"
#include "custom/RLNameplateItem.hpp"

using namespace geode::prelude;

class RLShopLayer2 : public CCLayer, public rl::LazyListButtonPageDelegate {
protected:
    bool init() override;
    void keyBackClicked() override;
    void onExitTransitionDidStart() override;
    void onEnterTransitionDidFinish() override;

public:
    static RLShopLayer2* create();
    static bool shouldEnableShopNav();
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

    // page creation
    cocos2d::CCArray* getItemsForPage(int page) override final;
    void loadingFinished(int page) override final;
    void loadingFailed(int page) override final;
    CCArray* createInitShopPages();
    bool createShopPage(int page); // Returns if new page
    CCLayer* createNewShopPage(int page);

    // pagination
    void moveToPage(int page);
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
    BoomScrollLayer* m_pagesNode = nullptr;
    gtl::bit_vector m_loaded;
    int m_shopPage = 0; // The internal page
    int m_boomPage = 0; // The page for BoomScrollLayer
    async::TaskHolder<bool> m_delayedFetchTask;
};
