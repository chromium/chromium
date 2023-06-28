#ifndef THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_THREADPOOL_H_
#define THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_THREADPOOL_H_

#include <functional>
#include <thread>  // NOLINT
#include <vector>

class ThreadPool {
 public:
  ThreadPool(int num_threads) {}
  virtual ~ThreadPool() {
    for (auto &task : tasks_) {
      task.join();
    }
  }

  void StartWorkers() {}
  void Schedule(std::function<void()> closure) { tasks_.emplace_back(closure); }

 private:
  std::vector<std::thread> tasks_;
};
#endif  // THIRD_PARTY_SENTENCEPIECE_SRC_DEPS_THREADPOOL_H_
