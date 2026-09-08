#include "custom/RLNameplateItem.hpp"
#include <fmt/core.h>
#include "Geode/ui/NineSlice.hpp"
#include "utils/LazyNameplate.hpp"

using namespace geode::prelude;
using namespace rl;

std::map<int, RLNameplateInfo> RLNameplateItem::s_items;

CCMenuItemSpriteExtra* RLNameplateItem::create(RLNameplateInfo const& info,
                                               CCObject* target,
                                               SEL_MenuHandler selector) {
    // create icon either from URL or fallback sprite frame
    const int index = info.index;
    CCNode* iconNode = nullptr;
    CCSize texSize = {40.f, 40.f};

    LazySprite* lazy = nullptr;
    // lazy load remote image
    if (!info.iconUrl.empty()) {
        lazy = LazyIcon::create({40, 40}, info.iconUrl, true);
        lazy->setAutoResize(true);
    } else {
        lazy = LazyNameplate::create({40, 40}, index, true);
    }

    if (lazy) {
        auto* stencil = NineSlice::createWithSpriteFrameName("RL_nameplateIconClip.png"_spr);
        if (stencil) {
            auto clip = CCClippingNode::create(stencil);
            clip->setAnchorPoint({0.5, 0.5});
            clip->setAlphaThreshold(0.01f);
            clip->setContentSize({40.f, 40.f});
            clip->addChild(lazy);
            // Position lazy and stencil in the center of the clipping node
            lazy->setPosition({20.f, 20.f});
            stencil->setPosition({20.f, 20.f});
            iconNode = clip;
        } else {
            iconNode = lazy;
        }
    }

    float width = std::max(texSize.width, 40.f);
    float height = std::max(texSize.height, 40.f);  // room for price label

    // container node used as the menu item's normal image
    auto container = CCSprite::create();
    container->setContentSize({width, height});

    if (iconNode) {
        iconNode->setPosition({width / 2.f, height / 2.f});
        container->addChild(iconNode);
    }

    // price label underneath (show "Owned" when purchased)
    bool owned = isOwned(index);
    auto priceLabel = CCLabelBMFont::create(owned ? "Owned" : fmt::format("{}", info.price).c_str(), "bigFont.fnt");
    priceLabel->setScale(0.4f);
    priceLabel->setPosition({width / 2.f - 6.f, -6.f});
    if (owned) {
        priceLabel->setColor({120, 255, 140});
        priceLabel->setPosition({width / 2.f, -5.f});
    }
    container->addChild(priceLabel);

    // small ruby icon to the right of the price
    if (!owned) {
        auto rubyIcon = CCSprite::createWithSpriteFrameName("RL_rubySmall.png"_spr);
        if (rubyIcon) {
            rubyIcon->setScale(0.8f);
            float priceHalfW = priceLabel->getContentSize().width * priceLabel->getScale() * 0.5f;
            rubyIcon->setPosition({priceLabel->getPositionX() + priceHalfW + 8.f, priceLabel->getPositionY() - 1.f});
            container->addChild(rubyIcon);
        }
    }

    auto item = CCMenuItemSpriteExtra::create(container, target, selector);
    item->setContentSize(container->getContentSize());
    item->setTag(index);

    // register metadata for lookup
    if (!s_items.contains(index)) s_items[index] = info;
    return item;
}

bool RLNameplateItem::getInfo(int index, RLNameplateInfo* out) {
    if (!out) return false;
    auto it = s_items.find(index);
    if (it == s_items.end()) return false;
    *out = it->second;
    return true;
}

static std::filesystem::path ownedPath() { return dirs::getModsSaveDir() / Mod::get()->getID() / "owned_items.json"; }

// TODO: Save in memory

std::vector<int> RLNameplateItem::getOwnedItems() {
    std::vector<int> out;
    auto path = ownedPath();
    auto existing = utils::file::readString(utils::string::pathToString(path));
    if (!existing) return out;
    auto parsed = matjson::parse(existing.unwrap());
    if (!parsed) return out;
    auto root = parsed.unwrap();
    if (root.isArray()) {
        auto arr = root.asArray().unwrap();
        for (auto& v : arr) {
            int n = v.asInt().unwrapOr(-1);
            if (n >= 0) out.push_back(n);
        }
    }
    return out;
}

bool RLNameplateItem::isOwned(int index) {
    auto owned = getOwnedItems();
    for (auto i : owned)
        if (i == index) return true;
    return false;
}

bool RLNameplateItem::markOwned(int index) {
    auto path = ownedPath();
    auto owned = getOwnedItems();
    for (auto i : owned)
        if (i == index) return true;  // already owned
    owned.push_back(index);
    // write simple JSON array
    std::string out = "[";
    for (size_t i = 0; i < owned.size(); ++i) {
        if (i) out += ",";
        out += fmt::format("{}", owned[i]);
    }
    out += "]";
    auto writeRes = utils::file::writeString(utils::string::pathToString(path), out);
    return static_cast<bool>(writeRes);
}
