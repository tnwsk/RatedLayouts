#include "layer/RLShopLayer2.hpp"
#include "RLConfig.hpp"
#include "RLDialogIcons.hpp"
#include "custom/LazyListButtonPage.hpp"
#include "custom/RLShopkeeperSprite.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/Cast.hpp"
#include "utils/NoHashHasher.hpp"
#include "utils/RLArgon.hpp"
#include "utils/RLData.hpp"
#include "utils/RandomGen.hpp"
#include "utils/ScopedAtomic.hpp"
#include "utils/StartupFunctions.hpp"
#include "popup/RLNameplateSubmitPopup.hpp"
#include "popup/RLBuyItemPopup.hpp"
#include "RLSecretLayer1.hpp"
#include <Geode/Enums.hpp>
#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/modify/BoomScrollLayer.hpp>
#include <arc/future/Join.hpp>
#include <arc/sync/Mutex.hpp>
#include <arc/time/Sleep.hpp>
#include <cue/DropdownNode.hpp>
#include <fmt/format.h>
#include "RLConstants.hpp"
#include "RLRubyUtils.hpp"

using namespace geode::prelude;
using namespace rl;

using PageHasher = NoHashHasher<int>;
using ShopCacheType = std::unordered_map<int, std::vector<RLNameplateInfo>, PageHasher>;

// TODO: Add option to lazily load pages instead of batching at startup.

static arc::Mutex<ShopCacheType> ShopCache;
static std::atomic<int> TotalPages = 1;
static async::TaskHolder<> PrefetchTask;

static std::atomic<bool> DidInit = false;
static std::atomic<bool> TriedPrefetching = false;
static std::atomic<bool> AllPrefetched = false;
static std::atomic<int> FailedTries = {0};
static arc::Mutex<> FetchMtx;

static constexpr int kACTION_TAG = 59597;
static constexpr ccColor4B SELECTED_PAGE_COLOR = {255, 224, 74, 255};
static constexpr ccColor4B INACTIVE_PAGE_COLOR = {209, 201, 146, 200};

namespace rl {
struct ShopBoomScrollLayer : public Modify<ShopBoomScrollLayer, BoomScrollLayer> {
    struct Fields {
        bool m_useCustom = false;
        ccColor4B m_selectedColor = {255, 255, 255, 255};
        ccColor4B m_inactiveColor = {166, 166, 166, 255};
    };

    static void onModify(auto& self) {
        // TODO: Test for incompatibilities
        Result<> res = self.setHookPriorityPost("BoomScrollLayer::updateDots", Priority::Last);
        if (res.isErr()) log::warn("Failed to set updateDots priority: {}", res.unwrapErr());
    }

    void updateDots(float dt);

    void setSelectedColor(ccColor4B col) {
        m_fields->m_useCustom = true;
        m_fields->m_selectedColor = col;
    }
    void setInactiveColor(ccColor4B col) {
        m_fields->m_useCustom = true;
        m_fields->m_inactiveColor = col;
    }

    ccColor4B getSelectedColor() { return m_fields->m_selectedColor; }
    ccColor4B getInactiveColor() { return m_fields->m_inactiveColor; }
};
}  // namespace rl

namespace RL_INTERNAL_NS(rl) {
class RL_INTERNAL_LINKAGE ShopPageLabel final : public CCLabelBMFont {
    WeakRef<BoomScrollLayer> m_boomLayer;
    int m_totalPages = 1;
    int m_page = -999;
    int m_realPage = -1;

public:
    static ShopPageLabel* create(BoomScrollLayer* layer, const char* fmtFile);
    bool init(BoomScrollLayer* layer, const char* fmtFile);
    void updatePageLabel(float dt);
    std::string formatPageLabel() const;
    bool checkPage();
};
}  // namespace RL_INTERNAL_NS(rl)

////////////////////////////////////////////////////////////////////////////////
// RLShopLayer2

bool RLShopLayer2::shouldEnableShopNav() { return DidInit.load(); }

