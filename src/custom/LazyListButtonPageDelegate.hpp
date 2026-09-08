#pragma once

#include <Geode/cocos/cocoa/CCArray.h>

namespace rl {

class LazyListButtonPageDelegate {
public:
    virtual cocos2d::CCArray* getItemsForPage(int page) = 0;
    virtual void loadingFinished(int page) {}
    virtual void loadingFailed(int page) {}

private:
    // Defined in `custom/LazyListButtonPage.cpp`.
    virtual void anchor();
};

}  // namespace rl
