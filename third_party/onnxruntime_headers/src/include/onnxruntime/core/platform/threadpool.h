/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

/* Modifications Copyright (c) Microsoft. */

#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "core/common/common.h"
#include "core/platform/env.h"

#include <functional>
#include <memory>

// ORT thread pool overview
// ------------------------
//
// The ORT thread pool implementation is split into two layers.  This
// file provides the high-level component.  See the accompanying
// comments in EigenNonBlockingThreadPool.h for the low-level
// component.
//
// threadpool.h defines the user-facing functions for use in
// operators.  The main abstraction are parallel loops
// (ThreadPool::TryParallelFor*), although we also support scheduling
// of asynchronous tasks (ThreadPool::Schedule), and the construction
// of multi-loop parallel sections (ThreadPool::ParallelSection).
//
// This high level API is accessed via static methods on the
// ThreadPool class.  These methods map the operations onto one of
// three low-level implementations: (#1) direct execution of the
// operations if there is no thread pool configured, (#2) execution of
// the operations using the modified Eigen threadpool, (#3) execution
// of the operations using OpenMP.  Option #1 enables execution in
// simple settings without needing threads.  Option #2 is the
// preferred approach for use in settings with parallelism.
//
// The high-level part of the thread pool is responsible for:
//
// - Exposing the desired degree of parallelism to user code, and to
//   libraries such as MLAS.  This lets the libraries tailor the
//   extent to which they parallelize work.
//
// - Handling trivial cases (such as directly running parallel loops
//   with only a single iteration, or with no iterations at all).
//
// - Deciding how to divide work efficiently between the threads
//   available.
//
//   The ThreadPool::TryParallelFor methods do this based on cost
//   estimates supplied by the caller, and are designed to support
//   loops with small amounts of work per iteration.  The loop body is
//   supplied as a function taking a [start,end) range of iterations
//   to execute (avoiding the need for per-iteration std::function
//   calls, or a reliance upon inlining to avoid those calls).
//
//   ThreadPool::TrySimpleParallelFor uses a simpler single-iteration
//   API based on the assumption that the caller has divided work to
//   an appropriate granularity.
//
// - When used with the Eigen-based thread pool, the implementation of
//   all of the loops maps down onto
//   ThreadPool::ParallelForFixedBlockSizeScheduling.  This method
//   takes the degree of parallelism (d_of_p) and work distribution
//   block size (from the cost-based heuristics), and creates a set of
//   tasks in the underlying thread pool (via
//   ThreadPool::RunInParallel).
//
//   These tasks then run a loop which picks off batches of iterations
//   from the user's code.  The distribution of these batches is
//   handled dynmamically via LoopCounter::ClaimIterations.  This
//   dynamic balancing behavior helps make performance robust to any
//   variability in the execution time across iterations, and to
//   situations such as multiple loops running concurrently on the
//   same thread pool.
//
// - When running a series of loops inside a parallel section, the
//   LoopCounter also helps obtain affinity between these loops (i.e.,
//   iteration X of one loop will tend to run on the same thread that
//   ran iteration X of prior loops).  This locality helps improve hit
//   rates in per-core caches across the series of short loops used in
//   operators like GRU.
//
// There are some known areas for exploration here:
//
// - The cost-based heuristics were developed prior to recent changes
//   to the thread pool.  The heuristics seem to work well, but we
//   should revisit the tuning periodically.
//
// - Can we unify the APIs for the different kinds of parallel loop?
//
//   In particular, we may be able to replace the current use of
//   TryBatchParallelFor with appropriate costs for each call site,
//   and then use TryParallelFor.  This would allow for more dynamic
//   re-balancing of work between threads than the current
//   ThreadPool::PartitionWork function provides.
//
// - Given the extensive modifications to original Eigen code, should
//   we separate that out as a new class and remove the dependence on
//   other Eigen components.

// This file use PIMPL to avoid having eigen headers here
namespace Eigen {
class Allocator;
class ThreadPoolInterface;
}  // namespace Eigen

