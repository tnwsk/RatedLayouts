#include "PeekSettingsPopup.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/utils/Keyboard.hpp>
//#include <asp/collections/SmallVec.hpp>
#include <alphalaneous.alphas-ui-pack/include/Utils.hpp>
#include <alphalaneous.alphas-ui-pack/include/nodes/RenderNode.hpp>
#include "utils/CachedSettings.hpp"
#include "utils/ConstexprMap.hpp"
#include "utils/Listeners.hpp"
#include "utils/ProxyNode.hpp"

#include <Geode/utils/casts.hpp>

using namespace geode::prelude;
using namespace rl;

using alpha::ui::RenderNode;

#define QUERY_WITH(RET, ...)                             \
    ({                                                   \
        auto _local_n = (__VA_ARGS__);                   \
        if (!_local_n) {                                 \
            log::warn("Query failed: {}", #__VA_ARGS__); \
            return (RET);                                \
        }                                                \
        _local_n;                                        \
    })

#define QUERY(...) QUERY_WITH(false, __VA_ARGS__)

namespace {
enum BackgroundSetting : int {
    rgbBackground = 0,
    backgroundType = 1,
    disableBackground = 2,
};
enum class Transition : uint8_t {
    SHOW = 0,
    HIDE = 1,
    MISC = 2,
};
}  // namespace

static constexpr auto ElementMap =
    rl::cxpr_hash_map<rl::CString, BackgroundSetting>({{"rgbBackground", rgbBackground},
                                                       {"backgroundType", backgroundType},
                                                       {"disableBackground", disableBackground}});

static bool isPointInsideNode(WeakRef<CCNode> weak, CCPoint const& point) {
    if (auto node = weak.lock())
        return alpha::utils::isPointInsideNode(node.data(), point);
    else
        return true;
}
static bool isPointInsideNode(Ref<CCNode> node, CCPoint const& point) {
    return alpha::utils::isPointInsideNode(node.data(), point);
}

class PeekSettingsPopup::Impl final : public cocos2d::CCNode {
    static constexpr int kACTION_TAG = 7373;
    static constexpr float kACTION_TIME = 2.f;

    PeekSettingsPopup::Callback m_callback;
    WeakRef<CCScene> m_scene = nullptr;
    unsigned m_sceneChildren = 0;

    Ref<geode::Popup> m_popup;
    //WeakRef<geode::Popup> m_popup;
    Ref<CCLayer> m_layer;
    Ref<RenderNode> m_renderer;
    Ref<CCLayerColor> m_draw;
    Ref<ProxyNode> m_proxy;
    WeakRef<CCNode> m_body;
    WeakRef<CCNode> m_close;

    Ref<geode::GenericContentLayer> m_content;
    map_setting_node_t<ccColor3B>* m_rgbBackground = nullptr;
    map_setting_node_t<int>* m_backgroundType = nullptr;
    map_setting_node_t<bool>* m_disableBackground = nullptr;

    comm::ListenerHandle m_tabListener;
    comm::ListenerHandle m_closeListener;

    RLLayerBackgroundData m_data = {};

    bool m_holdingTab = false;
    bool m_offSettings = false;

    Transition m_state = Transition::SHOW;
    bool m_wasFocused = true;
    bool m_uncommitted = false;

public:
    Impl() = default;
    static PeekSettingsPopup::Impl* create(PeekSettingsPopup::Callback cb, Popup* popup);
    bool init(PeekSettingsPopup::Callback cb, Popup* popup);
    void update(float dt) override;
    // FIXME: Find out what's wrong with this at some point
    bool shouldRender() const { return false && CachedSettings::get()->enableExperimentalFeatures; }

    static CCAction* makeFadeAction(GLubyte opacity, float time = kACTION_TIME);
    static void stopAllActions(CCNode* node);

private:
    bool setupBody(Popup* popup);
    bool setupSettings();
    bool setupListeners(Popup* popup);

    void updateMouse();
    bool handleUnfocused();
    bool handleBackground();

    void drawTexture();
    void transition(Transition state, float time = kACTION_TIME);
    bool hasUncommittedChanges() const;
    bool isTransparent() const { return m_holdingTab || m_offSettings; }
};

void PeekSettingsPopup::Impl::update(float) {
    if (!shouldRender()) {
        handleBackground();
        return;
    }
    // We do have transparency...
    this->updateMouse();
    const bool transparent = isTransparent();
    bool unfocused = handleUnfocused();
    bool hasChanges = handleBackground();
    // Handle state transitions
    if (unfocused)
        this->transition(Transition::SHOW);
    else if (hasChanges && transparent)
        this->transition(Transition::HIDE);
    else if (m_holdingTab)
        this->transition(Transition::MISC);
    else
        this->transition(Transition::SHOW);
}

void PeekSettingsPopup::Impl::transition(Transition state, float time) {
    if (state == m_state) return;
    m_state = state;
    // Get transition opacity
    GLubyte opacity;
    if (state == Transition::SHOW) {
        opacity = 255;
        time *= 0.5f;
    } else if (state == Transition::HIDE) {
        opacity = 10;
        time *= 0.9f;
    } else /*state == Transition::MISC*/ {
        opacity = 128;
    }
    // Do transition
    stopAllActions(m_renderer);
    m_renderer->runAction(makeFadeAction(opacity, time));
}

void PeekSettingsPopup::Impl::updateMouse() {
#ifdef GEODE_IS_DESKTOP
    auto winSize = CCDirector::get()->getWinSize();
    CCPoint point = getMousePos();
    CCPoint pos = {point.x, winSize.height - point.y};
    if (isPointInsideNode(m_body, pos))
        this->m_offSettings = false;
    else if (isPointInsideNode(m_close, pos))
        this->m_offSettings = false;
    else
        this->m_offSettings = true;
#endif
}

bool PeekSettingsPopup::Impl::handleUnfocused() {
    // Check if the scene is focused
    if (auto scene = m_scene.lock()) {
        // This usually means something else is in focus.
        if (m_sceneChildren != scene->getChildrenCount()) {
            if (m_wasFocused) {
                log::debug("Scene unfocused!");
                m_wasFocused = false;
            }
            return true;
        } else if (!m_wasFocused) {
            m_wasFocused = true;
        }
    }
    return false;
}

bool PeekSettingsPopup::Impl::handleBackground() {
    bool hasChanges = this->hasUncommittedChanges();
    if (!hasChanges) {
        if (m_uncommitted) {
            m_data = rl::defaultLayerBackgroundData();
            if (m_callback) m_callback(m_data);
            m_uncommitted = false;
        }
        return false;
    }
    // We do have changes here!
    auto data = rl::defaultLayerBackgroundData();
    if (m_rgbBackground) data.color = m_rgbBackground->getValue();
    if (m_disableBackground) data.enabled = !m_disableBackground->getValue();
    if (m_backgroundType) data.type = m_backgroundType->getValue();
    // Check if we have NEW changes...
    if (data != m_data) {
        m_data = data;
        if (m_callback) m_callback(m_data);
    }
    m_uncommitted = true;
    return true;
}

static bool HasUncommittedChanges(SettingNodeV3* setting) {
    return setting && setting->hasUncommittedChanges();
}

bool PeekSettingsPopup::Impl::hasUncommittedChanges() const {
    if (HasUncommittedChanges(m_rgbBackground)) return true;
    if (HasUncommittedChanges(m_disableBackground)) return true;
    if (HasUncommittedChanges(m_backgroundType)) return true;
    return false;
}

CCAction* PeekSettingsPopup::Impl::makeFadeAction(GLubyte opacity, float time) {
    auto* baseAction = CCFadeTo::create(time, opacity);
    CCAction* action = CCEaseExponentialOut::create(baseAction);
    action->setTag(kACTION_TAG);
    return action;
}

void PeekSettingsPopup::Impl::stopAllActions(CCNode* node) {
    if (!node) return;
    CCAction* action = nullptr;
    while ((action = node->getActionByTag(kACTION_TAG))) {
        //action->update(1.f);
        node->stopAction(action);
    }
}

void PeekSettingsPopup::Impl::drawTexture() {
    bool visibility = m_popup->isVisible();
    if (!visibility) m_popup->setVisible(true);

    //m_draw->visit();
    glDisable(GL_BLEND);
    m_popup->visit();
    glEnable(GL_BLEND);

    // Rehide if needed
    if (!visibility) {
        m_popup->setVisible(false);
        m_popup->setMouseEnabled(true);
        m_popup->setTouchEnabled(true);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Initialization

template <typename T>
static T* CreateWithProps(CCNode* node) {
    T* out = T::create();
    out->setScale(node->getScale());
    out->setContentSize(node->getContentSize());
    out->setPosition(node->getPosition());
    out->setAnchorPoint(node->getAnchorPoint());
    return out;
}

static void changeColorLayerBlending(auto* layer) {
    ccBlendFunc blend = layer->getBlendFunc();
    log::info("Blend func: (src={}, dst={})", blend.src, blend.dst);
    layer->setBlendFunc(kCCBlendFuncDisable);
}

static Ref<geode::GenericContentLayer> getContentLayer(CCNode* layer) {
    auto* layerColor = QUERY_WITH(nullptr, layer->getChildByType<CCLayerColor>(0));
    //changeColorLayerBlending(layerColor);
    auto* content = QUERY_WITH(nullptr, layerColor->querySelector("ScrollLayer > content-layer"));
    //auto* content = QUERY_WITH(nullptr, layerColor->getChildByIDRecursive("content-layer"));
    return typeinfo_cast<GenericContentLayer*>(content);
}

bool PeekSettingsPopup::Impl::init(PeekSettingsPopup::Callback cb, Popup* popup) {
    if (!CCNode::init()) return false;
    if (!setupBody(popup)) return false;
    if (!setupSettings()) return false;
    if (!setupListeners(popup)) return false;

    this->m_popup = popup;
    this->m_callback = std::move(cb);
    this->m_data = rl::defaultLayerBackgroundData();

    this->setID("settings-popup-listener"_spr);
    this->scheduleUpdate();

    return true;
}

// TODO: Handle undefined0.draggable-popups
bool PeekSettingsPopup::Impl::setupBody(Popup* popup) {
    m_scene = popup->getParentByType<CCScene>();
    if (auto scene = m_scene.lock()) m_sceneChildren = scene->getChildrenCount();

    // Get the real layer
    Ref<CCLayer> layer = QUERY(popup->getChildByType<CCLayer>(0));
    layer->addChild(CreateWithProps<CCLayer>(layer.data()));
    layer->addChild(CreateWithProps<CCLayerRGBA>(layer.data()));

    // Load the settings list
    m_content = QUERY(getContentLayer(layer));

    if (!shouldRender()) {
        popup->getParent()->addChild(this);
        return true;
    }

    CCLayer* body = nullptr;
    geode::CircleButtonSprite* closeButtonSpr = nullptr;

    for (CCNode* child : layer->getChildrenExt()) {
        // Load the popup body
        body = typeinfo_cast<CCLayer*>(child);
        if (!body) continue;
        // Load the button itself
        auto* closeButton = body->getChildByIndex(0);
        if (!closeButton) continue;
        // Load the button sprite
        auto* sprite = closeButton->getChildByIndex(0);
        closeButtonSpr = typeinfo_cast<geode::CircleButtonSprite*>(sprite);
        if (closeButtonSpr) break;
    }
    if (!body || !closeButtonSpr) {
        log::warn("Could not find closeButtonSprite");
        return false;
    }

    m_body = body;
    m_close = closeButtonSpr;
    m_layer = layer.data();
    m_layer->setVisible(true);

    m_proxy = ProxyNode::create([weak = WeakRef(this)]() {
        if (auto self = weak.lock()) self->drawTexture();
    });

    popup->getParent()->addChild(this);
    popup->removeFromParentAndCleanup(false);
    m_renderer = RenderNode::create(m_proxy.data(), /*constrain=*/false);
    m_renderer->addChild(popup);
    addChild(m_renderer.data());

    return true;
}

bool PeekSettingsPopup::Impl::setupSettings() {
    size_t found = 0;
    auto load = [this, &found]<class S>(SettingValueNodeV3<S>*& val, SettingNode* setting) {
        using T = typename S::ValueType;
        if (auto* res = rl::setting_node_cast<T>(setting)) {
            log::debug("Found setting '{}'!", setting->getSetting()->getKey());
            val = res;
            ++found;
        } else {
            std::string_view type = geode::cocos::getObjectName(setting);
            log::warn("Setting node '{}' had the wrong type: {}", setting->getSetting()->getKey(), type);
        }
    };
    for (auto* setting : m_content->getChildrenExt<SettingNode>()) {
        if (!setting) continue;
        std::string key = setting->getSetting()->getKey();
        //log::debug("Setting: {}", key);
        auto it = ElementMap.find(key.c_str());
        if (it == ElementMap.end()) continue;
        switch (it->second) {
        case rgbBackground: load(m_rgbBackground, setting); break;
        case backgroundType: load(m_backgroundType, setting); break;
        case disableBackground: load(m_disableBackground, setting); break;
        }
        // Skip iteration if we can!
        if (found >= 3) break;
    }
    // Log if something went wrong
    if (found == 0)
        log::error("Unable to find background settings for popup!");
    else if (found < 3)
        log::warn("Unable to find all background settings for popup.");
    return found > 0;
}

bool PeekSettingsPopup::Impl::setupListeners(Popup* popup) {
    m_closeListener = Popup::CloseEvent(popup).listen([weak = WeakRef(this)]() {
        if (auto self = weak.lock()) {
            if (self->hasUncommittedChanges()) {
                auto data = rl::defaultLayerBackgroundData();
                if (self->m_callback) self->m_callback(data);
            }
            self->removeFromParent();
        }
    });
    if (!shouldRender()) return true;
    m_tabListener = KeyboardInputEvent(enumKeyCodes::KEY_Tab).listen([this](KeyboardInputData& data) {
        using enum KeyboardInputData::Action;
        if (data.action == Press)
            this->m_holdingTab = true;
        else if (data.action == Repeat)
            this->m_holdingTab = true;
        else
            this->m_holdingTab = false;
    });
    return true;
}

PeekSettingsPopup::Impl* PeekSettingsPopup::Impl::create(PeekSettingsPopup::Callback cb, Popup* popup) {
    auto ret = new PeekSettingsPopup::Impl();
    if (ret->init(std::move(cb), popup)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

static bool shouldTryInjecting() {
    //if (!CachedSettings::get()->enableExperimentalFeatures) return false;
    if (CachedSettings::mods()->draggable_popups) {
        log::warn("draggable-popups is potentially incompatibile with PeekSettingsPopup");
        return false;
    }
    return true;
}

Popup* PeekSettingsPopup::create(PeekSettingsPopup::Callback cb, geode::Mod* mod) {
    Popup* popup = geode::openSettingsPopup(mod, true);
    if (!popup) return nullptr;
    if (!shouldTryInjecting()) return popup;
    // Initialize the listener
    PeekSettingsPopup::Impl::create(std::move(cb), popup);
    return popup;
}