static std::vector<RLNameplateInfo> ParseShopItems(std::vector<matjson::Value> const& arr) {
    std::vector<RLNameplateInfo> items;
    items.reserve(arr.size());
    for (auto& it : arr) {
        // TODO: Add matjson::Serializer
        RLNameplateInfo si;
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

static int UpdateToRealPage(int& page) {
    const auto totalPages = TotalPages.load();
    if (page < 0)
        page = totalPages - 1;
    else if (page >= totalPages)
        page = 0;
    return page;
}

static int GetRealPage(int page) { return UpdateToRealPage(page); }

static arc::Future<bool> PreloadShopPage(int page) {
    UpdateToRealPage(page);
    /*Is cached*/ {
        auto cache = co_await ShopCache.lock();
        if (cache->contains(page)) co_return true;
    }
    log::trace("Preloading shop page {}", page);

    // Set up our map without actually changing anything
    Result<matjson::Value> res = co_await LocalEndpoint::build("getNameplates")
                                     .ibody({{"page", page + 1}, {"amount", 8}})
                                     .expires(18_hours)
                                     .get();
    // Check the results...
    if (res.isErr()) {
        log::trace("Error preloading shop page {}: {}", page, res.unwrapErr());
        co_return false;
    }
    auto json = std::move(res).unwrap();

    // server may return object with nameplates/items array or raw array
    if (!json.isObject()) [[unlikely]] {
        log::trace("Error preloading shop page {}: json is not an object", page);
        co_return false;
    }
    if (auto nPages = json["totalPages"].asInt()) TotalPages = nPages.unwrap();
    auto itemsVal = json.contains("nameplates") ? json["nameplates"] : json["items"];
    if (!itemsVal.isArray()) [[unlikely]] {
        log::trace("Error preloading shop page {}: nameplates/items is not an array", page);
        co_return false;
    }
    auto& arr = itemsVal.asArray().unwrap();
    std::vector<RLNameplateInfo> items = ParseShopItems(arr);

    // Now add to cache
    auto cache = co_await ShopCache.lock();
    if (!cache->contains(page)) {
        (*cache)[page] = std::move(items);
        log::trace("Preloaded shop page {}", page);
    }
    co_return true;
}

void rl::ShopLayer_init() {
    if (DidInit.exchange(true)) return;
    if (FailedTries.load() > 3) return;  // Don't load
    // Fetch now...
    PrefetchTask.spawn([]() -> arc::Future<> {
        auto guard = co_await FetchMtx.lock();
        // Fetch the first page ahead to update TotalPages.
        bool result = co_await PreloadShopPage(0);
        if (co_await PreloadShopPage(0)) {
            FailedTries.store(0);
            log::info("Finished shop page info init");
        } else {
            DidInit.store(false);
            if (FailedTries.fetch_add(1) == 0) log::warn("Failed to prefetch shop page info");
        }
        co_return;
    }, []() {});
}

static arc::Future<int> PrefetchShopPages() {
    int failed = 0;
    int total = TotalPages.load();
    // Fetch the pages next to the first before the others.
    if (!co_await PreloadShopPage(1)) ++failed;
    if (!co_await PreloadShopPage(total - 1)) ++failed;
    // Fetch the rest of the pages.
    for (int Ix = 1; Ix < (total - 1); ++Ix) {
        // Create tasks for each page.
        if (!co_await PreloadShopPage(Ix)) ++failed;
    }
    co_return failed;
}

static arc::Future<bool> PrefetchShopPagesWithExponentialBackoff() {
    // Try the first round
    int failed = co_await PrefetchShopPages();
    if (failed == 0) co_return true;
    // Try in a loop.
    int tries = 0;
    size_t sleepTime = 1;
    while (tries++ < 3) {
        log::warn("Failed to load {} shop pages, retrying...", failed);
        FailedTries.fetch_add(1);
        co_await arc::sleepFor(asp::Duration::fromSecs(sleepTime));
        failed = co_await PrefetchShopPages();
        if (failed == 0) co_return true;
        sleepTime *= 3;
    }
    log::error("Failed to load {} shop pages.", failed);
    co_return false;
}

void rl::ShopLayer_prefetch() {
    if (TriedPrefetching.exchange(true)) return;
    // Try prefetching the rest of the pages
    PrefetchTask.spawn([]() -> arc::Future<> {
        auto guard = co_await FetchMtx.lock();
        if (!DidInit.load()) co_return;
        AllPrefetched = co_await PrefetchShopPagesWithExponentialBackoff();
        co_return;
    }, []() {
        if (AllPrefetched.load()) {
            log::info("Finished prefetching shop items!");
        }
    });
}

static LazyListButtonPage* CreateLazyLayerImpl() {
    CCSize winSize = CCDirector::get()->getWinSize();
    CCPoint pos = {winSize.width / 2, 95};
    float offsetX = 57.5f;
    float offsetY = 47.5f;
    return LazyListButtonPage::create(pos, 4, 2, offsetX, offsetY, 30);
}

static LazyListButtonPage* CreateImmLayer(LazyListButtonPageDelegate* delegate, int page) {
    auto* pageNode = CreateLazyLayerImpl();
    if (!pageNode) return nullptr;
    pageNode->setPage(GetRealPage(page));
    pageNode->setDelegate(delegate);
    pageNode->load();
    return pageNode;
}

static LazyListButtonPage* CreateLazyLayer(LazyListButtonPageDelegate* delegate, int page) {
    auto* pageNode = CreateLazyLayerImpl();
    if (!pageNode) return nullptr;
    pageNode->setPage(GetRealPage(page));
    pageNode->setDelegate(delegate);
    pageNode->setLoadWhenVisible(true);
    return pageNode;
}

CCArray* RLShopLayer2::getItemsForPage(int page) {
    std::vector<RLNameplateInfo> items;
    /*Get items*/ {
        auto cache = ShopCache.blockingLock();
        if (!cache->contains(page)) {
            //log::error("Shop page {} did not load!", page);
            return nullptr;
        }
        items = cache->at(page);
    }

    if (items.empty()) [[unlikely]] {
        log::error("Shop page {} is empty!", page);
        return nullptr;
    }

    // Create the array...
    CCArray* itemsArray = CCArray::createWithCapacity(items.size());
    if (!itemsArray) {
        log::error("Shop page {} failed to create items array!", page);
        return nullptr;
    }

    for (RLNameplateInfo const& s : items) {
        auto* item = RLNameplateItem::create(s, this, menu_selector(RLShopLayer2::onBuyItem));
        item->setTag(s.index);
        itemsArray->addObjectNew(item);  // FIXME
    }

    return itemsArray;
}

void RLShopLayer2::loadingFinished(int page) {
    log::trace("Shop page {} finished loading", page);
    m_loaded.set(page);
}

void RLShopLayer2::loadingFailed(int page) {
    log::trace("Shop page {} failed to load", page);
    m_loaded.set(page);
    // TODO: Notify?
}

CCLayer* RLShopLayer2::createNewShopPage(int page) { return CreateImmLayer(this, page); }

bool RLShopLayer2::createShopPage(int page) {
    if (!m_pagesNode) utils::terminate("BoomScrollLayer was not initialized!");
    // Check if the page needs loading.
    UpdateToRealPage(page);
    if (m_loaded.test(page)) return false;
    // Get the page
    auto* pageLayer = m_pagesNode->getPage(page);
    if (!pageLayer) {
        log::warn("Invalid shop page {}", page);
        return false;
    }
    auto* pageNode = dyn_cast<LazyListButtonPage>(pageLayer);
    if (!pageNode || pageNode->isLoaded()) return false;
    return pageNode->load();
}

CCArray* RLShopLayer2::createInitShopPages() {
    const int totalPages = TotalPages.load();
    auto* pages = CCArray::createWithCapacity(totalPages);
    // Create the first two pages.
    pages->addObject(createNewShopPage(0));
    pages->addObject(createNewShopPage(1));
    // Add empty pages to be filled in later.
    for (int page = 2; page < totalPages - 1; ++page) {
        auto* layer = CreateLazyLayer(this, page);
        pages->addObject(layer);
    }
    // Add the looping page.
    pages->addObject(createNewShopPage(-1));
    return pages;
}

RLShopLayer2* RLShopLayer2::create() {
    auto* layer = new RLShopLayer2();
    if (layer && layer->init()) {
        layer->autorelease();
        return layer;
    }
    delete layer;
    return nullptr;
}

// TODO: Set up
bool RLShopLayer2::init() {
    if (!CCLayer::init()) return false;
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    addBackButton(this, BackButtonStyle::Pink);

    m_loaded.resize(TotalPages.load());

    CCArray* initPages = createInitShopPages();
    m_pagesNode = ShopBoomScrollLayer::create(initPages, 0, true);
    m_pagesNode->instantMoveToPage(0);
    m_pagesNode->updatePages();
    m_pagesNode->setZOrder(100);
    m_pagesNode->setID("rl-shop-pages");

    // desk
    auto* deskSpr = CCSprite::createWithSpriteFrameName("RL_storeDesk.png"_spr);
    deskSpr->setPosition({winSize.width / 2, 95});
    deskSpr->setID("rl-shop-nav");
    this->addChild(deskSpr);

    //1854, 778
    //1688, 634

    if (auto* clipSpr = CCSprite::createWithSpriteFrameName("RL_storeDeskBG.png"_spr)) {
        //clipSpr->setScaleX(1688.f / 1854.f);
        clipSpr->setScaleY(778.f / 634.f);
        CCPoint sprPos = {winSize.width / 2, 95};
        auto* clip = CCClippingNode::create(clipSpr);
        clip->setPosition(sprPos);
        m_pagesNode->setPosition(-sprPos);
        clip->setAlphaThreshold(0.01f);
        clip->setZOrder(100);
        clip->setID("rl-shop-pages-clip");
        clip->addChild(m_pagesNode);
        addChild(clip);
    } else {
        addChild(m_pagesNode);
    }

    // bg
    auto* shopBGSpr = CCSprite::createWithSpriteFrameName("RL_shopBG.png"_spr);
    auto bgSize = shopBGSpr->getTextureRect().size;

    shopBGSpr->setAnchorPoint({0.0f, 0.0f});
    shopBGSpr->setScaleX((winSize.width + 10.0f) / bgSize.width);
    shopBGSpr->setScaleY((winSize.height + 10.0f) / bgSize.height);
    shopBGSpr->setPosition({-5.0f, -5.0f});
    this->addChild(shopBGSpr, -3);

    // ruby counter
    auto* rubySpr = CCSprite::createWithSpriteFrameName("RL_rubiesIcon.png"_spr);
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
    auto* shopkeeperIcon = RLShopkeeperSprite::create(true);
    shopkeeperIcon->setScale(2.f);

    m_shopkeeper = CCMenuItemSpriteExtra::create(
        shopkeeperIcon, this, menu_selector(RLShopLayer2::onShopkeeper));
    m_shopkeeper->setPosition({winSize.width / 2 - 120, deskSpr->getContentHeight()});
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
    plushiesSpr->setPosition({winSize.width / 2 - 10, deskSpr->getContentHeight()});
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
    auto redeemBtn =
        CCMenuItemSpriteExtra::create(orcaleSpr, this, menu_selector(RLShopLayer2::onRedeemLayer));
    redeemBtn->setPosition({20, 25});
    menu->addChild(redeemBtn);

    // pagination controls
    auto* dots = m_pagesNode->getChildByType<CCSpriteBatchNode>(0);
    const bool usePageLabel = CachedSettings::get()->usePageLabelInShop || !dots;
    if (!usePageLabel) {
        if (auto* pages = dyn_cast<ShopBoomScrollLayer>(m_pagesNode)) {
            pages->setSelectedColor(SELECTED_PAGE_COLOR);
            pages->setInactiveColor(INACTIVE_PAGE_COLOR);
        }
        // Hide the dots, use page number instead.
        dots->setPositionY(-54.f);
        m_pagesNode->setDotScale(.9f);
        m_pagesNode->updateDots(0.05f);
    } else {
        m_pageLabel = ShopPageLabel::create(m_pagesNode, "goldFont.fnt");
        //if (dots) dots->setVisible(false);
        m_pagesNode->togglePageIndicators(/*visible=*/false);
        if (m_pageLabel) {
            m_pageLabel->setScale(0.5f);
            m_pageLabel->setPosition({deskSpr->getContentWidth() / 2.f, 4.f});
            m_pageLabel->setAnchorPoint({0.5f, 0.f});
            deskSpr->addChild(m_pageLabel, 2);
        }
    }

    // pagination menu thingy
    auto pageMenu = CCMenu::create();
    pageMenu->setPosition({0, 0});
    pageMenu->setContentSize(deskSpr->getContentSize());
    pageMenu->setID("rl-shop-buttons");
    deskSpr->addChild(pageMenu, 2);

    auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    if (prevSpr) {
        m_prevPageBtn =
            CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(RLShopLayer2::onPrevPage));
        if (m_prevPageBtn) {
            m_prevPageBtn->setPosition({-10, pageMenu->getContentSize().height / 2});
            pageMenu->addChild(m_prevPageBtn);
        }
    }

    auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    nextSpr->setFlipX(true);
    if (nextSpr) {
        m_nextPageBtn =
            CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(RLShopLayer2::onNextPage));
        if (m_nextPageBtn) {
            m_nextPageBtn->setPosition(
                {pageMenu->getContentSize().width + 10, pageMenu->getContentSize().height / 2});
            pageMenu->addChild(m_nextPageBtn);
        }
    }

    if (!AllPrefetched.load()) {
        WeakRef<RLShopLayer2> weak = this;
        m_delayedFetchTask.spawn([] -> arc::Future<bool> {
            [[maybe_unused]] auto guard = co_await FetchMtx.lock();
            co_return AllPrefetched.load();
        }, [weak](bool success) {
            if (!success) return;
            if (auto self = weak.lock()) {
                Notification::create("Some shop pages failed to load", NotificationIcon::Warning)
                    ->show();
            }
        });
    }

    //shopMenu->updateLayout();
    //deskSpr->addChild(shopMenu);
    this->setKeypadEnabled(true);
    return true;
}