namespace onnxruntime {

struct TensorOpCost {
  double bytes_loaded;
  double bytes_stored;
  double compute_cycles;
};

namespace concurrency {

template <typename Environment>
class ThreadPoolTempl;

class ExtendedThreadPoolInterface;
class LoopCounter;
class ThreadPoolParallelSection;

class ThreadPool {
 public:
#ifdef _WIN32
  using NAME_CHAR_TYPE = wchar_t;
#else
  using NAME_CHAR_TYPE = char;
#endif
  // Constructs a pool for running with with "degree_of_parallelism" threads with
  // specified "name". env->StartThread() is used to create individual threads
  // with the given ThreadOptions. If "low_latency_hint" is true the thread pool
  // implementation may use it as a hint that lower latency is preferred at the
  // cost of higher CPU usage, e.g. by letting one or more idle threads spin
  // wait. Conversely, if the threadpool is used to schedule high-latency
  // operations like I/O the hint should be set to false.
  //
  // REQUIRES: degree_of_parallelism > 0
  ThreadPool(Env* env,
             const ThreadOptions& thread_options,
             const NAME_CHAR_TYPE* name,
             int degree_of_parallelism,
             bool low_latency_hint,
             bool force_hybrid = false);

  // Waits until all scheduled work has finished and then destroy the
  // set of threads.
  ~ThreadPool();

  // Start and end a multi-loop parallel section.  Parallel loops can
  // be executed directly (without using this API), but entering a
  // parallel section allows the runtime system to amortize loop
  // entry/exit costs over multiple loops, and allows it to promote
  // affinity between corresponding iterations of different loops.
  //
  // Multi-loop sections would typically be used in cases where a
  // series of loops executes without much code in between them, and
  // where it is impractical to refactor code into a single loop.  For
  // instance:
  //
  // {
  //   onnxruntime::concurrency::ThreadPoool::ParallelSection ps(tp);
  //   for (int x = 0; x < seq_len; x++) {
  //     TrySimpleParallelFor(tp, 16, [&]() { ... });
  //   }
  // }
  //
  // The parallel section is entered via the constructor of
  // ThreadPool::ParallelSection, and exited via the destructor.
  // Currently, thread-local state is used to track whether or not the
  // current thread is inside a parallel section.  In contrast to
  // handling parallel section objects explicitly in user code, this
  // approach allows code such as MLAS to operate with/without the use
  // of parallel sections.
  //
  // Parallel sections are only implemented with the Eigen threadpool.
  // They have no effect when using OpenMP.
  //
  // Parallel sections may not be nested, and may not be used inside
  // parallel loops.

  class ParallelSection {
   public:
    explicit ParallelSection(ThreadPool* tp);
    ~ParallelSection();

   private:
    friend class ThreadPool;

    // Owning reference for the underlying ThreadPoolParallelSection
    // which implements the thread management.  We use an explicit
    // deleter here so that the definition of
    // ThreadPoolParallelSection does not need to be available at this
    // point to avoid a dependence on the Eigen headers.
    ThreadPoolParallelSection* ps_{nullptr};
    ThreadPool* tp_;
    ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(ParallelSection);
  };

  // The below API allows to disable spinning
  // This is used to support real-time scenarios where
  // spinning between relatively infrequent requests
  // contributes to high CPU usage while not processing anything.
  void EnableSpinning();

  void DisableSpinning();

  // Schedules fn() for execution in the pool of threads.  The function may run
  // synchronously if it cannot be enqueued.  This will occur if the thread pool's
  // degree-of-parallelism is 1, but it may also occur for implementation-dependent
  // reasons such as if queues used for buffering work are full.
  static void Schedule(ThreadPool* tp,
                       std::function<void()> fn) {
    if (tp) {
      tp->Schedule(fn);
    } else {
      fn();
    }
  }

  // ParallelFor shards the "total" units of work assuming each unit of work
  // having roughly "cost_per_unit" cost, in cycles. Each unit of work is
  // indexed 0, 1, ..., total - 1. Each shard contains 1 or more units of work
  // and the total cost of each shard is roughly the same.
  //
  // "cost_per_unit" is an estimate of the number of CPU cycles (or nanoseconds
  // if not CPU-bound) to complete a unit of work. Overestimating creates too
  // many shards and CPU time will be dominated by per-shard overhead, such as
  // Context creation. Underestimating may not fully make use of the specified
  // parallelism, and may also cause inefficiencies due to load balancing
  // issues and stragglers.

