#pragma once

#include <optional>
#include <Geode/ui/LazySprite.hpp>
#include "ccTypes.h"

namespace rl {
struct RLLazyImageOpts {
    bool loadingCircle = true;
    bool autoResize = false;
    std::optional<cocos2d::CCPoint> position = std::nullopt;
};

struct LazyNameplate {
    static geode::LazySprite* create(cocos2d::CCSize size, int id, bool loadingCircle = true);
    static geode::LazySprite* create(cocos2d::CCSize size, int id, RLLazyImageOpts const& opts);
};

struct LazyIcon {
    static geode::LazySprite* create(cocos2d::CCSize size, std::string url, bool loadingCircle = true);
    static geode::LazySprite* create(cocos2d::CCSize size, std::string url, RLLazyImageOpts const& opts);
};
}  // namespace rl
