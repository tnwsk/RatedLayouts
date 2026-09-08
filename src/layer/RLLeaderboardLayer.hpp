#pragma once

#include <array>
#include <atomic>
#include <span>
#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

class RLLeaderboardLayer : public CCLayer {
public:
    static constexpr int kFetchSize = 100;

protected:
    cue::ListNode* m_userListNode = nullptr;
    ScrollLayer* m_scrollLayer = nullptr;
    // FIXME: Pad elements for seamless scrollbar
    Scrollbar* m_scrollBar = nullptr;
    LoadingSpinner* m_spinner = nullptr;
    TabButton* m_starsTab = nullptr;
    TabButton* m_planetsTab = nullptr;
    TabButton* m_creatorTab = nullptr;
    TabButton* m_coinsTab = nullptr;
    TabButton* m_votesTab = nullptr;
    CCMenuItemSpriteExtra* m_creatorTypeToggleBtn = nullptr;
    bool m_creatorType6 = false;
    CCMenuItemSpriteExtra* m_refreshBtn = nullptr;
    CCMenuItemSpriteExtra* m_accountRefreshBtn = nullptr;
    int m_currentAccountID = 0;

    bool init() override;
    void keyBackClicked() override;
    void onLeaderboardTypeButton(CCObject* sender);
    void onCreatorTypeToggle(CCObject* sender);
    void onAccountClicked(CCObject* sender);
    void onAccountRefreshButton(CCObject* sender);
    void fetchLeaderboard(int type, int = kFetchSize);
    void populateLeaderboardStaggered(std::vector<matjson::Value> users, unsigned by = 10);
    bool populateLeaderboard(std::span<matjson::Value> users, int rank = 1);
    void onInfoButton(CCObject* sender);
    void onRefreshButton(CCObject* sender);

    void addSpinner(bool updateLayout = false);
    void removeSpinner(bool updateLayout = false);
    void refresh(CCObject* sender, int type);

    geode::async::TaskHolder<geode::utils::web::WebResponse> m_fetchTask;
    std::atomic<bool> m_isFetchingRemote = false;
    std::array<bool, 6> m_alreadyFetched = {};  // TODO

private:
    template <bool ClearElts>
    inline bool populateLeaderboardImpl(std::span<matjson::Value> users, int rank = 1);

    void setUpdates(bool state) {
        if (m_userListNode) {
            // TODO: Fix disabling mouse input on load...
            //m_userListNode->setMouseEnabled(state);
            //m_userListNode->getScrollLayer()->enableScrollWheel(state);
            //m_userListNode->setAutoUpdate(state);
            if (state) {
                m_userListNode->updateLayout();
                m_userListNode->scrollToTop();
            }
        }
        if (m_scrollBar) m_scrollBar->setVisible(state);
    }

    geode::Function<void()> m_refreshFn = nullptr;

public:
    static RLLeaderboardLayer* create();
};
