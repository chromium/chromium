// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// `_Static_assert` is used by threadpool-common.h for GCC build. Because
// `_Static_assert` is not defined in GCC C++ backend, define `_Static_assert`
// by `static_assert` as a work-around.
#if defined(__GNUC__) && \
    ((__GNUC__ > 4) || (__GNUC__ == 4) && (__GNUC_MINOR__ >= 6))
#define _Static_assert(predicate, message) static_assert(predicate, message)
#endif

// Configuration header.
#include "threadpool-common.h"

// Public library header.
#include <pthreadpool.h>

#if defined(_MSC_VER) && defined(_M_ARM64)
#include <arm64intr.h>

// The following ARM64 intrinsics are used by threadpool-atomics.h and should be
// defined in arm64intr.h. If arm64intr.h is not included correctly, e.g. due to
// LLVM issue: https://github.com/llvm/llvm-project/issues/62942, declare them
// here as a work-around.
//
// TODO(crbug.com/1228275): Remove this work-around once the LLVM issue is
// fixed.
#ifndef ARM64_FPCR
extern "C" {
unsigned __int32 __ldar32(unsigned __int32 volatile* _Target);
unsigned __int64 __ldar64(unsigned __int64 volatile* _Target);
void __stlr32(unsigned __int32 volatile* _Target, unsigned __int32 _Value);
void __stlr64(unsigned __int64 volatile* _Target, unsigned __int64 _Value);
}
#endif
#endif  // defined(_MSC_VER) && defined(_M_ARM64)

// Internal library headers.
#include "threadpool-atomics.h"
#include "threadpool-utils.h"

// Redeclare the following three pthreadpool internal functions with extern "C"
// linkage that are used by this C++ implementation.
#define pthreadpool_allocate _pthreadpool_allocate
#define pthreadpool_deallocate _pthreadpool_deallocate
#define pthreadpool_parallelize _pthreadpool_parallelize

#include "threadpool-object.h"

#undef pthreadpool_allocate
#undef pthreadpool_deallocate
#undef pthreadpool_parallelize

extern "C" {
PTHREADPOOL_INTERNAL struct pthreadpool* pthreadpool_allocate(
    size_t threads_count);

PTHREADPOOL_INTERNAL void pthreadpool_deallocate(
    struct pthreadpool* threadpool);

PTHREADPOOL_INTERNAL void pthreadpool_parallelize(
    struct pthreadpool* threadpool,
    thread_function_t thread_function,
    const void* params,
    size_t params_size,
    void* task,
    void* context,
    size_t linear_range,
    uint32_t flags);
}

// Chromium headers.
#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/task/post_job.h"
#include "base/task/task_traits.h"

// According to the tests on multiple systems, there will be a performance
// regression of XNNPACK model inference when the number of work items is
// greater than `kMaxNumWorkItems`.
//
// TODO(crbug.com/1228275): Ensure `kMaxNumWorkItems` value setting makes sense.
constexpr size_t kMaxNumWorkItems = 4;

// Processes `threadpool` which represents the parallel computation task
// dispatched by `pthreadpool_parallelize()` in `num_work_items` items with
// `Run()` from `base::PostJob`. `GetMaxConcurrency()` is a callback used in
// `base::PostJob()` to control the maximum number of threads calling `Run()`
// concurrently.
class PthreadPoolJob {
 public:
  PthreadPoolJob(struct pthreadpool* threadpool, size_t num_work_items)
      : threadpool_(threadpool),
        num_work_items_(num_work_items),
        num_remaining_work_items_(num_work_items) {}
  ~PthreadPoolJob() = default;

  void Run(base::JobDelegate* delegate) {
    CHECK(delegate);
    while (!delegate->ShouldYield()) {
      size_t index = GetNextIndex();
      if (index >= num_work_items_) {
        return;
      }

      DoWork(index);

      if (CompleteWork()) {
        return;
      }
    }
  }

  size_t GetMaxConcurrency(size_t worker_count) const {
    // `num_remaining_work_items_` includes the work items that other workers
    // are currently working on, so we can safely ignore the `worker_count` and
    // just return the current number of remaining work items.
    return num_remaining_work_items_.load(std::memory_order_relaxed);
  }

 private:
  void DoWork(size_t index) {
    struct thread_info* thread = &threadpool_->threads[index];
    const uint32_t flags =
        pthreadpool_load_relaxed_uint32_t(&threadpool_->flags);
    const thread_function_t thread_function =
        reinterpret_cast<thread_function_t>(
            pthreadpool_load_relaxed_void_p(&threadpool_->thread_function));

    // To avoid performance degradation of handling denormalized floating-point
    // numbers, the caller may set PTHREADPOOL_FLAG_DISABLE_DENORMALS flag that
    // instructs the thread pool workers to disable support for denormalized
    // numbers before running the computation and restore the state after the
    // computation is complete.
    //
    // See pthreadpool document for more details:
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/pthreadpool/src/include/pthreadpool.h;l=51
    struct fpu_state saved_fpu_state = {0};
    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      saved_fpu_state = get_fpu_state();
      disable_fpu_denormals();
    }

