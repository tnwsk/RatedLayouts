#include "RLAchievements.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/terminate.hpp>

using namespace geode::prelude;
using namespace RLAchievements;

// TODO: Look into achievements randomly resetting

#if RL_DICTIONARY_ENABLED
static CCDictionary* s_achievementsDict = nullptr;
#endif // RL_DICTIONARY_ENABLED

static constexpr Achievement s_sparkAchievements[] {
    {"spark_1", "First Spark of Hope", "Collect your First Spark", Collectable::Sparks, 1, "RL_starBig.png"_spr},
    {"spark_50", "Into the Night Sky", "Collected 50 Sparks", Collectable::Sparks, 50, "RL_starBig.png"_spr},
    {"spark_100", "Getting on pace", "Collected 100 Sparks", Collectable::Sparks, 100, "RL_starBig.png"_spr},
    {"spark_250", "Spark Overture", "Collected 250 Sparks", Collectable::Sparks, 250, "RL_starBig.png"_spr},
    {"spark_500", "Many Shiny Sparkles", "Collected 500 Sparks", Collectable::Sparks, 500, "RL_starBig.png"_spr},
    {"spark_750", "Lots Beautiful Sparkles", "Collected 750 Sparks", Collectable::Sparks, 750, "RL_starBig.png"_spr},
    {"spark_1000", "So many Sparkles…", "Collected 1000 Sparks", Collectable::Sparks, 1000, "RL_starBig.png"_spr},
    {"spark_2500", "Yum Yum Sparks", "Collected 2500 Sparks", Collectable::Sparks, 2500, "RL_starBig.png"_spr},
    {"spark_5000", "ExefMn would be proud", "Collected 5000 Sparks", Collectable::Sparks, 5000, "RL_starBig.png"_spr},
    {"spark_7500", "Sparks on the Galaxy", "Collected 7500 Sparks", Collectable::Sparks, 7500, "RL_starBig.png"_spr},
    {"spark_10000", "Road to 10K", "Collected 10000 Sparks", Collectable::Sparks, 10000, "RL_starBig.png"_spr},
    {"spark_12000", "More than that", "Collected 12000 Sparks", Collectable::Sparks, 12000, "RL_starBig.png"_spr},
    {"spark_14000", "14K Sta- I mean SPARKS!", "Collected 14000 Sparks", Collectable::Sparks, 14000, "RL_starBig.png"_spr},
    {"spark_16000", "Lots of Sparks", "Collected 16000 Sparks", Collectable::Sparks, 16000, "RL_starBig.png"_spr},
    {"spark_18000", "Imagine counting all that", "Collected 18000 Sparks", Collectable::Sparks, 18000, "RL_starBig.png"_spr},
    {"spark_20000", "Laniakea Supercluster", "Collected 20000 Sparks", Collectable::Sparks, 20000, "RL_starBig.png"_spr},
    {"spark_25000", "End of the Spark Road", "Collected 25000 Sparks", Collectable::Sparks, 25000, "RL_starBig.png"_spr}
};
static constexpr Achievement s_planetAchievements[] {
    {"planet_1", "That's no Moon", "Collect your First Planet", Collectable::Planets, 1, "RL_planetBig.png"_spr},
    {"planet_10", "Is a planet", "Collected 10 Planets", Collectable::Planets, 10, "RL_planetBig.png"_spr},
    {"planet_25", "Planet Novice", "Collected 25 Planets", Collectable::Planets, 25, "RL_planetBig.png"_spr},
    {"planet_50", "A thing with rings", "Collected 50 Planets", Collectable::Planets, 50, "RL_planetBig.png"_spr},
    {"planet_75", "MORE ROOMS LEVELS!", "Collected 75 Planets", Collectable::Planets, 75, "RL_planetBig.png"_spr},
    {"planet_250", "Saturn Cows", "Collected 250 Planets", Collectable::Planets, 250, "RL_planetBig.png"_spr},
    {"planet_500", "Uranus has rings", "Collected 500 Planets", Collectable::Planets, 500, "RL_planetBig.png"_spr},
    {"planet_750", "Needling my beloved", "Collected 750 Planets", Collectable::Planets, 750, "RL_planetBig.png"_spr},
    {"planet_1000", "Dragonix's Grindset", "Collected 1000 Planets", Collectable::Planets, 1000, "RL_planetBig.png"_spr},
    {"planet_2500", "Need more platformers...", "Collected 2500 Planets", Collectable::Planets, 2500, "RL_planetBig.png"_spr},
    {"planet_5000", "31 Jolly Planets", "Collected 5000 Planets", Collectable::Planets, 5000, "RL_planetBig.png"_spr},
    {"planet_7500", "I have a small galaxy", "Collected 7500 Planets", Collectable::Planets, 7500, "RL_planetBig.png"_spr},
    {"planet_10000", "Found my Dad!", "Collected 10000 Planets", Collectable::Planets, 10000, "RL_planetBig.png"_spr}
};
static constexpr Achievement s_coinAchievements[] {
    {"coin_5", "Did they just paint it blue?!", "Collected 5 Blue Coin", Collectable::Coins, 5, "RL_BlueCoinUI.png"_spr},
    {"coin_10", "These can come in handy", "Collected 10 Blue Coins", Collectable::Coins, 10, "RL_BlueCoinUI.png"_spr},
    {"coin_25", "Nailed it!", "Collected 25 Blue Coins", Collectable::Coins, 25, "RL_BlueCoinUI.png"_spr},
    {"coin_50", "MLG Coin Pro", "Collected 50 Blue Coins", Collectable::Coins, 50, "RL_BlueCoinUI.png"_spr},
    {"coin_100", "Da wae to the Blue Coins", "Collected 100 Blue Coins", Collectable::Coins, 100, "RL_BlueCoinUI.png"_spr},
    {"coin_200", "Why did you scratch this Rob", "Collected 200 Blue Coins", Collectable::Coins, 200, "RL_BlueCoinUI.png"_spr},
    {"coin_300", "My coins are kinda blueless", "Collected 300 Blue Coins", Collectable::Coins, 300, "RL_BlueCoinUI.png"_spr},
    {"coin_400", "I’m feeling Blue", "Collected 400 Blue Coins", Collectable::Coins, 400, "RL_BlueCoinUI.png"_spr},
    {"coin_500", "High as the Blue Sky", "Collected 500 Blue Coins", Collectable::Coins, 500, "RL_BlueCoinUI.png"_spr}
};
static constexpr Achievement s_blueprintAchievements[] {
    {"points_1", "I forgot how to deco.", "Get a Rated Layout", Collectable::Points, 1, "RL_blueprintPoint01.png"_spr},
    {"points_5", "Layouter ", "Collected 5 Blueprint Points", Collectable::Points, 5, "RL_blueprintPoint01.png"_spr},
    {"points_10", "Certified GLC", "Collected 10 Blueprint Points", Collectable::Points, 10, "RL_blueprintPoint01.png"_spr},
    {"points_25", "Visuals are 0%", "Collected 25 Blueprint Points", Collectable::Points, 25, "RL_blueprintPoint01.png"_spr},
    {"points_50", "Reached Flow-state", "Collected 50 Blueprint Points", Collectable::Points, 50, "RL_blueprintPoint01.png"_spr},
    {"points_75", "Revv-olutionary!", "Collected 75 Blueprint Points", Collectable::Points, 75, "RL_blueprintPoint01.png"_spr},
    {"points_100", "Noob turned Pro", "Collected 100 Blueprint Points", Collectable::Points, 100, "RL_blueprintPoint01.png"_spr}
};
static constexpr Achievement s_voteAchievements[] {
    {"vote_1", "Civic Layout Duty", "Submit your first vote on a Layout", Collectable::Votes, 1, "RL_commVote01.png"_spr},
    {"vote_10", "Democracy in Layouts", "Submit 10 votes on Layouts", Collectable::Votes, 10, "RL_commVote01.png"_spr},
    {"vote_50", "Layout Pollster", "Submit 50 votes on Layouts", Collectable::Votes, 50, "RL_commVote01.png"_spr},
    {"vote_100", "Holding an Election", "Submit 100 votes on Layouts", Collectable::Votes, 100, "RL_commVote01.png"_spr},
    {"vote_250", "Helping out the Poor", "Submit 250 votes on Layouts", Collectable::Votes, 250, "RL_commVote01.png"_spr},
    {"vote_500", "You should be a Mod!", "Submit 500 votes on Layouts", Collectable::Votes, 500, "RL_commVote01.png"_spr},
    {"vote_1000", "Can I NOW rate layouts?", "Submit 1000 votes on Layouts", Collectable::Votes, 1000, "RL_commVote01.png"_spr}
};
static constexpr Achievement s_miscAchievements[] {
    {"misc_begin", "It Rating Out Time", "Play a Rated Layout for the first time", Collectable::Misc, 1, "RL_btn01.png"_spr},
    {"misc_finish", "Laying Out!", "Complete a Rated Layout for the first time", Collectable::Misc, 1, "RL_btn01.png"_spr},
    {"misc_news", "Layouts Out Loud", "Check the Rated Layouts Announcement", Collectable::Misc, 1, "RL_news01.png"_spr},
    {"misc_credits", "Our Beautiful Team", "View the Rated Layouts Credits", Collectable::Misc, 1, "RL_creditsIcon.png"_spr},
    {"misc_ruby", "Cha-ching!", "Purchase your first cosmetic in the Ruby Shop", Collectable::Misc, 1, "RL_rubiesIcon.png"_spr},
    {"misc_leaderboard", "Best of the Best", "Be on the Leaderboard", Collectable::Misc, 1, "rankIcon_top500_001.png"},
    {"misc_custom_bg", "Personal Stylist", "Set a Custom Background or Ground", Collectable::Misc, 1, "bgIcon_01_001.png"},
    {"misc_creator_1", "A Fellow Creator", "Talk to the Layout Creator for the first time", Collectable::Misc, 1, "RL_bob.png"_spr},
    {"misc_creator_25", "Layout entrepreneur", "Talk to the Layout Creator 25 times", Collectable::Misc, 1, "RL_bob.png"_spr},
    {"misc_creator_50", "Nosey Creator", "Talk to the Layout Creator 50 times", Collectable::Misc, 1, "RL_bob.png"_spr},
    {"misc_creator_100", "Business Layout Creator", "Talk to the Layout Creator 100 times", Collectable::Misc, 1, "RL_bob.png"_spr},
    {"misc_browser", "www.ratedlayouts.com", "Browse the Rated Layouts Website", Collectable::Misc, 1, "RL_browser01.png"_spr},
    {"misc_salt", "SALT finally rated", "but is it verified?", Collectable::Misc, 1, "RL_bob.png"_spr},
    {"misc_arcticwoof", "Find the Woof", "Find the Rated Layouts Creator", Collectable::Misc, 1, "RL_arcticwoof01.png"_spr},
    {"misc_gauntlet", "Gauntlet Conqueror", "Complete a Layout Gauntlet", Collectable::Misc, 1, "RL_gauntlet-2.png"_spr},
    {"misc_spire", "Something has aligned...", "You now know about it, be careful about where you go…", Collectable::Misc, 1, "RL_planetBig.png"_spr},
    {"misc_code", "Cosmos has accepted you", "Redeem a code to the Cosmos", Collectable::Misc, 1, "RL_oracle.png"_spr},
    {"misc_room", "Trespasser", "Complete a Spire Room.", Collectable::Misc, 1, "RL_spireDoor_unlocked.png"_spr},
    {"misc_extreme", "TOP ONE LAYOUT LIST!", "Complete an Extreme Demon Rated Layout", Collectable::Misc, 1, "diffIcon_10_btn_001.png"},
    {"misc_goog", "GOOG CAT!!!", "Find the goog cat dialogue...", Collectable::Misc, 1, "RL_goog.png"_spr},
    {"misc_gem", "Get Gem To Win Experience", "Beat the level 'Get Gem To Win'", Collectable::Misc, 1, "GJ_bigDiamond_noShadow_001.png"}
};

