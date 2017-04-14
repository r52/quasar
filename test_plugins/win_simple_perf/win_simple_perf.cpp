#include <Windows.h>

#include <cstdio>
#include <functional>
#include <map>
#include <plugin_support.h>
#include <plugin_api.h>

const char* pluginName = "Simple Performance Query";
const char* pluginCode = "win_simple_perf";

using GetDataFnType = std::function<bool(char*, size_t, int*)>;
using DataCallTable = std::map<size_t, GetDataFnType>;

static DataCallTable calltable;

enum PerfDataSources
{
    PERF_SRC_CPU = 0,
    PERF_SRC_RAM
};

quasar_data_source_t sources[2] =
{
    { "cpu", 5000, 0 },
    { "ram", 5000, 0 }
};

// From https://stackoverflow.com/questions/23143693/retrieving-cpu-load-percent-total-in-windows-with-c
static float CalculateCPULoad(unsigned long long idleTicks, unsigned long long totalTicks)
{
    static unsigned long long _previousTotalTicks = 0;
    static unsigned long long _previousIdleTicks = 0;

    unsigned long long totalTicksSinceLastTime = totalTicks - _previousTotalTicks;
    unsigned long long idleTicksSinceLastTime = idleTicks - _previousIdleTicks;

    float ret = 1.0f - ((totalTicksSinceLastTime > 0) ? ((float)idleTicksSinceLastTime) / totalTicksSinceLastTime : 0);

    _previousTotalTicks = totalTicks;
    _previousIdleTicks = idleTicks;
    return ret;
}

static unsigned long long FileTimeToInt64(const FILETIME & ft) { return (((unsigned long long)(ft.dwHighDateTime)) << 32) | ((unsigned long long)ft.dwLowDateTime); }

// Returns 1.0f for "CPU fully pinned", 0.0f for "CPU idle", or somewhere in between
// You'll need to call this at regular intervals, since it measures the load between
// the previous call and the current one.  Returns -1.0 on error.
float GetCPULoad()
{
    FILETIME idleTime, kernelTime, userTime;
    return GetSystemTimes(&idleTime, &kernelTime, &userTime) ? CalculateCPULoad(FileTimeToInt64(idleTime), FileTimeToInt64(kernelTime) + FileTimeToInt64(userTime)) : -1.0f;
}

bool getCPUData(char* buf, size_t bufsz, int* treatDataType)
{
    double cpu = GetCPULoad() * 100.0;

    snprintf(buf, bufsz, "%d", (int) cpu);

    return true;
}

bool getRAMData(char* buf, size_t bufsz, int* treatDataType)
{
    // https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
    // collect ram
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
    DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;

    snprintf(buf, bufsz, "{ \"total\": %lld, \"used\": %lld }", totalPhysMem, physMemUsed);

    if (nullptr != treatDataType)
    {
        *treatDataType = QUASAR_TREAT_AS_JSON;
    }

    return true;
}

bool simple_perf_init(quasar_plugin_info_t* info)
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

bool simple_perf_shutdown(quasar_plugin_info_t* info)
{
    // nothing to do. no dynamic allocations
    return true;
}

bool simple_perf_get_data(size_t srcUid, char* buf, size_t bufsz, int* treatDataType)
{
    if (calltable.count(srcUid) == 0)
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "Plugin '%s' - Unknown source %Iu", pluginCode, srcUid);
        quasar_log(QUASAR_LOG_WARNING, msg);
    }
    else
    {
        return calltable[srcUid](buf, bufsz, treatDataType);
    }

    return false;
}

quasar_plugin_info_t info =
{
    "Simple Performance Query",
    "win_simple_perf",
    "v1",
    "me",
    "Sample plugin that queries basic performance numbers",

    _countof(sources),
    sources,

    simple_perf_init,
    simple_perf_shutdown,
    simple_perf_get_data,
    nullptr,
    nullptr
};

quasar_plugin_info_t* quasar_plugin_load(void)
{
    return &info;
}
