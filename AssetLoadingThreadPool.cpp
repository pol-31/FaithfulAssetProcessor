#include "AssetLoadingThreadPool.h"

#include <exception>
#include <iostream>

AssetLoadingThreadPool::AssetLoadingThreadPool(int thread_number) {
  // subtracted by 1 because it's Main thread (see explanation in header)
  int actual_thread_number = std::max(1, thread_number) - 1;
  if (actual_thread_number == 0) {
    throw std::invalid_argument("AssetLoadingThreadPool has 0 worker threads");
  }
  threads_ = std::vector<std::thread>(actual_thread_number);
  threads_task_ = {};
}

void AssetLoadingThreadPool::Run() {
  threads_left_ = 0;
  stopped_ = false;
  for (std::size_t i = 0; i < threads_.size(); ++i) {
    threads_[i] = std::thread([this, i](){
      while (!stopped_) {
        {
          std::unique_lock lock(mu_all_ready_);
          thread_tasks_ready_.wait(lock, [this]() {
            return static_cast<bool>(threads_task_);
          });
        }
        WorkAndWaitAll(i);
      }
    });
  }
}

void AssetLoadingThreadPool::Stop() {
  stopped_ = true;
  // its like poison pill but allows thread to check was thread pool joined
  // (see AssetLoadingThreadPool::Run() if (joined_) { break; })
  threads_left_ = GetThreadNumber() - 1; // this thread don't work
  threads_task_ = [](int){};
  thread_tasks_ready_.notify_all();
  for (auto& thread : threads_) {
    thread.join();
  }
}

void AssetLoadingThreadPool::Execute(TaskType task) {
  threads_left_ = GetThreadNumber();
  threads_task_ = std::move(task);
  thread_tasks_ready_.notify_all();

  // because threads_size() == all_threads - 1, where 1 is Calling thread
  WorkAndWaitAll(threads_.size());
}

void AssetLoadingThreadPool::WorkAndWaitAll(int thread_id) {
  threads_task_(thread_id);
  {
    std::unique_lock lock(mu_all_completed_);
    if (--threads_left_ != 0) {
      thread_tasks_completed_.wait(lock, [this]() {
        return threads_left_ == 0;
      });
      return;
    }
  }
  threads_task_ = {}; // don't need mutex here - all other threads 100% wait
  thread_tasks_completed_.notify_all();
}
