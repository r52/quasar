#include "log.h"

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Log
{
    std::shared_ptr<spdlog::logger> setup_logger(std::vector<spdlog::sink_ptr> sinks)
    {
        auto logger = spdlog::get(quasar_log);
        if (not logger)
        {
            if (sinks.size() > 0)
            {
                spdlog::init_thread_pool(8192, 1);
                logger = std::make_shared<spdlog::async_logger>(quasar_log,
                    sinks.begin(),
                    sinks.end(),
                    spdlog::thread_pool(),
                    spdlog::async_overflow_policy::overrun_oldest);

                spdlog::register_logger(logger);
            }
            else
            {
                logger = spdlog::stdout_color_mt(quasar_log);
            }
        }

        return logger;
    }
}  // namespace Log