  static void TryParallelFor(ThreadPool* tp, std::ptrdiff_t total, double cost_per_unit,
                             const std::function<void(std::ptrdiff_t first, std::ptrdiff_t last)>& fn) {
    TryParallelFor(tp, total, TensorOpCost{0, 0, static_cast<double>(cost_per_unit)}, fn);
  }

  static void TryParallelFor(ThreadPool* tp, std::ptrdiff_t total, const TensorOpCost& cost_per_unit,
                             const std::function<void(std::ptrdiff_t first, std::ptrdiff_t last)>& fn);

  // Directly schedule the 'total' tasks to the underlying threadpool, without
  // cutting them by halves

  inline static void TrySimpleParallelFor(ThreadPool* tp, std::ptrdiff_t total,
                                          const std::function<void(std::ptrdiff_t)>& fn) {
    if (tp != nullptr) {
      tp->SimpleParallelFor(total, fn);
    } else {
      for (std::ptrdiff_t i = 0; i < total; ++i) {
        // In many cases, fn can be inlined here.
        fn(i);
      }
    }
  }

  /**
   * Tries to call the given function in parallel, with calls split into (num_batches) batches.
   *\param num_batches If it is zero, it will be replaced to the value of DegreeOfParallelism().
   *\param fn A std::function or STL style functor with signature of "void f(std::ptrdiff_t);"
   * Pitfall: Caller should cap `num_batches` to a reasonable value based on the cost of `fn` and the value of `total`.
   *For example, if fn is as simple as: int sum=0; fn = [&](int i){sum +=i;} and `total` is 100, then num_batches should
   *be just 1.
   *
   * ```
   **/
  template <typename F>
  inline static void TryBatchParallelFor(ThreadPool* tp, std::ptrdiff_t total, F&& fn, std::ptrdiff_t num_batches) {
    if (tp == nullptr) {
      for (std::ptrdiff_t i = 0; i < total; ++i) {
        // In many cases, fn can be inlined here.
        fn(i);
      }
      return;
    }
    if (total <= 0)
      return;

    if (total == 1) {
      fn(0);
      return;
    }

    if (num_batches <= 0) {
      num_batches = std::min<std::ptrdiff_t>(total, DegreeOfParallelism(tp));
    }

    if (num_batches <= 1) {
      for (int i = 0; i < total; i++) {
        fn(i);
      }
      return;
    }

    tp->SimpleParallelFor(num_batches, [&](std::ptrdiff_t batch_index) {
      auto work = PartitionWork(batch_index, num_batches, total);
      for (std::ptrdiff_t i = work.start; i < work.end; i++) {
        fn(i);
      }
    });
  }

  struct WorkInfo {
    std::ptrdiff_t start{0};
    std::ptrdiff_t end{0};
  };

  /** Calculate the start and end offsets for a batch.
      @remarks Based on MlasPartitionWork
  */
  constexpr static WorkInfo PartitionWork(std::ptrdiff_t batch_idx, std::ptrdiff_t num_batches, std::ptrdiff_t total_work) {
    const std::ptrdiff_t work_per_batch = total_work / num_batches;
    const std::ptrdiff_t work_per_batch_extra = total_work % num_batches;

    WorkInfo info;
    if (batch_idx < work_per_batch_extra) {
      info.start = (work_per_batch + 1) * batch_idx;
      info.end = info.start + work_per_batch + 1;
    } else {
      info.start = work_per_batch * batch_idx + work_per_batch_extra;
      info.end = info.start + work_per_batch;
    }

    return info;
  }

  //......................................................................
  //
  // The following static methods take into account whether OpenMP is
  // enabled/disabled, and if the thread pool pointer is nullptr
  // during sequential execution.

