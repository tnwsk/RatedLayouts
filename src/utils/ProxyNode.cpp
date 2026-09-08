#include "utils/ProxyNode.hpp"

using namespace geode::prelude;
using namespace rl;

ProxyNode* ProxyNode::create(geode::Function<void()> callback) {
    if (!callback) return nullptr;

    auto ret = new ProxyNode;
    if (ret->init(std::move(callback))) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

bool ProxyNode::init(geode::Function<void()> callback) {
    if (!CCNode::init()) return false;
    m_callback = std::move(callback);
    return true;
}

void ProxyNode::visit() { m_callback(); }
