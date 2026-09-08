#pragma once

#include <Geode/Geode.hpp>

namespace rl {
struct RLLayerBackgroundData {
    bool enabled = true;
    cocos2d::ccColor3B color = {};
    int type = 1;
public:
    bool operator==(RLLayerBackgroundData const&) const = default;
};

RLLayerBackgroundData defaultLayerBackgroundData();
cocos2d::CCSprite* addLayerBackground(cocos2d::CCNode* parent);
cocos2d::CCSprite* addLayerBackground(cocos2d::CCNode* parent, bool enabled);
cocos2d::CCSprite* addLayerBackground(cocos2d::CCNode* parent, RLLayerBackgroundData const& data);
}  // namespace rl