static constexpr AchievementData s_allAchievements {{
    AchievementList(s_sparkAchievements),
    AchievementList(s_planetAchievements),
    AchievementList(s_coinAchievements),
    AchievementList(s_blueprintAchievements),
    AchievementList(s_voteAchievements),
    AchievementList(s_miscAchievements)
}};

static CCSprite* makeSprite(const std::string& frameName) {
    if (frameName.empty()) return nullptr;
    auto frame = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frameName.c_str());
    if (frame) {
        return CCSprite::createWithSpriteFrameName(frameName.c_str());
    }
    CCTexture2D* tex = CCTextureCache::sharedTextureCache()->addImage(frameName.c_str(), false);
    if (tex) {
        return CCSprite::createWithTexture(tex);
    }
    return nullptr;
}

// TODO: Make this a map? (why is it a vector??)
static const std::vector<Achievement>* getAchievements() {
    // id, name, desc, type, amount, sprite
    static std::vector<Achievement> achievements = []() {
        std::vector<Achievement> out;
        for (AchievementList list : s_allAchievements)
            out.append_range(list);
        return out;
    }();
    return &achievements;
}

static CCDictionary* makeCCDictFromAchievement(const Achievement& ach) {
    auto entry = CCDictionary::create();
    entry->setObject(CCString::create(ach.id.data()), "id");
    entry->setObject(CCString::create(ach.name.data()), "name");
    entry->setObject(CCString::create(ach.desc.data()), "desc");
    // store type and amount as strings for compatibility
    entry->setObject(CCString::create(numToString(int(ach.type))), "type");
    entry->setObject(CCString::create(numToString(int(ach.amount))), "amount");
    entry->setObject(CCString::create(ach.sprite.data()), "sprite");
    return entry;
}

