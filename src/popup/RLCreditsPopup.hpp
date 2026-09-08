#pragma once

#include <Geode/Geode.hpp>
#include <cue/ListNode.hpp>
#include <matjson.hpp>

using namespace geode::prelude;

class RLCreditsPopup : public geode::Popup {
public:
    static RLCreditsPopup* create();

private:
    bool init() override;
    void initCredits(matjson::Value json);
    void initCreditsOld(matjson::Value json);
    Ref<cue::ListNode> makeListNode();

    void onAccountClicked(CCObject* sender);
    void onInfo(CCObject* sender);
    void onHeaderInfo(CCObject* sender);

    Ref<cue::ListNode> m_listNode;
    LoadingSpinner* m_spinner = nullptr;
    Scrollbar* m_scrollbar = nullptr;
};
