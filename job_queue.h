// SPDX-FileCopyrightText: 2022 Ivan Matveev <imatveev13@yandex.ru>
// SPDX-License-Identifier: MIT

#ifndef JOB_QUEUE_H
#define JOB_QUEUE_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>

struct job_queue_t
{
    void invoke_async_void_func(std::function<void()> &&job)
    {
        std::unique_lock lock(mutex_);
        queue_.emplace_back(job);
        empty_ = false;
        condvar_.notify_one();
    }

    template <typename F, typename... Args>
    auto invoke_async(F&& f, Args&&... args)
    {
        using return_type = std::invoke_result_t<F, Args...>;
        using packaged_task_type = std::packaged_task<return_type()>;
        auto task_ptr = std::make_shared<packaged_task_type>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        invoke_async_void_func(std::bind(&packaged_task_type::operator(), task_ptr));
        return task_ptr->get_future();
    }

    template <typename Ret, typename F, typename... Args>
    Ret invoke_blocking(F&& f, Args&&... args)
    {
        std::packaged_task<Ret()> task(std::bind(
            std::forward<F>(f),
                std::forward<Args>(args)...));
        std::future<Ret> future = task.get_future();
        invoke_async_void_func([&]() { task(); });
        return future.get();
    }

    template <typename F, typename... Args>
    void invoke_blocking(F&& f, Args&&... args)
    {
        std::promise<void> promise;
        std::future<void> future = promise.get_future();
        std::packaged_task<void()> task(std::bind(
            std::forward<F>(f),
                std::forward<Args>(args)...));
        invoke_async_void_func([&]() { task(); promise.set_value(); });
        future.wait();
    }

    bool exec_one()
    {
        if (empty())
            return false;

        std::function<void()> func;

        {
            std::unique_lock lock(mutex_);
            std::swap(func, queue_.front());
            queue_.pop_front();
            if (queue_.empty())
                empty_ = true;
        }
        func();
        return true;
    }

    size_t size()
    {
        std::unique_lock lock(mutex_);
        return queue_.size();
    }

    bool empty() { return empty_; }

    void wait_job()
    {
        std::unique_lock lock(mutex_);
        wait_thread_id_ = std::this_thread::get_id();
        condvar_.wait(lock, [this]() { return queue_.size() > 0; });
    }

    void wait_job_cancel()
    {
        invoke_async_void_func([]() {});
    }

    std::deque<std::function<void()>> queue_;
    std::mutex mutex_;
    std::atomic_bool empty_ = ATOMIC_VAR_INIT(true);
    std::condition_variable condvar_;
    std::atomic<std::thread::id> wait_thread_id_;
};

#endif // JOB_QUEUE_H