void RLAchievements::init() {
    static int once = []() -> int {
        auto* achievements = getAchievements();
#if RL_DICTIONARY_ENABLED
        s_achievementsDict = CCDictionary::create();
        s_achievementsDict->retain();
        for (auto const& ach : *achievements) {
            auto* entry = makeCCDictFromAchievement(ach);
            s_achievementsDict->setObject(entry, ach.id);
        }
#endif // RL_DICTIONARY_ENABLED
        log::info("RLAchievements initialized with {} achievements", achievements->size());
        return 0;
    }();
}

static std::string saveKeyFor(std::string_view id) {
    return fmt::format("achieved_{}", id);
}

// TODO: Check if Collectable::Misc should return early

void RLAchievements::onUpdated(Collectable type, int oldVal, int newVal) {
    RLAchievements::init();

    log::debug("RLAchievements::onUpdated(type={}, oldVal={}, newVal={})", static_cast<int>(type), oldVal, newVal);

    if (newVal <= oldVal) {
        log::debug("Ignoring update because newVal ({}) <= oldVal ({})", newVal, oldVal);
        return;  // only consider increases
    }

    AchievementList list = s_allAchievements[type];
    for (auto const& ach : list) {
        if (newVal < ach.amount) continue;  // not reached yet
        std::string idKey = saveKeyFor(ach.id);
        int awarded = Mod::get()->getSavedValue<int>(idKey, 0);
        if (awarded) {
            log::debug("Achievement {} (id={}) already awarded (save={})", ach.name, ach.id, awarded);
            continue;
        }
        if (oldVal >= ach.amount) {
            log::debug("Player already had achievement {} (id={}) based on oldVal {}", ach.name, ach.id, oldVal);
            continue;  // already had it
        }

        // Candidate reached
        log::info("Awarding achievement {} (id={}) amount={} (oldVal={}, newVal={})", ach.name, ach.id, ach.amount, oldVal, newVal);

        // Mark awarded
        Mod::get()->setSavedValue<int>(idKey, 1);
        int verify = Mod::get()->getSavedValue<int>(idKey, 0);
        log::debug("Saved value for {} is now {}", idKey, verify);

        // give achievement thingy
        log::info("Notifying achievement {} via AchievementNotifier", ach.id);
        AchievementNotifier::sharedState()
            ->notifyAchievement(ach.name.data(), ach.desc.data(), ach.sprite.data(), true);
    }
}

