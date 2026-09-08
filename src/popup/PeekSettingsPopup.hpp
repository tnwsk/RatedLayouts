#pragma once

#include <Geode/loader/Event.hpp>
#include <Geode/ui/Popup.hpp>
#include "RLLayerBackground.hpp"

namespace rl {

class PeekSettingsPopup {
    class Impl;
    PeekSettingsPopup() = delete;

public:
    using Callback = geode::Function<void(RLLayerBackgroundData)>;
    static geode::Popup* create(Callback cb, geode::Mod* mod = geode::getMod());
};

}  // namespace rl
