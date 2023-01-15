#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
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
        SPDLOG_DEBUG("New timer thread with {}ms internal", intv);
        thread = std::jthread{[=](std::stop_token token) {
            std::unique_lock<std::mutex> lk(mtx);
            while (true)
            {
                if (cv.wait_for(lk, std::chrono::milliseconds(intv), [&] {
                        return token.stop_requested();
                    }))
                {
                    return;
                }

                fn();
            }
        }};

        thread.detach();
    }

    int  getInterval() { return interval; }

    void stop()
    {
        {
            std::unique_lock<std::mutex> lk(mtx);
            thread.request_stop();
        }

        cv.notify_one();

        if (thread.joinable())
        {
            thread.join();
        }
    }

private:
    const std::string       name{};
    int                     interval;
    std::jthread            thread;
    std::condition_variable cv;
    std::mutex              mtx;
};
