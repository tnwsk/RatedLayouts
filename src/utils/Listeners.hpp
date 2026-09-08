#pragma once

#include <Geode/loader/Event.hpp>
#include <Geode/loader/SettingV3.hpp>
#include "utils/Cast.hpp"

namespace rl {

template <typename T>
using map_setting_type_t = typename geode::SettingTypeForValueType<T>::SettingType;

template <typename T>
using map_setting_node_t = geode::SettingValueNodeV3<map_setting_type_t<T>>;

template <typename T>
inline map_setting_type_t<T>* setting_cast(geode::SettingV3* setting) {
    using Ty = map_setting_type_t<T>;
    return geode::cast::typeinfo_cast<Ty*>(setting);
}

template <typename T>
inline const map_setting_type_t<T>* setting_cast(const geode::SettingV3* setting) {
    using Ty = map_setting_type_t<T>;
    return geode::cast::typeinfo_cast<const Ty*>(setting);
}

template <typename T>
inline auto setting_cast(std::shared_ptr<geode::SettingV3> setting) {
    using Ty = map_setting_type_t<T>;
    return geode::cast::typeinfo_pointer_cast<Ty>(setting);
}

template <typename T>
inline map_setting_node_t<T>* setting_node_cast(geode::SettingNodeV3* setting) {
    using Ty = map_setting_node_t<T>;
    return geode::cast::typeinfo_cast<Ty*>(setting);
}

template <typename T>
inline const map_setting_node_t<T>* setting_node_cast(const geode::SettingNodeV3* setting) {
    using Ty = map_setting_node_t<T>;
    return geode::cast::typeinfo_cast<const Ty*>(setting);
}

template <class T, class Callback>
inline geode::comm::ListenerHandle makeSettingListener(std::string settingKey,
                                                       Callback&& callback,
                                                       int priority = 0) {
    return geode::SettingChangedEventV3(geode::getMod(), std::move(settingKey))
        .listen([callback = std::move(callback)](std::shared_ptr<geode::SettingV3> setting) {
        if (auto ty = rl::setting_cast<T>(setting)) {
            callback(ty->getValue());
        }
    }, priority);
}

}  // namespace rl
