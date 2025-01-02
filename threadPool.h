#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads) 
        : shutdownRequested(false)
        , busyThreads(0) {
        try {
            for (size_t i = 0; i < numThreads; ++i) {
                threads.emplace_back(&ThreadPool::workerFunction, this);
            }
        } catch (...) {
            shutdown();
            throw;
        }
    }

    ~ThreadPool() {
        shutdown();
    }

    template<typename F, typename... Args>
    auto addTask(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using ReturnType = typename std::invoke_result<F, Args...>::type;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<ReturnType> future = task->get_future();
        
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (shutdownRequested) {
                throw std::runtime_error("Cannot add tasks to a stopped ThreadPool");
            }
            
            tasks.push([task]() { (*task)(); });
        }
        
        conditionVariable.notify_one();
        return future;
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            shutdownRequested = true;
        }
        conditionVariable.notify_all();
        
        for (std::thread& worker : threads) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    size_t getNumBusyThreads() const {
        return busyThreads.load();
    }

    size_t getNumThreads() const {
        return threads.size();
    }

    long getQueueSize() const {
        return tasks.size();
    }

private:
    void workerFunction() {
        while (true) {
            std::function<void()> task;
            
            {
                std::unique_lock<std::mutex> lock(mutex);
                conditionVariable.wait(lock, [this] {
                    return shutdownRequested || !tasks.empty();
                });
                
                if (shutdownRequested && tasks.empty()) {
                    return;
                }
                
                task = std::move(tasks.front());
                tasks.pop();
            }
            
            busyThreads++;
            task();
            busyThreads--;
        }
    }

    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    
    mutable std::mutex mutex;
    std::condition_variable conditionVariable;
    bool shutdownRequested;
    std::atomic<size_t> busyThreads;
};