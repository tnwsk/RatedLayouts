#include "RLConstants.hpp"
#include "utils/CachedSettings.hpp"
#include "utils/Cast.hpp"
#include "utils/RLArgon.hpp"
#include "utils/RLData.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/utils/async.hpp>
#include <capeling.garage-stats-menu/include/StatsDisplayAPI.h>

using namespace geode::prelude;
using namespace rl;

class $modify(GJGarageLayer) {
    struct Fields : public rl::RLUserStats {
        CCNode* myStatItem = nullptr;
        CCNode* statMenu = nullptr;

        CCNode* starsValue = nullptr;
        CCNode* planetsValue = nullptr;
        CCNode* coinsValue = nullptr;
        CCNode* votesValue = nullptr;
        CCNode* pointsValue = nullptr;
        async::TaskHolder<Result<matjson::Value>> m_profileTask;
        ~Fields() { m_profileTask.cancel(); }
    };

    bool init() {
        if (!GJGarageLayer::init()) return false;
        if (CachedSettings::get()->disableGarageStats) return true;
        m_fields->statMenu = this->getChildByID("capeling.garage-stats-menu/stats-menu");
        fetchProfile(GJAccountManager::get()->m_accountID);
        return true;
    }

    void fetchProfile(int accountId) {
        m_fields->m_profileTask.spawn(LocalEndpoint::getWithAuth("profile"),
                                      [self = Ref(this)](Result<matjson::Value> res) {
            if (res.isErr()) {
                log::warn("GarageLayer: {}", res.unwrapErr());
                return;
            }

            auto json = std::move(res).unwrap();
            auto statsOrErr = json.as<RLUserStats>();
            if (statsOrErr.isErr()) {
                log::warn("GarageLayer: Unable to parse stats: {}", statsOrErr.unwrapErr());
                return;
            }

            RLUserStats stats = statsOrErr.unwrap();
            CachedSettings::update()->userData.init(stats);
            self->updateMenu(stats, /*doLog=*/true);
        });
    }

private:
    void updateStats(RLUserStats stats, bool doLog = false) {
        if (doLog) log::info("Profile data - points: {}, stars: {}", stats.points, stats.stars);
        m_fields->init(stats);
    }

    void updateMenu(RLUserStats stats, bool doLog = false) {
        this->updateStats(stats, doLog);
        return this->updateMenu();
    }

    void updateMenu() {
        auto* menu = m_fields->statMenu;
        if (!menu) {
            log::error("Could not locate stat menu!");
            return;
        }

        // sparks
        auto* starSprite = CCSprite::createWithSpriteFrameName("RL_starMed.png"_spr);
        auto* starsValue =
            StatsDisplayAPI::getNewItem("rl-sparks"_spr, starSprite, m_fields->stars, 0.54f);
        starsValue->setID("rl-stars-value");
        menu->addChild(starsValue);

        // planets
        auto* planetSprite = CCSprite::createWithSpriteFrameName("RL_planetMed.png"_spr);
        auto* planetsValue = StatsDisplayAPI::getNewItem(
            "planets-collected"_spr, planetSprite, m_fields->planets, 0.54f);
        planetsValue->setID("rl-planets-value");
        menu->addChild(planetsValue);

        // coins
        auto* coinsSprite = CCSprite::createWithSpriteFrameName("RL_BlueCoinSmall.png"_spr);
        auto* coinsValue =
            StatsDisplayAPI::getNewItem("coins-collected"_spr, coinsSprite, m_fields->coins, 0.54f);
        coinsValue->setID("rl-coins-value");
        menu->addChild(coinsValue);

        // votes
        auto* votesSprite = CCSprite::createWithSpriteFrameName("RL_commVote01.png"_spr);
        auto* votesValue =
            StatsDisplayAPI::getNewItem("votes-collected"_spr, votesSprite, m_fields->votes, 0.54f);
        votesValue->setID("rl-votes-value");
        menu->addChild(votesValue);

        if (m_fields->points > 0) {
            // points
            auto* pointsSprite = CCSprite::createWithSpriteFrameName("RL_blueprintPoint01.png"_spr);
            auto* pointsValue = StatsDisplayAPI::getNewItem(
                "points-collected"_spr, pointsSprite, m_fields->points, 0.54f);
            pointsValue->setID("rl-points-value");
            menu->addChild(pointsValue);
        }

        menu->updateLayout();
    }
};
