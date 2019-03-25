#include <Windows.h>

#include <cstdio>
#include <functional>
#include <sstream>
#include <unordered_map>

#include <extension_api.h>
#include <extension_support.h>

#define EXT_FULLNAME "Simple Performance Query"
#define EXT_NAME "win_simple_perf"

#define qlog(l, f, ...)                                             \
    {                                                               \
        char msg[256];                                              \
        snprintf(msg, sizeof(msg), EXT_NAME ": " f, ##__VA_ARGS__); \
        quasar_log(l, msg);                                         \
    }

#define info(f, ...) qlog(QUASAR_LOG_INFO, f, ##__VA_ARGS__)
#define warn(f, ...) qlog(QUASAR_LOG_WARNING, f, ##__VA_ARGS__)

using GetDataFnType = std::function<bool(quasar_data_handle hData)>;
using DataCallTable = std::unordered_map<size_t, GetDataFnType>;

static DataCallTable calltable;

enum PerfDataSources
{
    PERF_SRC_CPU = 0,
    PERF_SRC_RAM
};

quasar_data_source_t sources[2] = {{"cpu", 0, 1000, 0}, {"ram", 0, 1000, 0}};

// From https://stackoverflow.com/questions/23143693/retrieving-cpu-load-percent-total-in-windows-with-c
static float CalculateCPULoad(unsigned long long idleTicks, unsigned long long totalTicks)
{
    static unsigned long long _previousTotalTicks = 0;
    static unsigned long long _previousIdleTicks  = 0;

    unsigned long long totalTicksSinceLastTime = totalTicks - _previousTotalTicks;
    unsigned long long idleTicksSinceLastTime  = idleTicks - _previousIdleTicks;

    float ret = 1.0f - ((totalTicksSinceLastTime > 0) ? ((float) idleTicksSinceLastTime) / totalTicksSinceLastTime : 0);

    _previousTotalTicks = totalTicks;
    _previousIdleTicks  = idleTicks;
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

bool getCPUData(quasar_data_handle hData)
{
    double cpu = GetCPULoad() * 100.0;

    quasar_set_data_int(hData, (int) cpu);

    return true;
}

bool getRAMData(quasar_data_handle hData)
{
    // https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
    // collect ram
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    DWORDLONG physMemUsed  = memInfo.ullTotalPhys - memInfo.ullAvailPhys;

    std::stringstream ss;
    ss << "{ \"total\": " << totalPhysMem << ", \"used\": " << physMemUsed << " }";

    quasar_set_data_json(hData, ss.str().c_str());

    return true;
}

bool simple_perf_init(quasar_ext_handle handle)
{
    // Process uid entries.
    if (sources[PERF_SRC_CPU].uid != 0)
    {
        calltable[sources[PERF_SRC_CPU].uid] = getCPUData;
    }

    if (sources[PERF_SRC_RAM].uid != 0)
    {
        calltable[sources[PERF_SRC_RAM].uid] = getRAMData;
    }

    return true;
}

bool simple_perf_shutdown(quasar_ext_handle handle)
{
    // nothing to do. no dynamic allocations
    return true;
}

bool simple_perf_get_data(size_t srcUid, quasar_data_handle hData)
{
    if (calltable.count(srcUid) == 0)
    {
        warn("Unknown source %Iu", srcUid);
        return false;
    }

    return calltable[srcUid](hData);
}

quasar_ext_info_fields_t fields =
    {EXT_NAME, EXT_FULLNAME, "2.0", "r52", "Sample plugin that queries basic performance numbers", "https://github.com/r52/quasar"};

quasar_ext_info_t info = {QUASAR_API_VERSION,
                          &fields,

                          std::size(sources),
                          sources,

                          simple_perf_init,
                          simple_perf_shutdown,
                          simple_perf_get_data,
                          nullptr,
                          nullptr};

quasar_ext_info_t* quasar_ext_load(void)
{
    return &info;
}

void quasar_ext_destroy(quasar_ext_info_t* info)
{
    // does nothing; info is on stack
}
