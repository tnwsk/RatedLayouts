#include "RLArgon.hpp"
#include <atomic>
#include <optional>
#include <Geode/Result.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/async.hpp>
#include <argon/argon.hpp>
#include <arc/task/Yield.hpp>
#include <arc/time/Sleep.hpp>
#include <asp/sync/SpinLock.hpp>
#include <asp/time/sleep.hpp>
#include "RLConfig.hpp"

using namespace geode::prelude;
using namespace rl;

static std::optional<argon::AccountData> ArgonData;
static std::string ArgonToken;
static async::TaskHolder<Result<std::string>> ArgonTask;
static asp::SpinLock LowContentionLock;
static std::atomic<int> InProgress = {0};

// TODO: Use memory_order?
static std::atomic<bool> DidFail = {false};
static std::optional<std::string> FailureMessage;

namespace {
class AuthProgressRAII {
    bool isChecking;

public:
    AuthProgressRAII() : isChecking(true) {
        InProgress.fetch_add(1, std::memory_order_release);
    }
    ~AuthProgressRAII() {
        if (isChecking) {
            InProgress.fetch_sub(1, std::memory_order_acquire);
        }
    }

    RL_ALWAYS_INLINE AuthProgressRAII(AuthProgressRAII&& that) {
        that.isChecking = false;
        this->isChecking = true;
    }
    RL_ALWAYS_INLINE AuthProgressRAII& operator=(AuthProgressRAII&& that) {
        that.isChecking = false;
        this->isChecking = true;
        return *this;
    }

    AuthProgressRAII(AuthProgressRAII const&) = delete;
    AuthProgressRAII& operator=(AuthProgressRAII const&) = delete;
};
}  // namespace

static std::string failureMessageLockless() {
    if (FailureMessage && !FailureMessage->empty())
        return *FailureMessage;
    else
        return "Argon validation failed.";
}
static RLArgon::ResultType resolveLockless() {
    if (!RLArgon::failed())
        return Ok(ArgonToken);
    else
        return Err(failureMessageLockless());
}
RL_ALWAYS_INLINE static void clearLockless() {
    ArgonData.reset();
    ArgonToken.clear();
}

RL_ALWAYS_INLINE static bool isAuthPending() { return InProgress.load(std::memory_order_acquire) > 0; }

void RLArgon::authorize(bool forceStrong) {
    AuthProgressRAII prog;
    ArgonTask.cancel();
    auto guard = LowContentionLock.lock();
    if (!ArgonToken.empty() && ArgonData && ArgonData->valid()) {
        guard.unlock();
        log::info("Already authorized!");
        return;
    }
    if (!argon::signedIn()) {
        DidFail = true;
        FailureMessage = "Not signed in";
        guard.unlock();
        log::error("Auth failed, not signed in.");
        return;
    }
    // Set up our info
    ArgonData = argon::getGameAccountData();
    argon::AuthOptions opts{
        .progress =
            [](argon::AuthProgress progress) {
                log::debug("Auth progress: {}", argon::authProgressToString(progress));
            },
        .account = ArgonData,
        .forceStrong = forceStrong,
    };
    ArgonTask.spawn(argon::startAuth(std::move(opts)), [prog = std::move(prog)](Result<std::string> res) {
        DidFail.store(res.isErr());
        (void)prog; // Make sure this is used
        auto guard = LowContentionLock.lock();
        if (res.isErr()) {
            argon::clearToken(*ArgonData);
            clearLockless();
            FailureMessage = res.unwrapErr();
            guard.unlock();
            log::warn("Auth failed: {}", res.unwrapErr());
            return;
        } else {
            FailureMessage.reset();
            ArgonToken = std::move(res).unwrap();
            guard.unlock();
            log::info("Auth successful, got token!");
        }
    });
}

void RLArgon::wait() {
    if (!isAuthPending()) return;
    asp::yield();
    int tries = 0;
    while (isAuthPending()) {
        ++tries;
        const auto waitTime = asp::Duration::fromMillis(tries * 250);
        if (waitTime.seconds() > 2) {
            log::error("ArgonTask failed to complete in time");
            return;
        }
        asp::sleep(waitTime);
    }
    log::info("ArgonTask completed in {} tries", tries);
}

arc::Future<> RLArgon::waitAsync() {
    if (!isAuthPending()) co_return;
    co_await arc::yield();
    int tries = 0;
    while (isAuthPending()) {
        ++tries;
        const auto waitTime = asp::Duration::fromMillis(tries * 250);
        if (waitTime.seconds() > 4) {
            log::error("ArgonTask failed to complete in time");
            co_return;
        }
        co_await arc::sleepFor(waitTime);
    }
    log::info("ArgonTask completed in {} tries", tries);
    co_return;
}

RLArgon::ResultType RLArgon::resolve() {
    RLArgon::wait();
    auto guard = LowContentionLock.lock();
    return resolveLockless();
}

RLArgon::ResFuture RLArgon::resolveAsync() {
    auto guard = LowContentionLock.lock();
    if (!isAuthPending()) co_return resolveLockless();
    guard.unlock();
    // Wait for completion
    co_await RLArgon::waitAsync();
    guard.relock();
    co_return resolveLockless();
}

void RLArgon::clear() {
    auto guard = LowContentionLock.lock();
    ArgonTask.cancel();
    clearLockless();
}

std::string RLArgon::token() {
    auto guard = LowContentionLock.lock();
    return ArgonToken;
}

bool RLArgon::hasToken() {
    auto guard = LowContentionLock.lock();
    return !ArgonToken.empty();
}

bool RLArgon::failed() { return DidFail.load(); }

bool RLArgon::notifyFailed() {
    if (!DidFail.load()) return false;
    // TODO: Queue in main thread?
    Notification::create(RLArgon::failureMessage(), NotificationIcon::Error)->show();
    return true;
}

std::string RLArgon::failureMessage() {
    if (!DidFail.load()) return "";
    auto guard = LowContentionLock.lock();
    return failureMessageLockless();
}
