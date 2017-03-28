#include <Windows.h>

#include <cstdio>
#include <functional>
#include <map>
#include <plugin_api.h>

static log_func_type logFunc = nullptr;

const char* pluginName = "Simple Performance Query";
const char* pluginCode = "win_simple_perf";

using GetDataFnType = std::function<int(char*, int, int*)>;
using DataCallTable = std::map<unsigned int, GetDataFnType>;

static DataCallTable calltable;

enum PerfDataSources
{
    PERF_SRC_CPU = 0,
    PERF_SRC_RAM
};

QuasarPluginDataSource sources[2] =
{
    { "cpu", 1000, 0 },
    { "ram", 1000, 0 }
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

int getCPUData(char* buf, int bufsz, int* treatDataType)
{
    double cpu = GetCPULoad() * 100.0;

    snprintf(buf, bufsz, "%d", (int) cpu);

    return true;
}

int getRAMData(char* buf, int bufsz, int* treatDataType)
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

void quasar_plugin_free(void* ptr, int size)
{
    if (size > 1)
    {
        delete[] ptr;
    }
    else
    {
        delete ptr;
    }
}

int quasar_plugin_init(int cmd, QuasarPluginInfo* info)
{
    switch (cmd)
    {
        case QUASAR_INIT_START:
        {
            if (nullptr == info)
            {
                return false;
            }

            if (!info->logFunc)
            {
                return false;
            }

            logFunc = info->logFunc;
            strcpy_s(info->pluginName, pluginName);
            strcpy_s(info->pluginCode, pluginCode);
            strcpy_s(info->author, "me");
            strcpy_s(info->version, "v1");
            strcpy_s(info->description, "Sample plugin that queries basic performance numbers");

            // setup data sources

            info->numDataSources = _countof(sources);
            info->dataSources = sources;

            return true;
        }
        case QUASAR_INIT_RESP:
        {
            // Process uid returns.
            // This should also deallocate anything info->dataSources if
            // it was allocated dynamically

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
        case QUASAR_INIT_SHUTDOWN:
        {
            // does nothing in this example, but should deallocate and shutdown any allocated resources
            return true;
        }
        default:
        {
            if (logFunc)
            {
                logFunc(QUASAR_LOG_WARNING, "Unidentified command");
            }
            break;
        }
    }

    return false;
}

int quasar_plugin_get_data(unsigned int srcUid, char* buf, int bufsz, int* treatDataType)
{
    if (calltable.count(srcUid) == 0)
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "Plugin '%s' - Unknown source %u", pluginCode, srcUid);
        logFunc(QUASAR_LOG_WARNING, msg);
    }
    else
    {
        return calltable[srcUid](buf, bufsz, treatDataType);
    }

    return false;
}
