#pragma once

#include <Geode/Geode.hpp>
#include <Geode/Result.hpp>
#include <Geode/loader/Event.hpp>
#include <Geode/utils/async.hpp>
#include <matjson.hpp>

using namespace geode::prelude;

class RLMenuLayer : public CCLayer {
protected:
    bool init() override;
    void keyBackClicked() override;
    void onEnterTransitionDidFinish() override;

    void onLeaderboard(CCObject* sender);
    void onFeaturedLayouts(CCObject* sender);
    void onNewRated(CCObject* sender);
    void onSentLayouts(CCObject* sender);
    void onInfoButton(CCObject* sender);
    void onCollapseInfoButton(CCObject* sender);
    void onDailyLayouts(CCObject* sender);
    void onWeeklyLayouts(CCObject* sender);
    void onMonthlyLayouts(CCObject* sender);
    void onUnknownButton(CCObject* sender);
    void onLayoutGauntlets(CCObject* sender);
    void onLayoutSpire(CCObject* sender);
    void onAnnouncementButton(CCObject* sender);
    void onSearchLayouts(CCObject* sender);
    void onAchievementsButton(CCObject* sender);
    void onCreditsButton(CCObject* sender);
    void onDemonListButton(CCObject* sender);
    void onShopButton(CCObject* sender);
    void onShopButton2(CCObject* sender);
    void onShopButton2Fail(CCObject* sender);
    void onSettingsButton(CCObject* sender);
    void onDiscordButton(CCObject* sender);
    void onBrowserButton(CCObject* sender);
    void onSecretDialogueButton(CCObject* sender);
    void onSupporterButton(CCObject* sender);
    void onQueueButton(CCObject* sender);
    void showReadGuidePopup();
    bool isGDServerOnline();
    void setGDServerOnline(bool online);

    // news / announcement UI
    CCMenuItemSpriteExtra* m_newsIconBtn = nullptr;
    CCSprite* m_newsBadge = nullptr;

    // labels for mod info
    CCLabelBMFont* m_modStatusLabel = nullptr;
    CCLabelBMFont* m_gdServerLabel = nullptr;
    CCLabelBMFont* m_modVersionLabel = nullptr;
    CCNode* m_modInfoBg = nullptr;
    bool m_modInfoCollapsed = false;

    int m_indexDia = 0;

    //geode::async::TaskHolder<geode::utils::web::WebResponse> m_announcementTask;
    //geode::async::TaskHolder<Result<matjson::Value>> m_dialogueTask;
    geode::async::TaskHolder<Result<matjson::Value>> m_announcementTask;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_dialogueTask;
    geode::async::TaskHolder<geode::utils::web::WebResponse> m_gdServerTask;

    /// Creates listeners for updated settings, as the background is visible on the main menu.
    void setupBGAndListeners();
    /// Removes background, if it exists.
    inline void removeBackground() {
        if (m_background) {
            m_background->removeFromParent();
            m_background = nullptr;
        }
    }

    cocos2d::CCSprite* m_background = nullptr;
    int m_backgroundType = 0;
    bool m_disableBackground = false;

    geode::comm::ListenerHandle m_disableBGListener;
    geode::comm::ListenerHandle m_rgbBGListener;
    geode::comm::ListenerHandle m_BGTypeListener;

public:
    void onEnter() override;
    static RLMenuLayer* create();
};
