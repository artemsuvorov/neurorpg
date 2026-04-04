#pragma once

#ifdef NR_ENABLE_ASSERTS

#include <cstdlib>


#if defined(_MSC_VER)
#define NR_DBG_BREAK() __debugbreak()
#else
#include <signal.h>
#define NR_DBG_BREAK() raise(SIGTRAP)
#endif


// Internal, always expects a message.
#define NR_ASSERT_IMPL(type, check, msg, ...)    \
	do                                           \
	{                                            \
		if (!(check))                            \
		{                                        \
			NR##type##ERROR(msg, ##__VA_ARGS__); \
			NR_DBG_BREAK();                      \
			std::abort();                        \
		}                                        \
	} while (0)


// For Debug only.
#define NR_DBG_ASSERT_MSG(check, msg, ...) NR_ASSERT_IMPL(_, check, msg, __VA_ARGS__)
#define NR_DBG_ASSERT(check) NR_ASSERT_IMPL(_, check, "Assertion failed: '{}'! ({}:{})", #check, __FILE__, __LINE__)

// For Rebug and Release.
#define NR_DEV_ASSERT_MSG(check, msg, ...) NR_ASSERT_IMPL(_, check, msg, __VA_ARGS__)
#define NR_DEV_ASSERT(check) NR_ASSERT_IMPL(_, check, "Assertion failed: '{}'! ({}:{})", #check, __FILE__, __LINE__)

// Conditionally disable debug asserts.
#ifdef NDEBUG
#undef NR_DBG_ASSERT_MSG
#undef NR_DBG_ASSERT
#define NR_DBG_ASSERT_MSG(check, ...) ((void)0)
#define NR_DBG_ASSERT(check) ((void)0)
#endif

#else

#define NR_DBG_ASSERT_MSG(check, ...) ((void)0)
#define NR_DBG_ASSERT(check) ((void)0)
#define NR_DEV_ASSERT_MSG(check, ...) ((void)0)
#define NR_DEV_ASSERT(check) ((void)0)
#define NR_DBG_BREAK() ((void)0)

#endif
