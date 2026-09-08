#include "custom/RLShopkeeperSprite.hpp"
#include <Geode/Geode.hpp>
#include "utils/RandomGen.hpp"

using namespace geode::prelude;
using namespace rl;

static constexpr const char* KEEPER_IDLE = "RL_arcticwoof01.png"_spr;
static constexpr const char* KEEPER_CLOSING = "RL_arcticwoof02.png"_spr;
static constexpr const char* KEEPER_CLOSED = "RL_arcticwoof03.png"_spr;

static constexpr float kTimeBetweenBlinks = 0.02f;
static constexpr float kScaleTime = 2.0f;
static constexpr float kScaleFactorX = 0.02f;
static constexpr float kScaleFactorY = kScaleFactorX / 2.0f;

//CCPartAnimSprite
//CCSpritePart

// TIME BETWEEN BLINKS: [3.5, 6.0)
// TIME BLINKING: [0.05, 0.15)

static int GetFrameNumber(RLShopkeeperSprite::Transition state) {
    using enum RLShopkeeperSprite::Transition;
    switch (state) {
    case IDLE:
    case BETWEEN: return 0;
    case CLOSING:
    case OPENING: return 1;
    case CLOSED: return 2;
    default: return 0;
    }
}

void RLShopkeeperSprite::update(float dt) {
    m_elapsed += dt;
    if (m_elapsed < m_totalTime) return;
    this->m_elapsed = 0.0f;
    // Transition to the next state...
    switch (m_state) {
    // Start blink
    case Transition::IDLE:
    case Transition::BETWEEN:
        m_state = Transition::CLOSING;
        m_totalTime = kTimeBetweenBlinks;
        break;
    // End blink
    case Transition::OPENING: this->scheduleBlink(); break;
    // Open eyes
    case Transition::CLOSED:
        m_state = Transition::OPENING;
        m_totalTime = kTimeBetweenBlinks;
        break;
    // Close eyes
    case Transition::CLOSING:
        m_state = Transition::CLOSED;
        m_totalTime = globalRNG()->generate<float>(0.05, 0.15);
        break;
    }
    this->changeFrame();
}

void RLShopkeeperSprite::scheduleBlink() {
    ++m_timesBlinked;
    if (m_timesBlinked >= m_blinks) {
        m_state = Transition::IDLE;
        m_totalTime = globalRNG()->generate<float>(3.5, 6.0);
        m_timesBlinked = 0;
        // For double blinks
        float odds = globalRNG()->generate<float>(0.0, 10.0);
        m_blinks = (odds > 1.5) ? 1 : 2;
    } else /*multi blink*/ {
        m_state = Transition::BETWEEN;
        m_totalTime = globalRNG()->generate<float>(0.1, 0.2);
    }
}

void RLShopkeeperSprite::changeFrame() {
    if (m_currFrame) m_currFrame->setVisible(false);
    m_currFrame = m_frames[GetFrameNumber(m_state)];
    if (m_currFrame) m_currFrame->setVisible(true);
}

bool RLShopkeeperSprite::initSprites() {
    static constexpr const char* kFrames[]{KEEPER_IDLE, KEEPER_CLOSING, KEEPER_CLOSED};
    for (size_t Ix = 0; Ix < std::size(kFrames); ++Ix) {
        auto* spr = CCSprite::createWithSpriteFrameName(kFrames[Ix]);
        if (!spr) {
            log::error("Failed to init shopkeeper sprite {}!", Ix);
            return false;
        }
        spr->setVisible(false);
        spr->setAnchorPoint({0, 0});
        this->m_frames[Ix] = spr;
        m_keeper->addChild(spr, Ix);
    }
    return true;
}

bool RLShopkeeperSprite::initActions() {
    auto* tall = CCEaseSineInOut::create(CCScaleTo::create(kScaleTime, 1 - kScaleFactorX, 1 + kScaleFactorY));
    auto* wide = CCEaseSineInOut::create(CCScaleTo::create(kScaleTime, 1 + kScaleFactorX, 1 - kScaleFactorY));
    auto* join = CCSequence::createWithTwoActions(tall, wide);
    auto* action = CCRepeatForever::create(join);
    action->setTag(5559);
    m_keeper->runAction(action);
    return true;
}

bool RLShopkeeperSprite::init(bool actions) {
    if (!CCNode::init()) return false;

    m_keeper = CCNodeRGBA::create();
    if (!m_keeper) return false;
    this->addChild(m_keeper.data());

    if (!initSprites()) return false;
    m_keeper->setVisible(true);

    if (actions) {
        if (!initActions()) {
            log::warn("Unable to create shopkeeper actions.");
        }
    }

    auto contentSize = m_frames[0]->getContentSize();
    m_keeper->setContentSize(contentSize);
    this->setContentSize(contentSize);

    m_keeper->setAnchorPoint({0.5, 0.0});
    m_keeper->setPositionX(contentSize.width / 2);

    this->scheduleBlink();
    this->changeFrame();
    this->scheduleUpdate();

    return true;
}

bool RLShopkeeperSprite::init() {
    return init(false);
}

RLShopkeeperSprite* RLShopkeeperSprite::create(bool actions) {
    RLShopkeeperSprite* ret = new RLShopkeeperSprite;
    if (ret->init(actions)) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

RLShopkeeperSprite* RLShopkeeperSprite::create() {
    return RLShopkeeperSprite::create(false);
}
