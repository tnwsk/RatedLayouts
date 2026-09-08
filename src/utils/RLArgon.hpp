#pragma once

#include <string>
#include <arc/future/Future.hpp>
#include <arc/sync/Notify.hpp>
#include <Geode/Result.hpp>
#include <Geode/utils/async.hpp>
//#include <argon/argon.hpp>

namespace rl {

struct RLArgon {
    using ResultType = geode::Result<std::string>;
    using ArgonTaskType = geode::async::TaskHolder<ResultType>;
    using ResFuture = arc::Future<ResultType>;

    /// Handles authorization.
    /// @returns Notifies when validation is complete.
    static void authorize(bool forceStrong = false);

    /// Wait for auth to complete.
    static void wait();
    /// Wait for auth to complete asynchronously.
    static arc::Future<> waitAsync();
    /// Wait for auth with a result.
    static ResultType resolve();
    /// Wait for auth with a result asynchronously.
    static ResFuture resolveAsync();
    /// Clear the cached token.
    static void clear();
    /// Gets the cached token.
    static std::string token();
    /// If the token is valid.
    static bool hasToken();

    /// If auth failed.
    static bool failed();
    /// Notifies if failure occurred.
    static bool notifyFailed();
    /// Gets the failure message.
    static std::string failureMessage();
};

}  // namespace rl
