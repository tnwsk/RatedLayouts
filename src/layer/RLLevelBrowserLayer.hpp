#pragma once

#include <Geode/Geode.hpp>
#include <Geode/modify/GameLevelManager.hpp>
#include "Geode/cocos/label_nodes/CCLabelBMFont.h"
#include <cue/ListNode.hpp>
#include <matjson.hpp>

using namespace geode::prelude;

class RLLevelBrowserLayer : public CCLayer, public LevelManagerDelegate, public SetIDPopupDelegate {
public:
    enum class Mode {
        Featured = 1,
        Sent = 2,
        AdminSent = 3,
        Search = 4,
        Account = 5,
        EventSafe = 6,
        LegendarySends = 7
    };

    using ParamList = std::vector<std::pair<std::string, std::string>>;

    static RLLevelBrowserLayer* create(Mode mode,
                                       ParamList const& params = ParamList(),
                                       std::string const& title = "Rated Layouts");
    bool init(GJSearchObject* object);
    void keyBackClicked() override;

    void loadLevelsFinished(CCArray* levels, char const* key, int p2) override;
    void loadLevelsFailed(char const* key, int p1) override;

    void refreshLevels(bool force);
    void startLoading();
    void stopLoading();

    virtual void onEnter() override;
    virtual void onExit() override;
    virtual void update(float dt) override;

    void performSearchQuery(ParamList const& params);

protected:
    GJSearchObject* m_searchObject = nullptr;
    std::string m_title;
    int m_totalLevels = 0;

    cue::ListNode* m_listNode = nullptr;
    ScrollLayer* m_scrollLayer = nullptr;
    bool m_loading = false;
    bool m_needsLayout = false;

    CCLabelBMFont* m_levelsLabel = nullptr;
    CCLabelBMFont* m_titleLabel = nullptr;
    LoadingSpinner* m_spinner = nullptr;
    CCMenuItemSpriteExtra* m_prevButton = nullptr;
    CCMenuItemSpriteExtra* m_nextButton = nullptr;
    CCMenuItemSpriteExtra* m_refreshBtn = nullptr;

    // TODO: Make this actually do something?
    std::unordered_map<long long, GJGameLevel*> m_levelCache;

    // compact mode toggle
    bool m_compactMode = false;
    CCMenuItemSpriteExtra* m_compactToggleBtn = nullptr;
    CCLabelBMFont* m_compactToggleLabel = nullptr;

    bool m_filterClassic = false;
    bool m_filterPlat = false;
    CCMenuItemSpriteExtra* m_classicBtn = nullptr;
    CCMenuItemSpriteExtra* m_planetBtn = nullptr;
    CCMenuItemSpriteExtra* m_deleteBtnClassic = nullptr;
    CCMenuItemSpriteExtra* m_deleteBtnPlat = nullptr;
    bool m_filterButtonUpdating = false;

    int m_page = 0;
    int m_totalPages = 1;

    Mode m_mode = Mode::Featured;
    ParamList m_modeParams;
    async::TaskHolder<Result<matjson::Value>> m_searchTask;
    async::TaskHolder<web::WebResponse> m_searchTask2;
    async::TaskHolder<web::WebResponse> m_deleteAllSendsTask;

    ~RLLevelBrowserLayer() {
        m_searchTask.cancel();
        m_searchTask2.cancel();
        m_deleteAllSendsTask.cancel();
        auto* glm = GameLevelManager::get();
        if (glm && glm->m_levelManagerDelegate == this) {
            glm->m_levelManagerDelegate = nullptr;
        }
    }

    // UI: tabs and search input
    TabButton* m_featuredTab = nullptr;
    TabButton* m_sentTab = nullptr;
    TabButton* m_searchTab = nullptr;

    CCMenu* m_searchInputMenu = nullptr;
    geode::TextInput* m_searchInput = nullptr;
    CCMenuItemSpriteExtra* m_searchButton = nullptr;
    CCMenuItemSpriteExtra* m_clearButton = nullptr;

    CCNode* m_bgContainer = nullptr;
    CCNode* m_groundContainer = nullptr;
    std::vector<CCSprite*> m_bgTiles;
    std::vector<CCSprite*> m_groundTiles;
    float m_bgSpeed = 40.f;
    float m_groundSpeed = 150.f;

    // page picker UI
    CCMenuItemSpriteExtra* m_pageButton = nullptr;
    CCLabelBMFont* m_pageButtonLabel = nullptr;

    void prepareForSearch() {
        m_searchTask.cancel();
        m_searchTask2.cancel();
        m_levelCache.clear();
        this->startLoading();
    }

    // helpers
    void populateFromArray(CCArray* levels);
    void fetchLevelsForType(int type);
    void fetchAccountLevels(int accountId);
    void updatePageButton();

    void setupBackground();
    void setupControls(CCMenu* uiMenu);
    void applyModeFetch(bool force);
    int computeModeType() const;
    int parseModeParam(int fallback) const;

    void updatePagingFromJson(matjson::Value const& json);
    std::string extractLevelIDs(matjson::Value const& json) const;
    void processFetchedLevelIDs(matjson::Value const& json, std::string const& emptyMessage);
    void processFetchedLevelIDs(std::string const& levelIDs, std::string const& emptyMessage);

    void presentSearchResults(web::WebResponse const& res);
    void presentEventSafeResults(web::WebResponse const& res);
    LevelCell* createLevelCell(GJGameLevel* level, int index, float cellH);
    void updateLevelsLabel(int returned);

    // UI handlers
    void onPrevPage(CCObject* sender);
    void onNextPage(CCObject* sender);
    void onRefresh(CCObject* sender);
    void onModeButton(CCObject* sender);
    void onSearchButton(CCObject* sender);
    void onClearButton(CCObject* sender);
    void onPageButton(CCObject* sender);
    void onInfoButton(CCObject* sender);
    void onCompactToggle(CCObject* sender);

    // filter button callbacks
    void onClassicFilter(CCObject* sender);
    void onPlanetFilter(CCObject* sender);
    void onDeleteFilter(CCObject* sender);
    void updateFilterButtons();

    // SetIDPopup delegate
    void setIDPopupClosed(SetIDPopup* popup, int value) override;
};
