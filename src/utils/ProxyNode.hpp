#pragma once

#include <Geode/Geode.hpp>

namespace rl {

class ProxyNode : public cocos2d::CCNode {
public:
    static ProxyNode* create(geode::Function<void()> callback);
    bool init(geode::Function<void()> callback);

    geode::Function<void()> m_callback;

    virtual void visit() override;
};

}  // namespace rl
