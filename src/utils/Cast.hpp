#pragma once

#include <concepts>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <Geode/utils/casts.hpp>
#include <Geode/utils/cocos.hpp>
#include <Geode/utils/terminate.hpp>
#include "RLConfig.hpp"

#if RL_DEV
# define RL_ASSERT_IMPL(EXPR, ...)        \
     do {                                 \
         if (!EXPR) [[unlikely]]          \
             rl::assertFail(__VA_ARGS__); \
     } while (false)
#else
# define RL_ASSERT_IMPL(EXPR, ...) ((void)0)
#endif

#define RL_ASSERT(EXPR, ...) RL_ASSERT_IMPL((EXPR), #EXPR, __func__ __VA_OPT__(, ) __VA_ARGS__)

namespace rl {

// TODO: Move this somewhere else.
[[noreturn]] RL_ERROR_ATTR void assertFail(const char* expr, const char* func, const char* reason = nullptr);

namespace casts {

void castFail(const cocos2d::CCNode* node, std::type_info const& expected);
[[noreturn]] void castTerminate(const cocos2d::CCNode* node, std::type_info const& expected);

template <typename ExpectedT>
RL_ERROR_ATTR inline void castFail(const cocos2d::CCNode* node) {
    return castFail(node, typeid(ExpectedT));
}

template <typename ExpectedT>
[[noreturn]] RL_ERROR_ATTR inline void castTerminate(const cocos2d::CCNode* node) {
    castTerminate(node, typeid(ExpectedT));
}

////////////////////////////////////////////////////////////////////////////////
// Traits

template <class Derived, class Base>
concept is_derived_i = std::is_base_of_v<Base, Derived>;

template <class T>
concept is_polymorphic_i = std::is_polymorphic_v<T>;

template <class Derived, class Base>
concept is_derived = is_derived_i<Derived, Base>;

template <class T>
concept is_polymorphic = is_polymorphic_i<T>;

template <class T, class U>
concept both_polymorphic = is_polymorphic<T> && is_polymorphic<U>;

template <class T>
concept is_ccnode = is_derived<cocos2d::CCNode, T>;

template <class T, class U>
concept both_ccnode = is_ccnode<T> && is_ccnode<U>;

template <class T>
concept is_modify = geode::geode_internal::IsModifyClass<T>;

template <class T>
concept not_modify = !is_modify<T>;

////////////////////////////////////////////////////////////////////////////////
// Utility

template <typename To, typename From>
struct copy_const {
    using type = To;
};

template <typename To, typename From>
struct copy_const<To, const From> {
    using type = const To;
};

template <typename To, typename From>
using copy_const_t = typename copy_const<To, From>::type;

template <typename D, typename Base>
Base& get_modify_type_impl(geode::Modify<D, Base> const&);

template <is_modify ModifyT>
struct get_modify_type {
    using type_ = decltype(get_modify_type_impl<ModifyT>(std::declval<ModifyT&>()));
    using type = std::remove_reference_t<type_>;
};

template <is_modify ModifyT>
using get_modify_type_t = typename get_modify_type<ModifyT>::type;

////////////////////////////////////////////////////////////////////////////////
// Implementation

template <class To, class From, class Base>
struct CastImpl {
    static_assert(false, "CastImpl selection not found.");
};

template <class From>
struct CastImpl<From, From*, From> {
    static constexpr bool isPossible(const From*) { return true; }
    static constexpr const From* doCast(const From* from) { return from; }
};

template <class To, class From>
    requires both_polymorphic<To, From> && not_modify<To>
struct CastImpl<To, From*, From> {
    static bool isPossible(const From* from) {
        return geode::cast::typeinfo_cast<const To*>(from) != nullptr;
    }
    static const To* doCast(const From* from) { return static_cast<const To*>(from); }
};

template <class To, class From>
    requires both_polymorphic<To, From> && is_modify<To>
struct CastImpl<To, From*, From> {
    using Intermediate = get_modify_type_t<To>;
    using impl = CastImpl<Intermediate, From*, From>;
    static bool isPossible(const From* from) { return impl::isPossible(from); }
    RL_ALWAYS_INLINE static const To* doCast(const From* from) {
        return static_cast<const To*>(impl::doCast(from));
    }
};

template <class To, class From>
struct CastInfo {
    static_assert(false, "CastInfo selection not found.");
};

template <class To, class From>
struct CastInfo<To, From*> {
    using impl = CastImpl<To, std::remove_cv_t<From>*, std::remove_cv_t<From>>;

    static bool isPossible(const From* from) { return impl::isPossible(from); }
    static copy_const_t<To, From>* castFailed() { return nullptr; }

    static copy_const_t<To, From>* doCast(const From* from) {
        return const_cast<copy_const_t<To, From>*>(impl::doCast(from));
    }
    static copy_const_t<To, From>* doCastIfPossible(const From* from) {
        if (!impl::isPossible(from)) return CastInfo::castFailed();
        return CastInfo::doCast(from);
    }
};

template <class T>
inline constexpr bool isPresent(const T* val) {
    return val != nullptr;
}

}  // namespace casts

// TODO: Add support for shared_ptr/Ref/WeakRef

template <typename To, typename From>
[[nodiscard]] inline bool isa(From* val) {
    return casts::CastInfo<To, From*>::isPossible(val);
}

template <typename To0, typename To1, typename... Rest, typename From>
[[nodiscard]] inline bool isa(From* val) {
    return isa<To0>(val) || isa<To1>(val) || (isa<Rest>(val) || ...);
}

template <typename To, typename From>
[[nodiscard]] inline decltype(auto) cast(From* val) {
#if RL_DEV
    if (!isa<To>(val)) [[unlikely]]
        casts::castTerminate<To>(val);
#endif
    return casts::CastInfo<To, From*>::doCast(val);
}

template <typename To, typename From>
[[nodiscard]] inline decltype(auto) expect_cast(From* val) {
    RL_ASSERT(casts::isPresent(val), "expect_cast on a non-existent value");
#if RL_DEV
    if (!isa<To>(val)) [[unlikely]] {
        casts::castFail<To>(val);
        return nullptr;
    }
#endif
    return casts::CastInfo<To, From*>::doCast(val);
}

template <typename To, typename From>
[[nodiscard]] inline decltype(auto) dyn_cast(From* val) {
    RL_ASSERT(casts::isPresent(val), "dyn_cast on a non-existent value");
    return casts::CastInfo<To, From*>::doCastIfPossible(val);
}

template <typename To, typename From>
[[nodiscard]] inline decltype(auto) dyn_cast_or_null(From* val) {
    if (!casts::isPresent(val)) return casts::CastInfo<To, From*>::castFailed();
    return casts::CastInfo<To, From*>::doCastIfPossible(val);
}

}  // namespace rl
