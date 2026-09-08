#include "RLLayerBackground.hpp"
#include <Geode/Geode.hpp>
#include <cue/RepeatingBackground.hpp>
#include "utils/CachedSettings.hpp"

using namespace geode::prelude;
using namespace rl;

static std::string getBgName(int value) {
    //std::string bgIndex = (value >= 1 && value <= 9)
    //                          ? ("0" + numToString(value))
    //                          : numToString(value);
    return fmt::format("game_bg_{:02d}_001.png", value);
}

RLLayerBackgroundData rl::defaultLayerBackgroundData() {
    return RLLayerBackgroundData {
        .enabled = !CachedSettings::get()->disableBackground,
        .color = CachedSettings::get()->rgbBackground,
        .type = CachedSettings::get()->backgroundType
    };
}

cocos2d::CCSprite* rl::addLayerBackground(cocos2d::CCNode* parent, RLLayerBackgroundData const& data) {
    if (!data.enabled) {
        cocos2d::CCSprite* bg = createLayerBG();
        bg->setColor(data.color);
        parent->addChild(bg, -1);
        return bg;
    } else {
        std::string bgName = getBgName(data.type);
        auto* bg = cue::RepeatingBackground::create(bgName.c_str(), 1.f, cue::RepeatMode::X);
        bg->setColor(data.color);
        parent->addChild(bg, -1);
        return bg;
    }
}

cocos2d::CCSprite* rl::addLayerBackground(cocos2d::CCNode* parent, bool enabled) {
    auto color = CachedSettings::get()->rgbBackground;
    if (!enabled) {
        cocos2d::CCSprite* bg = createLayerBG();
        bg->setColor(color);
        parent->addChild(bg, -1);
        return bg;
    } else {
        std::string bgName = getBgName(CachedSettings::get()->backgroundType);
        auto* bg = cue::RepeatingBackground::create(bgName.c_str(), 1.f, cue::RepeatMode::X);
        bg->setColor(color);
        parent->addChild(bg, -1);
        return bg;
    }
}

cocos2d::CCSprite* rl::addLayerBackground(cocos2d::CCNode* parent) {
    return rl::addLayerBackground(parent, !CachedSettings::get()->disableBackground);
}
