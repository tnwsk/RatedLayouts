#include "custom/LazyListButtonPage.hpp"
#include "custom/LazyListButtonPageDelegate.hpp"
#include "utils/ScopedSave.hpp"

using namespace geode::prelude;
using namespace rl;

bool LazyListButtonPage::loadImpl(LazyListButtonPageDelegate* loader) {
    CCArray* items = loader->getItemsForPage(m_page);
    if (!items) {
        log::error("LazyListButtonPage[{}] failed to load items", m_page);
        return false;
    }

    m_pageNode =
        ListButtonPage::create(items, m_position, m_columns, m_rows, m_colOffset, m_rowOffset, m_offset);
    if (!m_pageNode) {
        log::error("LazyListButtonPage[{}] failed to create ListButtonPage", m_page);
        return false;
    }

    removeAllChildren();
    addChild(m_pageNode);

    return true;
}

bool LazyListButtonPage::load(LazyListButtonPageDelegate* loader) {
    if (isLoaded() || m_currentlyLoading) return true;
    ScopedSave save(m_currentlyLoading, true);

    if (m_page < 0) {
        log::error("LazyListButtonPage loaded with no provided page!");
        return false;
    }
    if (!loader) {
        if (!m_delegate) {
            log::error("LazyListButtonPage[{}] loaded with no loading delegate", m_page);
            return false;
        }
        // Set to our stored delegate.
        loader = m_delegate;
    }

    bool didLoad = loadImpl(loader);
    if (didLoad)
        loader->loadingFinished(m_page);
    else
        loader->loadingFailed(m_page);
    return didLoad;
}

void LazyListButtonPage::setVisible(bool visible) {
    CCLayer::setVisible(visible);
    if (visible && m_loadWhenVisible) this->load(nullptr);
}

void LazyListButtonPage::setLoadWhenVisible(bool enable) {
    if (enable) CCLayer::setVisible(false);
    this->m_loadWhenVisible = enable;
}

LazyListButtonPage* LazyListButtonPage::create(
    cocos2d::CCPoint position, int columns, int rows, float columnOffset, float rowOffset, float offset) {
    auto ret = new LazyListButtonPage();
    if (ret->init(position, columns, rows, columnOffset, rowOffset, offset)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LazyListButtonPage::init(
    cocos2d::CCPoint position, int columns, int rows, float columnOffset, float rowOffset, float offset) {
    if (!CCLayer::init()) return false;
    this->m_position = position;
    this->m_columns = columns;
    this->m_rows = rows;
    this->m_colOffset = columnOffset;
    this->m_rowOffset = rowOffset;
    this->m_offset = offset;
    return true;
}

void LazyListButtonPageDelegate::anchor() {}
