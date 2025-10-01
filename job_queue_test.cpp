// SPDX-FileCopyrightText: 2022 Ivan Matveev <imatveev13@yandex.ru>
// SPDX-License-Identifier: MIT

#include "job_queue.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

#include <pthread.h> // pthread_self()

#define LOGPP(X) std::cout << "-----" << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << ": " \
    << " thread: " << std::this_thread::get_id() << ":" << pthread_self() << " : "\
    << X << "\n";

int func(int val)
{
    return ++val;
}

int main()
{
    LOGPP("");
    job_queue_t job_queue_;
    std::atomic<bool> run_ = false;
    run_ = true;
    std::thread thread_;
    thread_ = std::thread([&run_, &job_queue_]() {
        while (run_) {
            job_queue_.wait_job();
            while (job_queue_.exec_one()) {
            }
        }
    });

    job_queue_.invoke_async([]() { LOGPP(""); });
    LOGPP("");
    job_queue_.invoke_blocking([](){
        LOGPP("sleep");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        LOGPP("sleep end");
    });

    {
        int res = job_queue_.invoke_blocking<int>(func, 2);
        LOGPP(res);
    }
    {
        auto res = job_queue_.invoke_blocking<std::string>([](){ return std::string("Hello"); });
        LOGPP(res);
    }
    {
        int res = job_queue_.invoke_blocking<int>([](){ LOGPP("Hello"); return 1; });
        LOGPP(res);
    }
    {
        job_queue_.invoke_blocking([](){ LOGPP("Hello"); });
    }
    {
        auto res = job_queue_.invoke_async([](){ return std::string("Hello"); });
        LOGPP(res.get());
    }

    LOGPP("");
    run_ = false;
    job_queue_.wait_job_cancel();
    thread_.join();
    
    LOGPP("--OK--");
}
