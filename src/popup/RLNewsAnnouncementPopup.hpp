#pragma once

#include <vector>
#include <Geode/Geode.hpp>
#include <Geode/ui/MDTextArea.hpp>
#include <Geode/utils/async.hpp>
#include <cue/ListNode.hpp>
#include <matjson.hpp>

using namespace geode::prelude;

class RLNewsAnnouncementPopup : public geode::Popup {
public:
    static RLNewsAnnouncementPopup* create();

private:
    struct AnnouncementEntry {
        int id = 0;
        std::string timestampLabel;
        std::string title;
        std::string body;
    };

    bool init() override;
    void updateContent(std::string const& content);
    void showError(std::string const& message);
    void addAnnouncementItem(AnnouncementEntry const& entry, int index);
    void onAnnouncementSelected(CCObject* sender);
    void selectAnnouncement(int index);
    void onChangelogButton(CCObject* sender);
    void updateContent(std::string const& content, std::string const& title);

    MDTextArea* m_bodyText = nullptr;
    cue::ListNode* m_listNode = nullptr;
    std::vector<AnnouncementEntry> m_announcements;
    geode::async::TaskHolder<Result<matjson::Value>> m_fetchTask;
};
