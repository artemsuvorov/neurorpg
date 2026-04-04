#include "Precompiled.h"
#include "Logging.h"


#ifdef NR_ENABLE_LOGGING


#include <spdlog/async.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/msvc_sink.h>


namespace Neuro::Core::Log {

	void Init()
	{
		constexpr uint32_t kQueueSize = 8 * 1024;
		constexpr uint32_t kThreadCount = 1;
		constexpr spdlog::async_overflow_policy kOverflowPolicy = spdlog::async_overflow_policy::block;

#ifdef _MSC_VER
		auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		auto msvc = std::make_shared<spdlog::sinks::msvc_sink_mt>();
		std::initializer_list<spdlog::sink_ptr> sinks{console, msvc};
#else
		auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		std::initializer_list<spdlog::sink_ptr> sinks{console};
#endif

		spdlog::init_thread_pool(kQueueSize, kThreadCount);

		std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::async_logger>(
		    "Neuro", sinks.begin(), sinks.end(), spdlog::thread_pool(), kOverflowPolicy);

		spdlog::set_default_logger(logger);

		spdlog::set_pattern("[%T] [%^%l%$] %v");

#ifdef NR_LOGGING_LEVEL
		spdlog::set_level(static_cast<spdlog::level::level_enum>(NR_LOGGING_LEVEL));
#else
		spdlog::set_level(spdlog::level::trace);
#endif

		spdlog::flush_on(spdlog::level::err);
	}


	void Shutdown()
	{
		spdlog::shutdown();
	}

}  // namespace Neuro::Core::Log


#endif
