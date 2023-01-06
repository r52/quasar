#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <spdlog/spdlog.h>

class Timer
{
public:
    ~Timer() { stop(); }

    void setInterval(auto&& fn, int interval)
    {
        SPDLOG_DEBUG("New timer thread with {}ms internal", interval);
        thread = std::jthread{[=](std::stop_token token) {
            std::unique_lock<std::mutex> lk(mtx);
            while (true)
            {
                if (cv.wait_for(lk, std::chrono::milliseconds(interval), [&] {
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
    std::jthread            thread;
    std::condition_variable cv;
    std::mutex              mtx;
};
