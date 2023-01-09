#include <Windows.h>

#include <cstdio>
#include <functional>
#include <sstream>
#include <unordered_map>

#include <extension_api.h>
#include <extension_support.hpp>

#include <fmt/core.h>

#define EXT_FULLNAME "Simple Performance Query"
#define EXT_NAME     "win_simple_perf"

#define qlog(l, ...)                                                      \
  {                                                                       \
    auto msg = fmt::format("{}: {}", EXT_NAME, fmt::format(__VA_ARGS__)); \
    quasar_log(l, msg.c_str());                                           \
  }

#define info(...) qlog(QUASAR_LOG_INFO, __VA_ARGS__)
#define warn(...) qlog(QUASAR_LOG_WARNING, __VA_ARGS__)

quasar_data_source_t sources[] = {
    {       "sysinfo",                  5000,    0, 0},
    {"sysinfo_polled", QUASAR_POLLING_CLIENT, 1000, 0}
};

// From https://stackoverflow.com/questions/23143693/retrieving-cpu-load-percent-total-in-windows-with-c
static float CalculateCPULoad(unsigned long long idleTicks, unsigned long long totalTicks)
{
    static unsigned long long _previousTotalTicks     = 0;
    static unsigned long long _previousIdleTicks      = 0;

    unsigned long long        totalTicksSinceLastTime = totalTicks - _previousTotalTicks;
    unsigned long long        idleTicksSinceLastTime  = idleTicks - _previousIdleTicks;

    float                     ret                     = 1.0f - ((totalTicksSinceLastTime > 0) ? ((float) idleTicksSinceLastTime) / totalTicksSinceLastTime : 0);

    _previousTotalTicks                               = totalTicks;
    _previousIdleTicks                                = idleTicks;
    return ret;
}

static unsigned long long FileTimeToInt64(const FILETIME& ft)
{
    return (((unsigned long long) (ft.dwHighDateTime)) << 32) | ((unsigned long long) ft.dwLowDateTime);
}

// Returns 1.0f for "CPU fully pinned", 0.0f for "CPU idle", or somewhere in between
// You'll need to call this at regular intervals, since it measures the load between
// the previous call and the current one.  Returns -1.0 on error.
float GetCPULoad()
{
    FILETIME idleTime, kernelTime, userTime;
    return GetSystemTimes(&idleTime, &kernelTime, &userTime) ?
               CalculateCPULoad(FileTimeToInt64(idleTime), FileTimeToInt64(kernelTime) + FileTimeToInt64(userTime)) :
               -1.0f;
}

bool simple_perf_init(quasar_ext_handle handle)
{
    // Nothing to do
    return true;
}

bool simple_perf_shutdown(quasar_ext_handle handle)
{
    // nothing to do. no dynamic allocations
    return true;
}

bool simple_perf_get_data(size_t srcUid, quasar_data_handle hData, char* args)
{
    if (srcUid != sources[0].uid && srcUid != sources[1].uid)
    {
        warn("Unknown source {}", srcUid);
        return false;
    }

    // CPU data
    double cpu = GetCPULoad() * 100.0;

    // https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
    // RAM data
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    DWORDLONG physMemUsed  = memInfo.ullTotalPhys - memInfo.ullAvailPhys;

    auto      res          = fmt::format("{{\"cpu\":{},\"ram\":{{\"total\":{},\"used\":{}}}}}", (int) cpu, totalPhysMem, physMemUsed);

    quasar_set_data_json_hpp(hData, res);

    return true;
}

quasar_ext_info_fields_t fields = {EXT_NAME, EXT_FULLNAME, "3.0", "r52", "Provides basic PC performance metrics", "https://github.com/r52/quasar"};

quasar_ext_info_t        info   = {QUASAR_API_VERSION,
             &fields,

             std::size(sources),
             sources,

             simple_perf_init,
             simple_perf_shutdown,
             simple_perf_get_data,
             nullptr,
             nullptr};

quasar_ext_info_t*       quasar_ext_load(void)
{
    return &info;
}

void quasar_ext_destroy(quasar_ext_info_t* info)
{
    // does nothing; info is on stack
}
