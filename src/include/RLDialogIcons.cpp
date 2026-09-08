#include "RLDialogIcons.hpp"
#include "utils/Cast.hpp"

using namespace rl;

static constexpr int CustomDialogIconTag = 0xD1A200;

static CCSprite* GetDialogTagged(DialogLayer* dialog, int tag) {
    return dyn_cast_or_null<CCSprite>(dialog->m_mainLayer->getChildByTag(tag));
}

void rl::setDialogObjectIcon(DialogLayer* dialog, int characterFrame) {
    if (!dialog || !dialog->m_mainLayer || !dialog->m_characterSprite) {
        return;
    }

    dialog->m_characterSprite->setVisible(false);

    // when characterFrame = 0 LITERALLY REMOVES THE SPRITE lol
    int iconFrame = characterFrame - 1;
    if (characterFrame <= 0) {
        iconFrame = 0;
    }
    if (iconFrame < 0 || iconFrame >= DialogIconCount) {
        iconFrame = 0;
    }

    for (int frame = 0; frame < DialogIconCount; frame++) {
        int tag = DialogIconTagBase + frame;
        auto* icon = GetDialogTagged(dialog, tag);
        if (!icon) {
            auto frameName = fmt::format("RL_dialogIcon_{:02}.png"_spr, frame);
            icon = CCSprite::createWithSpriteFrameName(frameName.c_str());
            if (!icon) {
                continue;
            }
            icon->setPosition(dialog->m_characterSprite->getPosition());
            icon->setTag(tag);
            dialog->m_mainLayer->addChild(icon, 3);
        } else {
            icon->setPosition(dialog->m_characterSprite->getPosition());
        }

        icon->setVisible(frame == iconFrame);
    }
}

void rl::setDialogObjectCustomIcon(DialogLayer* dialog, const std::string& frameName) {
    if (!dialog || !dialog->m_mainLayer || !dialog->m_characterSprite) {
        return;
    }

    dialog->m_characterSprite->setVisible(false);

    for (int frame = 0; frame < DialogIconCount; frame++) {
        int tag = DialogIconTagBase + frame;
        if (auto* existing = GetDialogTagged(dialog, tag)) {
            existing->setVisible(false);
        }
    }

    auto* icon = GetDialogTagged(dialog, CustomDialogIconTag);
    if (!icon) {
        icon = CCSprite::createWithSpriteFrameName(frameName.c_str());
        if (!icon) {
            return;
        }
        icon->setTag(CustomDialogIconTag);
        dialog->m_mainLayer->addChild(icon, 3);
    }

    icon->setPosition(dialog->m_characterSprite->getPosition());
    icon->setVisible(true);
}

void rl::setDialogObjectCustomIcon(DialogLayer* dialog, cocos2d::CCNode* icon) {
    if (!dialog || !icon || !dialog->m_mainLayer || !dialog->m_characterSprite) {
        return;
    }

    CCSprite* sprite = dialog->m_characterSprite;
    sprite->setVisible(false);

    for (int frame = 0; frame < DialogIconCount; frame++) {
        int tag = DialogIconTagBase + frame;
        if (auto* existing = GetDialogTagged(dialog, tag)) {
            existing->setVisible(false);
        }
    }

    icon->setTag(CustomDialogIconTag);
    dialog->m_mainLayer->addChild(icon, 3);

    CCSize spriteSize = sprite->getScaledContentSize();
    CCSize iconSize = icon->getScaledContentSize();

    icon->setPosition(sprite->getPosition());
    icon->setAnchorPoint(sprite->getAnchorPoint());
    icon->setScale(spriteSize.width / iconSize.width, spriteSize.height / iconSize.height);
    icon->setVisible(true);
}
