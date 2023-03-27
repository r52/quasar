#pragma once

#include <chrono>
#include <condition_variable>
#include <shared_mutex>
#include <thread>

#include <spdlog/spdlog.h>

class Timer
{
public:
    Timer(const std::string& timer_name) : name{timer_name} {}

    ~Timer() { stop(); }

    void setInterval(auto&& fn, int intv)
    {
        interval = intv;
        // SPDLOG_DEBUG("New timer thread with {}us internal", interval);
        thread = std::jthread{[&, fn](std::stop_token token) {
            const auto origInterval    = std::chrono::nanoseconds(std::chrono::microseconds(interval));
            auto       nextSleep       = origInterval;
            auto       frameTimeBefore = std::chrono::steady_clock::now();
            auto       frameTimeAfter  = std::chrono::steady_clock::now();

            while (true)
            {
                {
                    std::unique_lock lk(mtx);
                    if (cv.wait_for(lk, token, nextSleep, [&] {
                            return token.stop_requested();
                        }))
                    {
                        return;
                    }
                }

                frameTimeBefore = std::chrono::steady_clock::now();

                fn();

                frameTimeAfter = std::chrono::steady_clock::now();
                nextSleep      = origInterval - (frameTimeAfter - frameTimeBefore);
            }
        }};
    }

    int  getInterval() { return interval; }

    void stop()
    {
        if (thread.joinable())
        {
            thread.request_stop();
            thread.join();
        }
    }

private:
    const std::string           name{};
    std::shared_mutex           mtx;
    std::condition_variable_any cv;
    int                         interval;
    std::jthread                thread;
};
