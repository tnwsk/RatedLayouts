#pragma once

#include <Geode/cocos/sprite_nodes/CCSprite.h>
#include <Geode/cocos/actions/CCActionInterval.h>

namespace rl {

class RLShopkeeperSprite : public cocos2d::CCNode {
public:
    enum class Transition {
        IDLE = 0,
        CLOSING = 1,
        CLOSED = 2,
        OPENING,
        BETWEEN,
    };

private:
    geode::Ref<cocos2d::CCNodeRGBA> m_keeper;
    cocos2d::CCSprite* m_currFrame = nullptr;
    cocos2d::CCSprite* m_frames[3] = {};

    Transition m_state = Transition::IDLE;
    float m_elapsed = 0.0f;
    float m_totalTime = 0.0f;
    int m_timesBlinked = 0;
    int m_blinks = 1;

public:
    static RLShopkeeperSprite* create();
    static RLShopkeeperSprite* create(bool actions);
    bool init() override;
    bool init(bool actions);
    void update(float dt) override;
    void changeFrame();

private:
    void scheduleBlink();
    bool initSprites();
    bool initActions();
};

}  // namespace rl