    thread_function(threadpool_, thread);

    if (flags & PTHREADPOOL_FLAG_DISABLE_DENORMALS) {
      set_fpu_state(saved_fpu_state);
    }
  }

  // Returns the index of the next work item to process.
  size_t GetNextIndex() {
    // `index_` may exceeed `num_work_items_`, but only by the number of
    // workers at worst, thus it can't exceed 2 * `num_work_items_` and
    // overflow shouldn't happen.
    return index_.fetch_add(1, std::memory_order_relaxed);
  }

  // Returns true if the last work item was completed.
  bool CompleteWork() {
    size_t num_remaining_work_items =
        num_remaining_work_items_.fetch_sub(1, std::memory_order_relaxed);
    CHECK_GE(num_remaining_work_items, 1);
    return num_remaining_work_items == 1;
  }

  struct pthreadpool* threadpool_;
  const size_t num_work_items_;
  std::atomic<size_t> index_{0};
  std::atomic<size_t> num_remaining_work_items_;
};

struct pthreadpool* pthreadpool_create(size_t threads_count) {
  // When using Jobs API, the `threads_count` only means the number of work
  // items will be scheduled by ThreadPool for each `pthreadpool_parallelize()`
  // call. Jobs API won't guarantee scheduling the same number of physical
  // threads according to that.

  if (threads_count == 0) {
    threads_count = std::max(1, base::SysInfo::NumberOfProcessors());
  }

  // Cap to `kMaxNumWorkItems` that avoids too much scheduling overhead.
  threads_count = std::min(threads_count, kMaxNumWorkItems);
  CHECK_GT(threads_count, 0u);

  struct pthreadpool* threadpool = pthreadpool_allocate(threads_count);
  if (threadpool == nullptr) {
    return nullptr;
  }

  threadpool->threads_count = fxdiv_init_size_t(threads_count);
  for (size_t tid = 0; tid < threads_count; tid++) {
    threadpool->threads[tid].thread_number = tid;
    threadpool->threads[tid].threadpool = threadpool;
  }

  return threadpool;
}

// The `threadpool` struct is accessed by this method without holding a lock.
// The caller should ensure accessing a `threadpool` struct from the same
// sequence that inherently provides thread-safety.
PTHREADPOOL_INTERNAL void pthreadpool_parallelize(
    struct pthreadpool* threadpool,
    thread_function_t thread_function,
    const void* params,
    size_t params_size,
    void* task,
    void* context,
    size_t linear_range,
    uint32_t flags) {
  CHECK(threadpool);
  CHECK(thread_function);
  CHECK(task);
  CHECK_GT(linear_range, 1);

  // Setup global arguments.
  pthreadpool_store_relaxed_void_p(&threadpool->thread_function,
                                   (void*)thread_function);
  pthreadpool_store_relaxed_void_p(&threadpool->task, task);
  pthreadpool_store_relaxed_void_p(&threadpool->argument, context);
  pthreadpool_store_relaxed_uint32_t(&threadpool->flags, flags);

  const struct fxdiv_divisor_size_t threads_count = threadpool->threads_count;

  if (params_size != 0) {
    memcpy(&threadpool->params, params, params_size);
  }

  // Spread the work between threads.
  const struct fxdiv_result_size_t range_params =
      fxdiv_divide_size_t(linear_range, threads_count);
  size_t range_start = 0;
  for (size_t tid = 0; tid < threads_count.value; tid++) {
    struct thread_info* thread = &threadpool->threads[tid];
    // Besides taking `quotient` items, each of the first `remainder` threads
    // also takes one extra item if the `remainder` is not 0.
    const size_t range_length =
        range_params.quotient + (tid < range_params.remainder ? 1 : 0);
    const size_t range_end = range_start + range_length;
    pthreadpool_store_relaxed_size_t(&thread->range_start, range_start);
    pthreadpool_store_relaxed_size_t(&thread->range_end, range_end);
    pthreadpool_store_relaxed_size_t(&thread->range_length, range_length);

    // The next subrange starts where the previous ended.
    range_start = range_end;
  }

  // Request ThreadPool workers to process `threads_count.value` number of work
  // items in parallel.
  auto job = std::make_unique<PthreadPoolJob>(threadpool, threads_count.value);
  auto handle = base::PostJob(
      FROM_HERE, {},
      base::BindRepeating(&PthreadPoolJob::Run, base::Unretained(job.get())),
      base::BindRepeating(&PthreadPoolJob::GetMaxConcurrency,
                          base::Unretained(job.get())));
  handle.Join();
}

void pthreadpool_destroy(struct pthreadpool* threadpool) {
  if (threadpool != nullptr) {
    // Release resources.
    pthreadpool_deallocate(threadpool);
  }
}