void RLShopLayer2::updateShopPage() {
    // TODO
}

void RLShopLayer2::performDropdownAction() {
    m_pendingDropdownAction = 0;
    this->initDropdownMenu();
}

void RLShopLayer2::initDropdownMenu() {
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
            this->runAction(CCSequence::create(
                CCDelayTime::create(0.01f),
                CCCallFunc::create(this, callfunc_selector(RLShopLayer2::performDropdownAction)),
                nullptr));
        }
    });
}

void RLShopLayer2::onForm() {
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
void RLShopLayer2::onEnterTransitionDidFinish() {
    CCLayer::onEnterTransitionDidFinish();
    FMODAudioEngine::sharedEngine()->playMusic("rubyShop.mp3"_spr, true, 0.f, 0);
    refreshRubyLabel();
}

void RLShopLayer2::onExitTransitionDidStart() {
    CCLayer::onExitTransitionDidStart();
    GameManager::sharedState()->playMenuMusic();
}

void RLShopLayer2::onSubmitNameplate() { RLNameplateSubmitPopup::create()->show(); }

static void StopAllActions(CCNode* node) {
    if (!node) return;
    CCAction* action = nullptr;
    while ((action = node->getActionByTag(kACTION_TAG))) {
        action->update(1.f);
        node->stopAction(action);
    }
}

void RLShopLayer2::onShopkeeper(CCObject* sender) {
    if (rl::globalRNG()->generate<int>(0, 3) != 0) return this->onShopkeeperDialog(sender);
    // Make the shopkeeper jig
    constexpr float kTime = 0.06;
    constexpr float kFactor = 2.f;
    auto* action = CCSequence::create(CCSkewTo::create(kTime, 1.f + kFactor, 1.f),
                                      CCSkewTo::create(kTime * 2, 1.f - kFactor, 1.f),
                                      CCEaseSineOut::create(CCSkewTo::create(kTime * 2, 1.f, 1.f)),
                                      nullptr);
    action->setTag(kACTION_TAG);
    // Add the action.
    StopAllActions(m_shopkeeper);
    m_shopkeeper->runAction(action);

    auto sfx = fmt::format("grunt{:02}.ogg", rl::globalRNG()->generate<int>(1, 4));
    FMODAudioEngine::sharedEngine()->playEffect(sfx);
}

void RLShopLayer2::onShopkeeperDialog(CCObject* sender) {
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
    case 1: response = "I got all of the <cg>nameplates</c> in stock!"; break;
    case 2:
        response =
            "<cg>Layout Creator</c>? <cl>Well, he kind of ran away when I arrived</c>"
            ", odd fella but oh well...";
        break;
    case 3:
        response =
            "The plushies are <cr>NOT FOR SALE</c>. I just keep them here "
            "because they are <cp>cute.</c>";
        voiceline = selectRandom("RL_no01.ogg"_spr, "RL_no02.ogg"_spr, "RL_no03.ogg"_spr);
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
        response =
            "Cooking some <cg>new nameplates</c> for you all! <cl>Can't wait for you to see "
            "them</c>!";
        break;
    case 10: response = "I'm <cr>lurking</c> on <cl>every move</c> you do..."; break;
    case 11:
        response = "<cg>Fun fact about me!</c> I actually <co>suck at making gameplay</c>.";
        break;
    case 12:
    case 13: response = "Ask <cp>The Oracle</c> about <cf>me</c>! That would be funny."; break;
    case 14:
        response =
            "Would you like to buy my entire shop for <cr>100k Rubies?</c> I know someone is "
            "<cy>interested</c> :P";
        break;
    case 15:
    case 16:
    case 17:
        response =
            "I heard there's <cf>The Spire</c> nearby, but I don't know how to get in there...";
        break;
    case 18:
        response =
            "I'm thinking of <cr><s100>burning</s></c> down this shop... <d100> <cy>just "
            "kidding!</c> <d100> "
            "<co>maybe...</c>";
        voiceline = selectRandom("RL_laugh01.ogg"_spr, "RL_fire01.ogg"_spr);
        break;
    case 19:
    case 20:
        response = "Wow <cl>Rated Layouts</c> updated after months... thats <co>shocking</c>...";
        break;
    default:
        response = "<cg>Weh!</c>";
        voiceline = "RL_huh01.ogg"_spr;
        break;
    }
    dialogObj = DialogObject::create("ArcticWoof", response.c_str(), 1, 1.f, false, ccWHITE);

    auto dialog = DialogLayer::createDialogLayer(dialogObj, nullptr, 2);
    dialog->addToMainScene();
    dialog->animateInRandomSide();

    //rl::setDialogObjectCustomIcon(dialog, RLShopkeeperSprite::create());
    rl::setDialogObjectCustomIcon(dialog, "RL_dialogIconAW.png"_spr);

    if (voiceline.empty())
        voiceline = selectRandom("RL_huh01.ogg"_spr, "RL_huh03.ogg"_spr, "RL_huh04.ogg"_spr);
    if (!voiceline.empty()) FMODAudioEngine::sharedEngine()->playEffect(voiceline);
}

