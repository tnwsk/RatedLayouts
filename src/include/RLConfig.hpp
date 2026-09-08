#pragma once
#include <Geode/platform/cplatform.h>

// Various attribute macros

#ifndef __has_attribute
# define __has_attribute(x) 0
#endif
#ifndef __has_builtin
# define __has_builtin(x) 0
#endif

#if defined(GEODE_IS_WINDOWS)
# define RL_NO_INLINE __declspec(noinline)
# define RL_ALWAYS_INLINE __forceinline
#else
# define RL_NO_INLINE __attribute__((noinline))
# define RL_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

#if __has_attribute(cold)
# define RL_ATTR_COLD __attribute__((cold))
#else
# define RL_ATTR_COLD
#endif
#if __has_attribute(preserve_most)
# define RL_ATTR_PRESERVE_MOST __attribute__((preserve_most))
#else
# define RL_ATTR_PRESERVE_MOST
#endif

#define RL_ERROR_ATTR RL_NO_INLINE RL_ATTR_COLD RL_ATTR_PRESERVE_MOST

// Miscellaneous

#define RL_FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

#ifdef __clang__
# define RL_INTERNAL_NS(...) __VA_ARGS__
# define RL_INTERNAL_LINKAGE [[clang::internal_linkage]]
#else
# define RL_INTERNAL_NS(...)
# define RL_INTERNAL_LINKAGE
#endif

#ifndef RL_DEV
# define RL_DEV 0
#endif
