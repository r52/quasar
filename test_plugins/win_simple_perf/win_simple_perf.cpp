#include <Windows.h>

#include <cstdio>
#include <plugin_api.h>

static log_func_type logFunc;

const char* pluginName = "Simple Performance Query";
const char* pluginCode = "win_simple_perf";

const char *cpuSource = "cpu";
const char *ramSource = "ram";

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

int quasar_plugin_init(QuasarPluginInfo* info)
{
    if (!info->logFunc)
    {
        return false;
    }

    logFunc = info->logFunc;
    strcpy_s(info->pluginName, pluginName);
    strcpy_s(info->pluginCode, pluginCode);
    strcpy_s(info->author, "me");
    strcpy_s(info->description, "Sample plugin that queries basic performance numbers");

    // setup data sources

    info->numDataSources = 2;

    QuasarPluginDataSource *sources = new QuasarPluginDataSource[2];

    strcpy_s(sources[0].dataSrc, cpuSource);
    sources[0].refreshMsec = 5000;

    strcpy_s(sources[1].dataSrc, ramSource);
    sources[1].refreshMsec = 10000;

    info->dataSources = sources;

    return true;
}

int quasar_plugin_get_data(const char* dataSrc, char* buf, int bufsz, int* convert_data_to_json)
{
    if (0 == strcmp(dataSrc, cpuSource))
    {
        // collect cpu
        double cpu = GetCPULoad() * 100.0;

        snprintf(buf, bufsz, "%d", (int)cpu);
    }
    else if (0 == strcmp(dataSrc, ramSource))
    {
        // https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process
        // collect ram
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        DWORDLONG totalPhysMem = memInfo.ullTotalPhys;
        DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;

        snprintf(buf, bufsz, "{ \"total\": %lld, \"used\": %lld }", totalPhysMem, physMemUsed);

        if (nullptr != convert_data_to_json)
        {
            *convert_data_to_json = true;
        }
    }
    else
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "Plugin '%s' - Unknown source %s", pluginCode, dataSrc);
        logFunc(QUASAR_LOG_WARNING, msg);

        return false;
    }

    return true;
}