void RLAchievements::checkAll(Collectable type, int currentVal) {
    RLAchievements::init();

    log::debug("RLAchievements::checkAll(type={}, currentVal={})", static_cast<int>(type), currentVal);

    AchievementList list = s_allAchievements[type];
    for (auto const& ach : list) {
        if (currentVal < ach.amount) continue;  // not reached yet
        std::string idKey = saveKeyFor(ach.id);
        int awarded = Mod::get()->getSavedValue<int>(idKey, 0);
        if (awarded) {
            log::debug("Achievement {} (id={}) already awarded (save={})", ach.name, ach.id, awarded);
            continue;
        }

        // Award retroactively
        log::info("Retroactively awarding achievement {} (id={}) amount={} (currentVal={})", ach.name, ach.id, ach.amount, currentVal);
        Mod::get()->setSavedValue<int>(idKey, 1);
        int verify = Mod::get()->getSavedValue<int>(idKey, 0);
        log::debug("Saved value for {} is now {}", idKey, verify);
        log::info("Notifying achievement {} via AchievementNotifier (retro)", ach.id);
        AchievementNotifier::sharedState()
            ->notifyAchievement(ach.name.data(), ach.desc.data(), ach.sprite.data(), true);
    }
}

std::vector<Achievement> const& RLAchievements::getAll() {
    RLAchievements::init();
    return *getAchievements();
}

