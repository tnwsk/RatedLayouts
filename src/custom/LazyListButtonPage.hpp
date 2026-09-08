#pragma once

#include <Geode/binding/ListButtonPage.hpp>

namespace rl {

class LazyListButtonPageDelegate;

class LazyListButtonPage final : public cocos2d::CCLayer {
    cocos2d::CCPoint m_position = {};
    int m_columns = 0, m_rows = 0;
    float m_colOffset = 0.f;
    float m_rowOffset = 0.f;
    float m_offset = 0.f;

    LazyListButtonPageDelegate* m_delegate = nullptr;
    ListButtonPage* m_pageNode = nullptr;
    int m_page = -1;
    bool m_currentlyLoading = false;
    bool m_loadWhenVisible = false;

    bool loadImpl(LazyListButtonPageDelegate* loader);

public:
    static LazyListButtonPage* create(
        cocos2d::CCPoint position, int columns, int rows, float columnOffset, float rowOffset, float offset);
    bool init(
        cocos2d::CCPoint position, int columns, int rows, float columnOffset, float rowOffset, float offset);

    // TODO: loadInBackground()?
    bool load(LazyListButtonPageDelegate* loader = nullptr);
    bool isLoaded() const { return !!m_pageNode; }
    void setVisible(bool visible) override;

    bool setPage(int page) {
        if (!isLoaded()) {
            this->m_page = page;
            return true;
        }
        return false;
    }
    int getPage() const { return m_page; }

    void setDelegate(LazyListButtonPageDelegate* delegate) { this->m_delegate = delegate; }
    LazyListButtonPageDelegate* getDelegate() const { return m_delegate; }

    void setLoadWhenVisible(bool enable); // If enabled, hides layer.
    bool isLoadWhenVisible() const { return m_loadWhenVisible; }
};

}  // namespace rl