  // Provide a hint to the caller for whether or not to parallelize
  // work.  This lets a caller switch to a sequential version of an
  // algorithm rather than using calls via the ParallelFor functions.

  static bool ShouldParallelize(const ThreadPool* tp);

  // Return the degree of parallelism that code should assume when using the thread pool.
  // It decouples the degree of parallelism for use with the thread pool from
  // the implementation choice of whether this matches the number of threads created in
  // the pool.
  //
  // Currently, a loop with degree-of-parallelism N is supported by a pool of N-1 threads
  // working in combination with the thread initiating the loop.
  static int DegreeOfParallelism(const ThreadPool* tp);

  ORT_DISALLOW_COPY_AND_ASSIGNMENT(ThreadPool);

  // StartProfiling and StopProfiling are not to be consumed as public-facing API
  static void StartProfiling(concurrency::ThreadPool* tp);
  static std::string StopProfiling(concurrency::ThreadPool* tp);

 private:
  friend class LoopCounter;

  // Returns the number of threads created in the pool.  This may be different from the
  // value returned by DegreeOfParallelism to code using the pool.
  int NumThreads() const;

  // Returns current thread id between 0 and NumThreads() - 1, if called from a
  // thread in the pool. Returns -1 otherwise.
  int CurrentThreadId() const;

  // Run fn with up to n degree-of-parallelism enlisting the thread pool for
  // help.  The degree-of-parallelism includes the caller, and so if n==1
  // then the function will run directly in the caller.  The fork-join
  // synchronization is handled in the thread pool, and so any state captured
  // by fn() is safe from concurrent access once RunWithHelp returns.
  void RunInParallel(std::function<void(unsigned idx)> fn, unsigned n, std::ptrdiff_t block_size);

  // Divides the work represented by the range [0, total) into k shards.
  // Calls fn(i*block_size, (i+1)*block_size) from the ith shard (0 <= i < k).
  // Each shard may be executed on a different thread in parallel, depending on
  // the number of threads available in the pool.
  // When (i+1)*block_size > total, fn(i*block_size, total) is called instead.
  // Requires 0 < block_size <= total.
  void ParallelForFixedBlockSizeScheduling(std::ptrdiff_t total, std::ptrdiff_t block_size,
                                           const std::function<void(std::ptrdiff_t, std::ptrdiff_t)>& fn);

  // Return whether or not the calling thread should run a loop of
  // num_iterations divided in chunks of block_size in parallel.  If not,
  // the caller should run the loop sequentially.
  bool ShouldParallelizeLoop(const std::ptrdiff_t num_iterations,
                             const std::ptrdiff_t block_size = 1) const;

  // Internal (non-static) parallel loop methods.  Unlike the public static methods,
  // these will not handle the cases of OpenMP builds. or builds without a threadpool.
  void ParallelFor(std::ptrdiff_t total, double cost_per_unit,
                   const std::function<void(std::ptrdiff_t first, std::ptrdiff_t last)>& fn);

  void ParallelFor(std::ptrdiff_t total, const TensorOpCost& cost_per_unit,
                   const std::function<void(std::ptrdiff_t first, std::ptrdiff_t)>& fn);

  void SimpleParallelFor(std::ptrdiff_t total, const std::function<void(std::ptrdiff_t)>& fn);

  void Schedule(std::function<void()> fn);

  void StartProfiling();

  std::string StopProfiling();

  ThreadOptions thread_options_;

  // If a thread pool is created with degree_of_parallelism != 1 then an underlying
  // EigenThreadPool is used to create OS threads and handle work distribution to them.
  // If degree_of_parallelism == 1 then underlying_threadpool_ is left as nullptr
  // and parallel work is run directly by the caller.
  ExtendedThreadPoolInterface* underlying_threadpool_ = nullptr;

  // If used, underlying_threadpool_ is instantiated and owned by the ThreadPool.
  std::unique_ptr<ThreadPoolTempl<Env> > extended_eigen_threadpool_;

  // Force the thread pool to run in hybrid mode on a normal cpu.
  bool force_hybrid_ = false;
};

}  // namespace concurrency
}  // namespace onnxruntime