void RLShopLayer2::onBuyItem(CCObject* sender) {
    auto item = static_cast<CCMenuItemSpriteExtra*>(sender);
    int idx = item->getTag();
    RLNameplateInfo info;
    if (!RLNameplateItem::getInfo(idx, &info)) {
        log::warn("RLShopLayer2: no nameplate info for index {}", idx);
        return;
    }

    // open buy popup with creator/price information
    RLBuyItemPopup::create(info, this)->show();
}

void RLShopLayer2::onUnequipNameplate() {
    createQuickPopup("Unequip Nameplate",
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
        Ref<RLShopLayer2> self = this;
        async::spawn(req.post(std::string(rl::BASE_API_URL) + "/setNameplate"),
                     [self, popupRef](web::WebResponse res) {
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
                popupRef->showFailMessage(
                    json["message"].asString().unwrapOr("Failed to unequip nameplate."));
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

void RLShopLayer2::refreshRubyLabel() {
    if (!m_rubyLabel) return;
    int val = rl::getPlayerRubies();
    m_rubyLabel->setTargetCount(val);
    m_rubyLabel->updateCounter(0.25f);
}

void RLShopLayer2::moveToPage(int page) {
    // taken from levelselectlayer
    if (m_pagesNode->m_pageMoving) {
        m_pagesNode->m_pageMoving = false;
        m_pagesNode->m_extendedLayer->stopActionByTag(2);
        m_pagesNode->m_extendedLayer->setPosition(m_pagesNode->m_position);
        m_pagesNode->moveToPageEnded();
    }
    m_pagesNode->moveToPage(m_boomPage);
}

void RLShopLayer2::onPrevPage(CCObject* sender) {
    m_boomPage = m_pagesNode->m_page - 1;
    UpdateToRealPage(--m_shopPage);
    createShopPage(m_shopPage);
    createShopPage(m_shopPage - 1);
    m_pagesNode->moveToPage(m_boomPage);
}

void RLShopLayer2::onNextPage(CCObject* sender) {
    m_boomPage = m_pagesNode->m_page + 1;
    UpdateToRealPage(++m_shopPage);
    createShopPage(m_shopPage);
    createShopPage(m_shopPage + 1);
    m_pagesNode->moveToPage(m_boomPage);
}

void RLShopLayer2::keyBackClicked() {
    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
}

void RLShopLayer2::onResetRubies() {
    //if (rl::getPlayerRubies() <= 0) {
    //    Notification::create("You don't have any rubies to reset!",
    //        NotificationIcon::Warning)
    //        ->show();
    //    return;
    //}
    createQuickPopup("Clear Rubies",
                     "Are you sure you want to <cr>clear your "
                     "rubies</c> and <co>all your brought cosmetics</c>?\n"
                     "<cy>This will clear all your rubies to zero, reset your redeemed codes and "
                     "all your collected "
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

        auto redeemedCodesPath =
            dirs::getModsSaveDir() / Mod::get()->getID() / "redeemed_codes.json";
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

void RLShopLayer2::onRedeemLayer(CCObject* sender) {
    auto searchLayer = RLSecretLayer1::create();
    auto scene = CCScene::create();
    scene->addChild(searchLayer);
    auto transitionFade = CCTransitionFade::create(0.5f, scene);
    CCDirector::sharedDirector()->pushScene(transitionFade);
}

////////////////////////////////////////////////////////////////////////////////
// ShopBoomScrollLayer

void ShopBoomScrollLayer::updateDots(float dt) {
    BoomScrollLayer::updateDots(dt);
    if (!m_fields->m_useCustom) return;
    if (!BoomScrollLayer::m_dots) return;

    int pageNum = pageNumberForPosition(m_extendedLayer->getPosition());
    if (m_looped) pageNum = getRelativePageForNum(pageNum);

    const int totalPages = BoomScrollLayer::getTotalPages();
    auto dots = BoomScrollLayer::m_dots->asExt<CCSprite>();
    for (int Ix = 0; Ix < totalPages; ++Ix) {
        if (size_t(Ix) >= dots.size()) break;
        // Update the color...
        ccColor4B dotColor = m_fields->m_inactiveColor;
        if (Ix == pageNum) [[unlikely]]
            dotColor = m_fields->m_selectedColor;
        dots[Ix]->setColor(to3B(dotColor));
        dots[Ix]->setOpacity(dotColor.a);
        // TODO: Make dots closest to the active one bigger
    }
}

////////////////////////////////////////////////////////////////////////////////
// ShopPageLabel

ShopPageLabel* ShopPageLabel::create(BoomScrollLayer* layer, const char* fmtFile) {
    if (!layer) return nullptr;
    auto* label = new ShopPageLabel();
    if (label && label->init(layer, fmtFile)) {
        label->autorelease();
        return label;
    }
    delete label;
    return nullptr;
}

bool ShopPageLabel::init(BoomScrollLayer* layer, const char* fmtFile) {
    if (!layer) return false;
    if (!CCLabelBMFont::initWithString("", fmtFile)) return false;

    // Update the initial page label.
    this->m_boomLayer = layer;
    this->m_totalPages = TotalPages.load();
    this->updatePageLabel(0.f);

    unscheduleUpdate();
    schedule(schedule_selector(ShopPageLabel::updatePageLabel), 0.1f);
    return true;
}

void ShopPageLabel::updatePageLabel(float dt) {
    if (checkPage()) {
        std::string pageLabel = formatPageLabel();
        CCLabelBMFont::setString(pageLabel.c_str());
        CCLabelBMFont::updateLabel();
    }
}

std::string ShopPageLabel::formatPageLabel() const {
    return fmt::format("{}/{}", m_realPage + 1, m_totalPages);
}

bool ShopPageLabel::checkPage() {
    if (auto boomLayer = m_boomLayer.lock()) {
        if (m_page == boomLayer->m_page) return false;
        // Update page numbers.
        this->m_page = boomLayer->m_page;
        this->m_realPage = boomLayer->getRelativePageForNum(m_page);
        return true;
    }
    // Disable everything...
    CCLabelBMFont::setString("");
    unscheduleUpdate();
    return false;
}
