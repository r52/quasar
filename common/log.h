#pragma once

#include "exports.h"

#include <spdlog/spdlog.h>

namespace Log
{
    constexpr auto quasar_log = "quasar_logger";

    common_API std::shared_ptr<spdlog::logger> setup_logger(std::vector<spdlog::sink_ptr> sinks);
}  // namespace Log
