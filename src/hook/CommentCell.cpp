#include "Geode/loader/Loader.hpp"
#include "Geode/loader/Mod.hpp"
#include "Geode/ui/NineSlice.hpp"
#include "ccTypes.h"
#include <optional>
#include <Geode/Geode.hpp>
#include <Geode/modify/CommentCell.hpp>
#include "FallbackBadges.hpp"

#include "RLConstants.hpp"
#include "RLNetworkUtils.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/LazyNameplate.hpp"
#include "utils/RLData.hpp"

// TODO: Merge implementation between this and ProfilePage

using namespace geode::prelude;
using namespace rl;

class $modify(RLCommentCell, CommentCell) {
    struct Fields : rl::RLUserInfo {
        std::optional<matjson::Value> m_pendingRoleJson;
        RLUserId m_pendingRoleAccountId = 0;
        bool m_pendingRoleTextColorScheduled = false;
        int m_pendingRoleTextColorRetry = 0;
        async::TaskHolder<Result<RLUserInfo>> m_fetchTask;
        ~Fields() { m_fetchTask.cancel(); }
    };

    static void onModify(auto& self) {
        if (!self.setHookPriorityAfterPost("CommentCell::fetchUserRole", "prevter.comment_emojis")) {
            log::warn("Failed to set hook priority from prevter.comment_emojis.");
        }
    }

    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);

        if (!comment) {
            return;
        }

        if (m_accountComment) {
            return;
        }

        fetchUserRole(comment->m_accountID);
    }

    void onEnter() override {
        CommentCell::onEnter();
        // TODO: Detangle this whole mess
        if (m_fields->m_pendingRoleJson && !m_accountComment) {
            this->applyUserRole(m_fields->userInfo(), m_fields->m_pendingRoleAccountId);
            if (m_fields->m_pendingRoleJson) {
                this->schedulePendingRoleTextColorRetry();
            }
        }
    }

    // for some damn reason comment text colors doesnt apply cuz of race conditions with the user role fetch
    // so another hacky way to apply the text color after role info is fetched and applied
    // this is really jank but it works so whatever
    void schedulePendingRoleTextColorRetry() {
        if (m_fields->m_pendingRoleTextColorScheduled || !m_fields->m_pendingRoleJson || !m_mainLayer) return;
        m_fields->m_pendingRoleTextColorScheduled = true;
        this->runAction(
            CCSequence::create(CCDelayTime::create(0.0f),
                               CCCallFunc::create(this, callfunc_selector(RLCommentCell::applyPendingRoleTextColor)),
                               nullptr));
    }

    void applyPendingRoleTextColor() {
        m_fields->m_pendingRoleTextColorScheduled = false;
        if (!m_fields->m_pendingRoleJson || !m_mainLayer) return;
        if (applyCommentTextColor(m_fields->m_pendingRoleAccountId)) {
            m_fields->m_pendingRoleJson = std::nullopt;
            m_fields->m_pendingRoleAccountId = 0;
            m_fields->m_pendingRoleTextColorRetry = 0;
            return;
        }
        if (m_fields->m_pendingRoleTextColorRetry++ < 5) {
            this->schedulePendingRoleTextColorRetry();
        }
    }

    bool applyCommentTextColor(RLUserId accountId) {
        // nothing to do if user has no special state
        if (m_fields->isEmptyUser())
            return false;
        else if (!m_mainLayer) {
            log::warn("main layer is null, cannot apply color");
            return false;
        }

        ccColor3B color = ccWHITE;  // default white
        // choose highest-priority role for color
        if (m_fields->isOwner) {
            color = {150, 255, 255};  // owner
        } else if (m_fields->isDeveloper) {
            color = {175, 255, 0};  // developer
        } else if (m_fields->isClassicAdmin) {
            color = {180, 180, 255};  // classic admin
        } else if (m_fields->isPlatAdmin) {
            color = {245, 150, 0};  // plat admin
        } else if (m_fields->isLeaderboardAdmin) {
            color = {140, 255, 140};  // leaderboard admin
        } else if (m_fields->isLeaderboardMod) {
            color = {0, 245, 200};  // leaderboard mod
        } else if (m_fields->isClassicMod) {
            color = {135, 190, 255};  // classic mod
        } else if (m_fields->isPlatMod) {
            color = {255, 200, 150};  // plat mod
        } else if (m_fields->isSupporter) {
            color = {255, 125, 200};  // supporter
        } else if (m_fields->isBooster) {
            color = {190, 150, 255};  // booster
        }

        log::debug("Applying comment text color for role: {} in {} mode",
                   m_fields->isClassicAdmin ? "admin"
                   : m_fields->isClassicMod ? "mod"
                   : m_fields->isSupporter  ? "supporter"
                   : m_fields->isBooster    ? "booster"
                                            : "normal",
                   m_compactMode ? "compact" : "non-compact");

        // check for prevter.comment_emojis
        bool colorApplied = false;
        if (CachedSettings::mods()->comment_emojis) {
            log::debug("prevter.comment_emojis mod detected, looking for custom text area");

            if (auto emojiTextArea = m_mainLayer->getChildByIDRecursive("prevter.comment_emojis/comment-text-area")) {
                if (auto colorable = typeinfo_cast<CCRGBAProtocol*>(emojiTextArea)) {
                    colorable->setColor(color);
                    colorApplied = true;
                }
            }
            if (auto emojiLabel = m_mainLayer->getChildByIDRecursive("prevter.comment_emojis/comment-text-label")) {
                if (auto colorable = typeinfo_cast<CCRGBAProtocol*>(emojiLabel)) {
                    colorable->setColor(color);
                    colorApplied = true;
                }
            }
            return colorApplied;
        }

        // vanilla text area/label
        if (auto commentTextLabel =
                m_mainLayer->getChildByIDRecursive("comment-text-label")) {  // compact mode (easy face mode)
            log::debug("Found comment-text-label, applying color");
            if (auto colorable = typeinfo_cast<CCRGBAProtocol*>(commentTextLabel)) {
                colorable->setColor(color);
                colorApplied = true;
            } else if (auto label = typeinfo_cast<CCLabelBMFont*>(commentTextLabel)) {
                label->setColor(color);
                colorApplied = true;
            }
        }
        if (auto textArea = m_mainLayer->getChildByIDRecursive("comment-text-area")) {
            if (auto colorable = typeinfo_cast<CCRGBAProtocol*>(textArea)) {
                colorable->setColor(color);
                colorApplied = true;
            } else if (auto area = typeinfo_cast<TextArea*>(textArea)) {
                area->colorAllLabels(color);
                colorApplied = true;
            }
        }
        if (!colorApplied) {
            log::debug("CommentCell: no compatible comment text node found for role color application");
        }
        return colorApplied;
    }

    void applyUserRole(RLUserInfo const& info, RLUserId accountId) {
        m_fields->init(info);
        bool validCell = m_mainLayer && m_backgroundLayer;
        const auto nameplate = info.nameplate;
        if (validCell && nameplate != 0 && !CachedSettings::get()->disableNameplateInComment) {
            auto createNameplateSprite = [nameplate](CCSize size, CCPoint pos) {
                return LazyNameplate::create(
                    size, nameplate, {.loadingCircle = true, .autoResize = true, .position = pos});
            };

            if (m_compactMode) {
                auto content = m_backgroundLayer->getScaledContentSize();
                auto* lazy = createNameplateSprite(content, {content.width / 2, content.height / 2});
                m_backgroundLayer->setOpacity(100);
                m_backgroundLayer->addChild(lazy, -1);

                if (!m_mainLayer->getChildByID("rl-comment-bg")) {
                    auto commentBg = NineSlice::create("square02_small.png");
                    commentBg->setID("rl-comment-bg");
                    auto commentText = m_mainLayer->getChildByIDRecursive("comment-text-label");
                    if (commentText) {
                        commentBg->setInsets({5, 5, 5, 5});
                        commentBg->setContentSize(commentText->getScaledContentSize() + CCSize(5, 0));
                        commentBg->setPosition({commentText->getPosition().x - 2, commentText->getPosition().y});
                        commentBg->setOpacity(150);
                        commentBg->setAnchorPoint(commentText->getAnchorPoint());
                        m_mainLayer->addChild(commentBg, -1);
                    }
                }
            } else {
                auto* lazy = createNameplateSprite({m_backgroundLayer->getScaledContentSize() + CCSize(425, 425)},
                                                   {-20, m_backgroundLayer->getScaledContentSize().height / 2});
                m_backgroundLayer->setOpacity(150);
                m_backgroundLayer->addChild(lazy, -1);
            }
        }

        loadBadgeForComment(accountId);
        bool colorApplied = applyCommentTextColor(accountId);
        if (!colorApplied) {
            m_fields->m_pendingRoleJson = info;
            m_fields->m_pendingRoleAccountId = accountId;
            this->schedulePendingRoleTextColorRetry();
        }
        applyStarGlow(accountId, info.stars, info.planets);
    }

    void applyUserRoleJson(matjson::Value const& json, RLUserId accountId) {
        auto infoOrErr = json.as<RLUserInfo>();
        if (infoOrErr.isErr() || infoOrErr.unwrap().isEmptyUser()) {
            // Empty user OR parse failed (unlikely).
            this->clearAllRoleBadges(accountId);
            return;
        }

        this->applyUserRole(infoOrErr.unwrap(), accountId);
    }

    void clearAllRoleBadges(RLUserId accountId) {
        m_fields->clear();
        if (!m_mainLayer) [[likely]] {
            return;
        }
        rl::clearFallbackBadgesImpl(Ref(this), "comment");
        // remove any glow
        auto glowId = fmt::format("rl-comment-glow-{}", accountId);
        if (auto glow = m_mainLayer->getChildByIDRecursive(glowId)) glow->removeFromParent();
    }

    void fetchUserRole(RLUserId accountId) {
        log::debug("Fetching role for comment user ID: {}", accountId);
        Ref<RLCommentCell> cellRef = this;  // commentcell ref
        m_fields->m_fetchTask.spawn(RLUserInfo::get(accountId), [cellRef, accountId](Result<RLUserInfo> infoOrErr) {
            // did this so it doesnt crash if the cell is deleted before
            // response yea took me a while
            if (!cellRef) {
                log::warn("CommentCell has been destroyed, skipping role update");
                return;
            }

            if (infoOrErr.isErr() || infoOrErr.unwrap().isEmptyUser()) {
                // log::warn("Server returned non-ok status: {}", response.code());
                if (infoOrErr.isErr())
                    log::debug("Profile load failed: {}", infoOrErr.unwrapErr());
                else
                    log::debug("User {} has no role/stars/planets", accountId);
                if (!cellRef) return;
                cellRef->clearAllRoleBadges(accountId);
                return;
            }

            RLUserInfo info = std::move(infoOrErr).unwrap();
            if (cellRef) {
            }
            cellRef->m_fields->init(info);

            // nameplate thing
            bool validCell = cellRef && cellRef->m_mainLayer && cellRef->m_backgroundLayer;
            const auto nameplate = info.nameplate;
            if (validCell && nameplate != 0 && !CachedSettings::get()->disableNameplateInComment) {
                if (cellRef->m_compactMode) {
                    auto* lazy =
                        LazyNameplate::create({cellRef->m_backgroundLayer->getScaledContentSize()}, nameplate, true);
                    lazy->setAutoResize(true);
                    lazy->setPosition({cellRef->m_backgroundLayer->getScaledContentSize().width / 2,
                                       cellRef->m_backgroundLayer->getScaledContentSize().height / 2});
                    cellRef->m_backgroundLayer->setOpacity(100);
                    cellRef->m_backgroundLayer->addChild(lazy, -1);

                    // add a background behind the comment text for better contrast with bright nameplates in compact mode
                    if (!cellRef->m_mainLayer->getChildByID("rl-comment-bg")) {
                        auto commentBg = NineSlice::create("square02_small.png");
                        commentBg->setID("rl-comment-bg");
                        auto commentText = cellRef->m_mainLayer->getChildByIDRecursive("comment-text-label");
                        if (commentText) {
                            commentBg->setInsets({5, 5, 5, 5});
                            commentBg->setContentSize(commentText->getScaledContentSize() + CCSize(5, 0));
                            commentBg->setPosition({commentText->getPosition().x - 2, commentText->getPosition().y});
                            commentBg->setOpacity(150);
                            commentBg->setAnchorPoint(commentText->getAnchorPoint());
                            cellRef->m_mainLayer->addChild(commentBg, -1);
                        } else {
                            log::warn(
                                "CommentCell compress mode: comment-text-label not found, skipping text background for "
                                "nameplate");
                        }
                    }
                } else {
                    auto* lazy = LazyNameplate::create(
                        {cellRef->m_backgroundLayer->getScaledContentSize() + CCSize(425, 425)}, nameplate, true);
                    lazy->setAutoResize(true);
                    lazy->setPosition({-20, cellRef->m_backgroundLayer->getScaledContentSize().height / 2});
                    cellRef->m_backgroundLayer->setOpacity(150);
                    cellRef->m_backgroundLayer->addChild(lazy, -1);
                }
            } else if (!validCell && nameplate != 0) {
                log::debug("Skipping nameplate for account {} because CommentCell was removed or invalid", accountId);
            }

            if (!cellRef->m_mainLayer) {
                cellRef->m_fields->m_pendingRoleJson = info;
                cellRef->m_fields->m_pendingRoleAccountId = accountId;
            }

            log::debug(
                "User comment supporter={}, booster={}, classicMod={}, "
                "classicAdmin={}, leaderboardMod={}, platMod={}, platAdmin={}, "
                "nameplate={}",
                info.isSupporter,
                info.isBooster,
                info.isClassicMod,
                info.isClassicAdmin,
                info.isLeaderboardMod,
                info.isPlatMod,
                info.isPlatAdmin,
                info.nameplate);

            cellRef->loadBadgeForComment(accountId);
            cellRef->applyCommentTextColor(accountId);
            cellRef->applyStarGlow(accountId, info.stars, info.planets);
        });
    }

    void loadBadgeForComment(RLUserId accountId) {
        if (!m_mainLayer) {
            log::warn("main layer is null, cannot load badge for comment");
            return;
        }
        rl::setupFallbackBadgesImpl(Ref(this), m_fields->userInfo(), "comment", 0.550f);
        applyCommentTextColor(accountId);
    }

    // show explanatory alert when a badge in a comment is tapped
    void onBadgeClicked(CCObject* sender) {
        if (auto* btn = static_cast<CCMenuItemSpriteExtra*>(sender)) rl::showRoleInfoPopup(btn->getTag());
    }

    void applyStarGlow(RLUserId accountId, int stars, int planets) {
        // skip if no stars/planets
        if (stars <= 0 && planets <= 0) return;
        // global disable
        if (CachedSettings::get()->disableCommentGlow) {
            log::debug("Skipping star glow: global setting disabled");
            return;
        }

        if (m_fields->nameplate != 0 && CachedSettings::get()->disableCommentGlowNameplate) {
            if (!CachedSettings::get()->disableNameplateInComment) {
                log::debug("Skipping star glow for account {} due to nameplate and setting", accountId);
                return;
            }
        }

        if (!m_mainLayer) return;

        if (m_accountComment) return;  // no glow for account comments

        auto glowId = fmt::format("rl-comment-glow-{}", accountId);
        // don't create duplicate glow
        if (m_mainLayer->getChildByIDRecursive(glowId)) return;

        auto glow = CCSprite::createWithSpriteFrameName("chest_glow_bg_001.png");
        if (!glow) return;

        if (m_backgroundLayer && m_mainLayer) {
            auto glow = CCLayerGradient::create({135, 180, 255, 180}, {0, 0, 0, 0}, {1.f, 0.f});
            glow->changeWidthAndHeight(m_backgroundLayer->getContentSize().width,
                                       m_backgroundLayer->getContentSize().height);
            glow->setID(glowId.c_str());
            m_mainLayer->addChild(glow, -2);
        }
    }
};
