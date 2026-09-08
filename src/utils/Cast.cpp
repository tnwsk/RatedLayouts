#include "utils/Cast.hpp"
#include <Geode/loader/Log.hpp>
#include <Geode/utils/terminate.hpp>
#include <fmt/format.h>

using namespace geode::prelude;
using namespace rl;

static std::string formatAssertReason(const char* expr, const char* reason) {
    if (reason)
        return fmt::format("`{}`. Reason: {}", expr, reason);
    else
        return fmt::format("`{}`", expr);
}

void rl::assertFail(const char* expr, const char* func, const char* reason) {
    std::string exprAndReason = formatAssertReason(expr ?: "<null>", reason);
    utils::terminate(fmt::format("Assertion failed (in {}): {}", func, exprAndReason));
}

static std::string formatNodeInfo(const cocos2d::CCNode* node, std::type_info const& expected) {
    if (!node) return fmt::format("Cast failed: <node> is null");
    const char* nodeType = cast::getRuntimeTypeName(node);
    const char* expectedType = cast::getRuntimeTypeName(expected);
    if (auto id = const_cast<cocos2d::CCNode*>(node)->getID(); !id.empty())
        return fmt::format("Cast failed: [{}] {} != expected {}", id, nodeType, expectedType);
    else
        return fmt::format("Cast failed: {} != expected {}", nodeType, expectedType);
}

void casts::castFail(const cocos2d::CCNode* node, std::type_info const& expected) {
    log::error("{}", formatNodeInfo(node, expected));
}

void casts::castTerminate(const cocos2d::CCNode* node, std::type_info const& expected) {
    utils::terminate(formatNodeInfo(node, expected));
}
