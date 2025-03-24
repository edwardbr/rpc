#pragma once

#include <functional>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <assert.h>
#include <stdint.h>


extern "C"
{
    int start_thread(uint64_t enclave_id, uint64_t temporary_id);
}

extern uint64_t enclave_id_;

class thread_proxy : std::enable_shared_from_this<thread_proxy>
{
    enum class state
    {
        not_started,
        running,
        stopped
    };
    std::function<void()> function_;
    std::atomic<state> state = state::not_started;
    std::mutex mtx_;
    uint64_t thread_id_ = 0;
    
    static std::mutex awaiting_for_thread_mtx_;
    static std::unordered_map<uint64_t, std::shared_ptr<thread_proxy>> awaiting_for_thread_map_;    
    inline static std::atomic<uint64_t> temporary_thread_id_generator_ = 0;
public:
    void start(std::function<void()> func) 
    { 
        uint64_t temp_id = 0;
        function_ = func; 
        std::unique_lock<std::mutex> lock(awaiting_for_thread_mtx_);
        temp_id = ++temporary_thread_id_generator_;
        awaiting_for_thread_map_.insert({temp_id, shared_from_this()});
        start_thread(temp_id, enclave_id_);
    }

    void join() { std::unique_lock<std::mutex> lock(mtx_); }

    bool joinable() { return state == state::running; }

    static void on_started(uint64_t temporary_id, uint64_t thread_id)
    {
        std::shared_ptr<thread_proxy> thread;    
        {
            std::unique_lock<std::mutex> lock(awaiting_for_thread_mtx_);
            auto it = awaiting_for_thread_map_.find(temporary_id);
            if(it == awaiting_for_thread_map_.end())
            {
                assert(false);
                return;
            }
            thread = it->second;
            awaiting_for_thread_map_.erase(it);
        }
        if(thread->state != state::not_started)
            return;
        std::unique_lock<std::mutex> lock(thread->mtx_);
        if(thread->state != state::not_started)
            return;
        thread->state = state::running;
        thread->thread_id_ = thread_id;
        thread->function_();
        thread->state = state::stopped;
    }
};
