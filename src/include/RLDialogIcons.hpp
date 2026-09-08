#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace rl {
    inline constexpr int DialogIconTagBase = 10100;
    inline constexpr int DialogIconCount = 5;

    void setDialogObjectIcon(DialogLayer* dialog, int characterFrame);
    void setDialogObjectCustomIcon(DialogLayer* dialog, const std::string& frameName);
    void setDialogObjectCustomIcon(DialogLayer* dialog, cocos2d::CCNode* icon);
}