AchievementData const& RLAchievements::getAllByType() {
    RLAchievements::init();
    return s_allAchievements;
}

bool RLAchievements::isAchieved(std::string const& id) {
    return Mod::get()->getSavedValue<int>(saveKeyFor(id), 0) != 0;
}

#if RL_DICTIONARY_ENABLED
CCDictionary* RLAchievements::getAllAsDictionary() {
    RLAchievements::init();
    return s_achievementsDict;
}

CCDictionary* RLAchievements::getAchievementDictionary(std::string const& id) {
    auto dict = getAllAsDictionary();
    if (!dict) return nullptr;
    return static_cast<CCDictionary*>(dict->objectForKey(id));
}
#endif // RL_DICTIONARY_ENABLED

void RLAchievements::onReward(std::string const& id) {
    RLAchievements::init();

    // find achievement details by id
    const Achievement* data = nullptr;
    for (auto const& ach : *getAchievements()) {
        if (ach.id == id) {
            data = &ach;
            break;
        }
    }

    if (data == nullptr) {
        log::warn("Achievement '{}' does not exist.", id);
        return;
    }

    std::string_view name = data->name;
    std::string_view desc = data->desc;
    std::string_view sprite = data->sprite;
    log::debug("RLAchievements::onReward(id={}, name={})", id, name);

    if (RLAchievements::isAchieved(id)) {
        log::debug("Misc achievement {} already awarded", id);
        return;
    }

    // Mark awarded
    std::string idKey = saveKeyFor(id);
    Mod::get()->setSavedValue<int>(idKey, 1);
    int verify = Mod::get()->getSavedValue<int>(idKey, 0);
    log::info("Awarded misc achievement {} (verify={})", id, verify);

    // Notify player
    AchievementNotifier::sharedState()
        ->notifyAchievement(name.data(), desc.data(), sprite.data(), true);
}
