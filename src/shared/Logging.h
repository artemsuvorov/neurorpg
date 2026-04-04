#pragma once


#ifdef NR_ENABLE_LOGGING

#include <spdlog/spdlog.h>


namespace Neuro::Core::Log {

	void Init();
	void Shutdown();


	template <typename... TArgs>
	inline void Info(spdlog::format_string_t<TArgs...> fmt, TArgs&&... args)
	{
		spdlog::info(fmt, std::forward<TArgs>(args)...);
	}


	template <typename... TArgs>
	inline void Warn(spdlog::format_string_t<TArgs...> fmt, TArgs&&... args)
	{
		spdlog::warn(fmt, std::forward<TArgs>(args)...);
	}


	template <typename... TArgs>
	inline void Error(spdlog::format_string_t<TArgs...> fmt, TArgs&&... args)
	{
		spdlog::error(fmt, std::forward<TArgs>(args)...);
	}

}  // namespace Neuro::Core::Log


#define NR_INFO(...) ::Neuro::Core::Log::Info(__VA_ARGS__)
#define NR_WARN(...) ::Neuro::Core::Log::Warn(__VA_ARGS__)
#define NR_ERROR(...) ::Neuro::Core::Log::Error(__VA_ARGS__)

#else

namespace Neuro::Core::Log {

	inline void Init() {}
	inline void Shutdown() {}


	template <typename... TArgs>
	inline void Info(TArgs&&...)
	{
	}


	template <typename... TArgs>
	inline void Warn(TArgs&&...)
	{
	}


	template <typename... TArgs>
	inline void Error(TArgs&&...)
	{
	}

}  // namespace Neuro::Core::Log


#define NR_INFO(...) ((void)0)
#define NR_WARN(...) ((void)0)
#define NR_ERROR(...) ((void)0)

#endif
