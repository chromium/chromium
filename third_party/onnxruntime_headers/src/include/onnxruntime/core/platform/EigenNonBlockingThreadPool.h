// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2016 Dmitry Vyukov <dvyukov@google.com>
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

/* Modifications Copyright (c) Microsoft. */

#include <type_traits>

#pragma once
#include "onnxruntime_config.h"
// build/external/eigen/unsupported/Eigen/CXX11/src/Tensor/TensorEvaluator.h:162:71:
// error: ignoring attributes on template argument "Eigen::PacketType<const float, Eigen::DefaultDevice>::type {aka
// __vector(4) float}" [-Werror=ignored-attributes]
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-result"
// cmake/external/eigen/unsupported/Eigen/CXX11/../../../Eigen/src/Core/arch/NEON/PacketMath.h:1633:9:
// error: ‘void* memcpy(void*, const void*, size_t)’ copying an object of non-trivial type ‘Eigen::internal::Packet4c’
// {aka ‘struct Eigen::internal::eigen_packet_wrapper<int, 2>’} from an array of ‘const int8_t’
// {aka ‘const signed char’} [-Werror=class-memaccess]
#ifdef HAS_CLASS_MEMACCESS
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
// eigen-src/unsupported/Eigen/CXX11/src/ThreadPool/EventCount.h:231:56: error: implicit conversion loses integer
//   precision: 'uint64_t' (aka 'unsigned long long') to 'size_t' (aka 'unsigned long') [-Werror,-Wshorten-64-to-32]
// next = wnext == kStackMask ? nullptr : &waiters_[wnext];
//                                         ~~~~~~~~ ^~~~~
#ifdef HAS_SHORTEN_64_TO_32
#pragma GCC diagnostic ignored "-Wshorten-64-to-32"
#endif
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127)
#pragma warning(disable : 4805)
#endif
#include <memory>
#include "unsupported/Eigen/CXX11/ThreadPool"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
#include "core/common/denormal.h"
#include "core/common/inlined_containers_fwd.h"
#include "core/common/spin_pause.h"
#include "core/platform/ort_spin_lock.h"
#include "core/platform/Barrier.h"

// ORT thread pool overview
// ------------------------
//
// The ORT thread pool implementation is split into two layers.  This
// file provides the low-level component.  See the accompanying
// comments in threadpool.h for the high-level component.
//
// The code here is derived from the Eigen non-blocking thread pool,
// although many parts have been updated over time.  The main
// abstractions used here are:
//
// - The thread pool maintains a set of OS threads running
//   ThreadPoolTempl::WorkerLoop.
//
//   Each thread has its own RunQueue object, holding a queue of tasks
//   that have been pushed to the thread for execution.  The main work
//   loop is to pop a task from the head of the queue, and to execute
//   it to completion.  If the worker's run queue is empty then it
//   will spin waiting for work, then attempt to steal tasks from
//   other threads' queues, and then block in the OS if it cannot find
//   work.
//
//   This spin-then-block behavior is configured via a flag provided
//   when creating the thread pool, and by the constant spin_count.
//
// - Although all tasks are simple void()->void functions,
//   conceptually there are three different kinds:
//
//   - One-shot tasks submitted externally via the Schedule() method.
//     These tasks are used to support asynchronous work.  These are
//     used in the parallel executor, but otherwise are not widely
//     used outside of test harnesses (see threadpool_test.cc for some
//     examples).
//
//   - Tasks for running a parallel loop.
//
//     The tasks themselves are defined in threadpool.cc, and are
//     submitted to the run queues via RunInParallel->SummonWorkers.
//     Each task will loop internally, picking off iterations from the
//     user's code via atoic-fetch-and-add, until the loop is
//     complete.
//
//     This two-layer approach lets us separate out the
//     super-lightweight per-iteration-batch work from the more
//     costly per-loop work of managing Task objects.
//
//   - Tasks for running a parallel section.  This is an extension of
//     the approach taken for parallel loops.  However, the Tasks are
//     defined in this file, and can pick up iterations from a series
//     of different parallel loops.  The tasks are defined in
//     RunInParallelSection->SummonWorkers.
//
//     The additional layer of parallel sections is a further way to
//     amortize costs: the work done creating the tasks can be
//     performed once, and then exploited over a series of loops.
//
// There are a few aspects of the modified Eigen thread pool to
// highlight:
//
// - The run queues follow the usual approach of having push/pop
//   operations on the front/back, and optimizing the PopFront case
//   for single-threaded use by the thread owning the run queue.
//   Two points to note here are:
//
//   * We should experiment with simplifying these queues.  In ORT, we
//     use the CAS-based scheduling layer in threadpool.cc for the
//     fine-grained allocation of individual loop iterations to worker
//     threads.  This means we do not have the form of recursive
//     sub-division of work that motivates the original design.
//
//   * We support an additional Revoke operation to replace an item in
//     the middle of a queue with a tombstone.  This operation is used
//     at the end of parallel loops and parallel sections to remove
//     any tasks that were created but not yet executed.  Once
//     revoked, a thread can rely on the fact that the task will no
//     longer execute.  Revocation helps manage captured state in
//     parallel loops: the alternatives would be (i) waiting for all
//     tasks that captured state to reach the head of their queues and
//     execute, or (ii) use heap-allocated state in tasks, and use a
//     technique such as reference counting to de-allocate it.
//
//     To support revocation, each thread has a unique "Tag" to
//     identify the items that it adds to the work queues.  A thread
//     can revoke an item only if it has the thread's own tag.
//
// - When entering a parallel loop (or parallel section), a thread
//   maintains a set of "preferred" worker hints, and initially
//   submits tasks to these workers.
//   When a task executes, it updates the submitting thread's
//   preferred workers to reflect the worker that the task ran on.
//   Hence, if a task is submitted to thread T1's queue, and then
//   stolen by T2 for execution, then T2 will become preferred.
//
//   This "stickiness" aims to retain locality between successive
//   loops submitted by the same thread, to maintain the same set of
//   active threads over time (when the entire pool is not needed),
//   and to allow concurrent requests to submit works to their own
//   respective sets of preferred workers.

namespace onnxruntime {
namespace concurrency {

#ifdef _WIN32
using CHAR_TYPE = wchar_t;
#else
using CHAR_TYPE = char;
#endif

class ThreadPoolParallelSection;
class ThreadPoolLoop;

enum class StealAttemptKind {
  TRY_ONE,
  TRY_ALL,
};

enum class PushResult {
  REJECTED,
  ACCEPTED_IDLE,
  ACCEPTED_BUSY
};

// Align to avoid false sharing with prior fields.  If required,
// alignment or padding must be added subsequently to avoid false
// sharing with later fields.  Note that:
//
// - The __x86_64__ value is twice the line size (64 bytes).  This
//   accounts for 2-line prefetch behavior on some cores.
//
// - Ideally, ORT_ALIGN_TO_AVOID_FALSE_SHARING is used.  However, the
//   definition of ThreadPoolParallelSection uses naive padding
//   because C++11 does not support alignment constraints on
//   allocation or expose stdlib.h aligned_alloc.  C++17 introduces
//   support for aligned allocation which we could use here.

#if defined(__x86_64__)
#define ORT_FALSE_SHARING_BYTES 128
#else
#define ORT_FALSE_SHARING_BYTES 64
#endif

#define ORT_ALIGN_TO_AVOID_FALSE_SHARING alignas(ORT_FALSE_SHARING_BYTES)

struct PaddingToAvoidFalseSharing {
  char padding[ORT_FALSE_SHARING_BYTES];
};

/* Usage:
1. In executor, call Start() before profiling and Stop() to get profiled numbers;
2. Inside thread pool, call LogStart() before interested section and LogEnd... after to log elapsed time;
3. To extend, just add more events in enum Event before "All", and update GetEventName(...) accordingly;
4. Note LogStart must pair with either LogEnd or LogEndAndStart, otherwise ORT_ENFORCE will fail;
5. ThreadPoolProfiler is thread-safe.
*/
#ifdef ORT_MINIMAL_BUILD
class ThreadPoolProfiler {
 public:
  enum ThreadPoolEvent {
    DISTRIBUTION = 0,
    DISTRIBUTION_ENQUEUE,
    RUN,
    WAIT,
    WAIT_REVOKE,
    MAX_EVENT
  };
  ThreadPoolProfiler(int, const CHAR_TYPE*) {};
  ~ThreadPoolProfiler() = default;
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(ThreadPoolProfiler);
  void Start() {};
  std::string Stop() { return "not available for minimal build"; }
  void LogStart() {};
  void LogEnd(ThreadPoolEvent){};
  void LogEndAndStart(ThreadPoolEvent){};
  void LogStartAndCoreAndBlock(std::ptrdiff_t){};
  void LogCoreAndBlock(std::ptrdiff_t){};
  void LogThreadId(int) {};
  void LogRun(int) {};
  std::string DumpChildThreadStat() { return {}; }
};
#else
class ThreadPoolProfiler {
 public:
  enum ThreadPoolEvent {
    DISTRIBUTION = 0,
    DISTRIBUTION_ENQUEUE,
    RUN,
    WAIT,
    WAIT_REVOKE,
    MAX_EVENT
  };
  ThreadPoolProfiler(int num_threads, const CHAR_TYPE* threal_pool_name);
  ~ThreadPoolProfiler();
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(ThreadPoolProfiler);
  using Clock = std::chrono::high_resolution_clock;
  void Start();                  // called by executor to start profiling
  std::string Stop();            // called by executor to stop profiling and return collected numbers
  void LogStart();               // called in main thread to record the starting time point
  void LogEnd(ThreadPoolEvent);  // called in main thread to calculate and save the time elapsed from last start point
  void LogEndAndStart(ThreadPoolEvent);
  void LogStartAndCoreAndBlock(std::ptrdiff_t block_size);
  void LogCoreAndBlock(std::ptrdiff_t block_size);  // called in main thread to log core and block size for task breakdown
  void LogThreadId(int thread_idx);                 // called in child thread to log its id
  void LogRun(int thread_idx);                      // called in child thread to log num of run
  std::string DumpChildThreadStat();                // return all child statitics collected so far

 private:
  static const char* GetEventName(ThreadPoolEvent);
  struct MainThreadStat {
    uint64_t events_[MAX_EVENT] = {};
    int32_t core_ = -1;
    std::vector<std::ptrdiff_t> blocks_;  // block size determined by cost model
    std::vector<onnxruntime::TimePoint> points_;
    void LogCore();
    void LogBlockSize(std::ptrdiff_t block_size);
    void LogStart();
    void LogEnd(ThreadPoolEvent);
    void LogEndAndStart(ThreadPoolEvent);
    std::string Reset();
  };
  bool enabled_ = false;
  MainThreadStat& GetMainThreadStat();  // return thread local stat
  int num_threads_;
#ifdef _MSC_VER
#pragma warning(push)
  // C4324: structure was padded due to alignment specifier
#pragma warning(disable : 4324)
#endif  // _MSC_VER
  struct ORT_ALIGN_TO_AVOID_FALSE_SHARING ChildThreadStat {
    std::thread::id thread_id_;
    uint64_t num_run_ = 0;
    onnxruntime::TimePoint last_logged_point_ = Clock::now();
    int32_t core_ = -1;  // core that the child thread is running on
  };
#ifdef _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER
  std::vector<ChildThreadStat> child_thread_stats_;
  std::string thread_pool_name_;
};
#endif

// Extended Eigen thread pool interface, avoiding the need to modify
// the ThreadPoolInterface.h header from the external Eigen
// repository.

class ExtendedThreadPoolInterface : public Eigen::ThreadPoolInterface {
 public:
  // Start/end a parallel section, within which calls to
  // RunInParallelSection may be made.  Parallel sections are
  // non-nesting.
  virtual void StartParallelSection(ThreadPoolParallelSection& ps) = 0;
  virtual void EndParallelSection(ThreadPoolParallelSection& ps) = 0;

  // Run fn with up to n degree-of-parallelism enlisting the thread
  // pool for help.  The degree-of-parallelism includes the caller,
  // and so if n==1 then the function will run directly in the caller.
  //
  // The fork-join synchronization is handled in the thread pool, and
  // so any state captured by fn() is safe from concurrent access once
  // RunInParallelSection returns.
  //
  // The parameter idx provides a loop-local thread ID in the range
  // [0,k) where k<=n.
  virtual void RunInParallelSection(ThreadPoolParallelSection& ps,
                                    std::function<void(unsigned idx)> fn,
                                    unsigned n, std::ptrdiff_t block_size) = 0;

  // Special case alternative to RunInParallelSection for use without
  // an existing parallel section.  Ideally we would use a single
  // implementation and a stack-allocated ThreadPoolParallelSection.
  //
  // However, on the BM_ThreadPoolParallelFor micro-benchmark I saw
  // ~20% overhead on the resulting single-loop parallel sections.
  // There are some additional costs (~5%) for additional invocations
  // through lambda functions on loop entry.  Most significantly, on
  // loop exit, we incurred ~15% cost by no longer being able to
  // overlap clean-up of unused Task objects in EndParallelSection
  // with waiting for loop iterations to complete.
  //
  // [ Note that this 20% overhead is more than paid for when we have
  // two loops execute in series in a parallel section. ]
  virtual void RunInParallel(std::function<void(unsigned idx)> fn,
                             unsigned n, std::ptrdiff_t block_size) = 0;
  virtual void StartProfiling() = 0;
  virtual std::string StopProfiling() = 0;
};

class ThreadPoolParallelSection {
 public:
  // State accessed only by the main thread
  // --------------------------------------

  // Tasks successfully submitted to the work queues.  This sets the
  // maximum degree of parallelism that the section will support.
  InlinedVector<std::pair<int, unsigned>> tasks;

  // Number of tasks revoked (i.e., removed from the queues prior to
  // execution).  We count this at various points, and omit waiting
  // for them at the end of a loop.
  unsigned tasks_revoked{0};

  // Current degree of parallelism, including work in the main thread
  // and in the dispatcher.
  unsigned current_dop{0};

  // State shared between the main thread and worker threads
  // -------------------------------------------------------

  // Flag to signal termination of the parallel section
  std::atomic<bool> active{false};

  // Count of the number of tasks that completed normally.  Other
  // tasks may be running currently, or may be present in work queues,
  // or may have been removed from the queues by
  // RunQueue::RevokeWithTag.
  PaddingToAvoidFalseSharing padding_1;
  std::atomic<unsigned> tasks_finished{0};
  PaddingToAvoidFalseSharing padding_2;

  // If non-null, the current loop that tasks should be executing.  We
  // need to be careful on access to the contents of current_loop
  // because it can be stack allocated on the thread entering the
  // loop:
  //
  // - Readers increment workers_in_loop and then read current_loop
  //
  // - Writers wishing to deallocate *current_loop must first clear
  //   current_loop and then wait for workers_in_loop==0
  std::atomic<ThreadPoolLoop*> current_loop{nullptr};
  std::atomic<unsigned> workers_in_loop{0};

  // Members to track asynchronous dispatching
  int dispatch_q_idx = -1;      // index of thread that dispatch work to all other threads
  unsigned dispatch_w_idx = 0;  // index of enqueued work
  std::atomic<bool> dispatch_started{false};
  std::atomic<bool> dispatch_done{false};
  std::atomic<bool> work_done{false};
};

class ThreadPoolLoop {
 public:
  ThreadPoolLoop(std::function<void(unsigned)> f, unsigned t) : fn(std::move(f)), threads_needed(t) {
  }

  const std::function<void(unsigned)> fn;
  const unsigned threads_needed;

 private:
  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(ThreadPoolLoop);
};

template <typename Work, typename Tag, unsigned kSize>
class RunQueue {
 public:
  RunQueue() : front_(0), back_(0) {
    // require power-of-two for fast masking
    assert((kSize & (kSize - 1)) == 0);
    assert(kSize > 2);            // why would you do this?
    assert(kSize <= (64 << 10));  // leave enough space for counter
    for (unsigned i = 0; i < kSize; i++) array_[i].state.store(ElemState::kEmpty, std::memory_order_relaxed);
  }

  ~RunQueue() {
    assert(Size() == 0);
  }

  // PopFront removes and returns the first element in the queue.
  // If the queue was empty returns default-constructed Work.
  Work PopFront() {
    unsigned front;
    Elem* e;
    ElemState s;

    // Drain revoked items from the front of the queue.  CAS to busy to synchronize with
    // any attempt to take the same item from the back of the queue.
    do {
      front = front_.load(std::memory_order_relaxed);
      e = &array_[(front - 1) & kMask];
      s = e->state.load(std::memory_order_relaxed);
      if (s == ElemState::kRevoked &&
          e->state.compare_exchange_strong(s, ElemState::kBusy, std::memory_order_acquire)) {
        e->state.store(ElemState::kEmpty, std::memory_order_release);
        front = ((front - 1) & kMask2) | (front & ~kMask2);
        front_.store(front, std::memory_order_relaxed);
      }
    } while (s == ElemState::kRevoked);

    // Attempt to take next item.  State kEmpty shows the queue is empty, kBusy shows
    // the work is in progress on the item at the front of the queue.
    if (s != ElemState::kReady ||
        !e->state.compare_exchange_strong(s, ElemState::kBusy, std::memory_order_acquire))
      return Work();
    Work w = std::move(e->w);
    e->tag = Tag();
    e->state.store(ElemState::kEmpty, std::memory_order_release);
    front = ((front - 1) & kMask2) | (front & ~kMask2);
    front_.store(front, std::memory_order_relaxed);
    return w;
  }

  // PushBack adds w at the end of the queue.
  // If queue is full returns w, otherwise returns default-constructed Work.
  Work PushBack(Work w) {
#ifdef USE_LOCK_FREE_QUEUE
    std::lock_guard<OrtSpinLock> mtx(spin_lock_);
#else
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    unsigned back = back_.load(std::memory_order_relaxed);
    Elem& e = array_[(back - 1) & kMask];
    ElemState s = e.state.load(std::memory_order_relaxed);
    if (s != ElemState::kEmpty ||
        !e.state.compare_exchange_strong(s, ElemState::kBusy, std::memory_order_acquire))
      return w;
    back = ((back - 1) & kMask2) | (back & ~kMask2);
    back_.store(back, std::memory_order_relaxed);
    e.w = std::move(w);
    e.tag = Tag();
    e.state.store(ElemState::kReady, std::memory_order_release);
    return Work();
  }

  // PushBackWithTag adds w at the end of the queue.  The tag value can be used on a
  // subsequent call to RevokeWithTag to remove the item from the queue in combination
  // with w_idx.  Typically the tag will be a per-thread ID to distinguish work
  // submitted from different threads.
  PushResult PushBackWithTag(Work w, Tag tag, unsigned& w_idx) {
#ifdef USE_LOCK_FREE_QUEUE
    std::lock_guard<OrtSpinLock> mtx(spin_lock_);
#else
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    unsigned back = back_.load(std::memory_order_relaxed);
    w_idx = (back - 1) & kMask;
    Elem& e = array_[w_idx];
    ElemState s = e.state.load(std::memory_order_relaxed);
    if (s != ElemState::kEmpty ||
        !e.state.compare_exchange_strong(s, ElemState::kBusy, std::memory_order_acquire))
      return PushResult::REJECTED; /* Not enqueued */
    bool was_ready = (((back ^ (front_.load(std::memory_order_relaxed))) & kMask) == 0);
    back = ((back - 1) & kMask2) | (back & ~kMask2);
    back_.store(back, std::memory_order_relaxed);
    e.w = std::move(w);
    e.tag = tag;
    e.state.store(ElemState::kReady, std::memory_order_release);
    return was_ready ? PushResult::ACCEPTED_IDLE : PushResult::ACCEPTED_BUSY; /* Enqueued */
  }

  // PopBack removes and returns the last elements in the queue.
  Work PopBack() {
    if (Empty())
      return Work();
#ifdef USE_LOCK_FREE_QUEUE
    std::lock_guard<OrtSpinLock> mtx(spin_lock_);
#else
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    unsigned back;
    Elem* e;
    ElemState s;

    // Drain revoked items from the back of the queue.  CAS to busy to synchronize with
    // any attempt to take the same item from the front of the queue.
    do {
      back = back_.load(std::memory_order_relaxed);
      e = &array_[back & kMask];
      s = e->state.load(std::memory_order_relaxed);
      if (s == ElemState::kRevoked &&
          e->state.compare_exchange_strong(s, ElemState::kBusy, std::memory_order_acquire)) {
        e->state.store(ElemState::kEmpty, std::memory_order_release);
        back_.store(back + 1 + (kSize << 1), std::memory_order_relaxed);
      }
    } while (s == ElemState::kRevoked);

    if (s != ElemState::kReady ||
        !e->state.compare_exchange_strong(s, ElemState::kBusy, std::memory_order_acquire))
      return Work();
    Work w = std::move(e->w);
    e->tag = Tag();
    e->state.store(ElemState::kEmpty, std::memory_order_release);
    back_.store(back + 1 + (kSize << 1), std::memory_order_relaxed);
    return w;
  }

  // RevokeItem removes a work item from the queue.  Items are identified positionally,
  // and so a tag is used to detect whether the same position is occupied by a
  // different work item at the time of removal.  RevokeWithTags lets threads offer work
  // for parallel execution, and then revoke the offer prior to the work executing (for
  // instance if the thread itself completes all of the work).  Revoking the work
  // lets the thread deallocate state that might otherwise have been captured by the work item
  // and accessed by it.
  //
  // Return true iff the item is successfully revoked.  If the item is not revoked then
  // the caller must assume that it may still execute, for instance because it
  // has been pop'd from the queue concurrent with the revocation request.

  bool RevokeWithTag(Tag tag, unsigned w_idx) {
    bool revoked = false;
#ifdef USE_LOCK_FREE_QUEUE
    std::lock_guard<OrtSpinLock> mtx(spin_lock_);
#else
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    Elem& e = array_[w_idx];
    ElemState s = e.state.load(std::memory_order_relaxed);

    // We have acquired a lock on the queue, synchronizing with
    // operations aside from the PopFront fast-path.  Synchronize with
    // that by attempting the same kReady->kBusy transition via CAS.

    if (s == ElemState::kReady &&
        e.state.compare_exchange_strong(s, ElemState::kBusy, std::memory_order_acquire)) {
      if (e.tag == tag) {
        unsigned back = back_.load(std::memory_order_relaxed);
        unsigned back_idx = back & kMask;
        if (back_idx != w_idx) {
          // Item is not at the back of the queue, mark it in-place as revoked
          e.tag = Tag();
          e.w = Work();
          e.state.store(ElemState::kRevoked, std::memory_order_release);
          revoked = true;
        } else {
          // Item being removed as still at the back; shift the back pointer over it,
          // and bump the version number.
          e.tag = Tag();
          e.w = Work();
          e.state.store(ElemState::kEmpty, std::memory_order_release);
          back_.store(back + 1 + (kSize << 1), std::memory_order_relaxed);
          revoked = true;
        }
      } else {
        // Tag mismatch, i.e. work queue slot re-used
        e.state.store(ElemState::kReady, std::memory_order_release);
      }
    }
    return revoked;
  }

  // Size returns current queue size.
  // Can be called by any thread at any time.
  unsigned Size() const {
    return SizeOrNotEmpty<true>();
  }

  // Empty tests whether container is empty.
  // Can be called by any thread at any time.
  bool Empty() const {
    return SizeOrNotEmpty<false>() == 0;
  }

 private:
  static const unsigned kMask = kSize - 1;
  static const unsigned kMask2 = (kSize << 1) - 1;

  enum class ElemState : uint8_t {
    kEmpty,
    kBusy,
    kReady,
    kRevoked,
  };

  // Updates to an element are bracketed by a std::memory_order_acquire
  // load from the state, and a std::memory_order_release store.  Accesses
  // to the front/back indices for the work queue use relaxed semantics,
  // with the state of the elements being authoritative.
  //
  // TODO: Revisit whether there is a significant benefit for the current
  // workloads in the complexity here.
  struct Elem {
    std::atomic<ElemState> state;
    Tag tag;
    Work w;
  };

#ifdef USE_LOCK_FREE_QUEUE
  OrtSpinLock spin_lock_;
#else
  std::mutex mutex_;
#endif

  // Low log(kSize) + 1 bits in front_ and back_ contain rolling index of
  // front/back, respectively. The remaining bits contain modification counters
  // that are incremented on Push operations. This allows us to (1) distinguish
  // between empty and full conditions (if we would use log(kSize) bits for
  // position, these conditions would be indistinguishable); (2) obtain
  // consistent snapshot of front_/back_ for Size operation using the
  // modification counters.
  ORT_ALIGN_TO_AVOID_FALSE_SHARING std::atomic<unsigned> front_;
  ORT_ALIGN_TO_AVOID_FALSE_SHARING std::atomic<unsigned> back_;
  ORT_ALIGN_TO_AVOID_FALSE_SHARING Elem array_[kSize];

  // SizeOrNotEmpty returns current queue size; if NeedSizeEstimate is false,
  // only whether the size is 0 is guaranteed to be correct.
  // Can be called by any thread at any time.
  template <bool NeedSizeEstimate>
  unsigned SizeOrNotEmpty() const {
    // Emptiness plays critical role in thread pool blocking. So we go to great
    // effort to not produce false positives (claim non-empty queue as empty).
    unsigned front = front_.load(std::memory_order_acquire);
    for (;;) {
      // Capture a consistent snapshot of front/tail.
      unsigned back = back_.load(std::memory_order_acquire);
      unsigned front1 = front_.load(std::memory_order_relaxed);
      if (front != front1) {
        front = front1;
        std::atomic_thread_fence(std::memory_order_acquire);
        continue;
      }
      if (NeedSizeEstimate) {
        return CalculateSize(front, back);
      }
      // This value will be 0 if the queue is empty, and undefined otherwise.
      unsigned maybe_zero = ((front ^ back) & kMask2);
      // Queue size estimate must agree with maybe zero check on the queue
      // empty/non-empty state.
      eigen_assert((CalculateSize(front, back) == 0) == (maybe_zero == 0));
      return maybe_zero;
    }
  }

  EIGEN_ALWAYS_INLINE
  unsigned CalculateSize(unsigned front, unsigned back) const {
    int size = (front & kMask2) - (back & kMask2);
    // Fix overflow.
    if (size < 0)
      size += 2 * kSize;
    // Order of modification in push/pop is crafted to make the queue look
    // larger than it is during concurrent modifications. E.g. push can
    // increment size before the corresponding pop has decremented it.
    // So the computed size can be up to kSize + 1, fix it.
    if (size > static_cast<int>(kSize))
      size = kSize;
    return static_cast<unsigned>(size);
  }

  RunQueue(const RunQueue&) = delete;
  void operator=(const RunQueue&) = delete;
};

static std::atomic<uint32_t> next_tag{1};

template <typename Environment>
class ThreadPoolTempl : public onnxruntime::concurrency::ExtendedThreadPoolInterface {
 private:
  struct PerThread;

  static unsigned WorkerLoop(int id, Eigen::ThreadPoolInterface* param) {
    // unsafe downcast
    ThreadPoolTempl* this_ptr = (ThreadPoolTempl*)param;
    this_ptr->WorkerLoop(id);
    return 0;
  }

  ThreadPoolProfiler profiler_;

  void SignalAllAndWait() {
    done_ = true;

    // Now if all threads block without work, they will start exiting.
    // But note that threads can continue to work arbitrary long,
    // block, submit new work, unblock and otherwise live full life.
    WakeAllWorkersForExit();
    // Join threads explicitly (by destroying) to avoid destruction order within
    // this class.
    for (size_t i = 0; i < worker_data_.size(); ++i) worker_data_[i].thread.reset();
  }

 public:
  void StartProfiling() override {
    profiler_.Start();
  }

  std::string StopProfiling() override {
    return profiler_.Stop();
  }

  struct Tag {
    constexpr Tag() : v_(0) {
    }

    Tag(uint32_t v) : v_(v) {
    }

    // Allocate a new tag to use to identify work items from a given
    // thread in a parallel section.  Ideally, threads will have
    // unique tags, but re-use is not incorrect if the counter wraps
    // (for intsance, if a long-running workload is calling into ORT
    // from a fresh thread for each request).  We must not re-use the
    // default tag 0 which is used to identify work items added via
    // Schedule as opposed to requests for help in parallel sections.

    static Tag GetNext() {
      Tag t = Tag(next_tag++);
      if (t.v_ == 0) {
        t = Tag(next_tag++);
      }
      return t;
    }

    uint32_t Get() const {
      return v_;
    }

    bool operator==(const Tag& other) const {
      return v_ == other.v_;
    }

    uint32_t v_ = 0;
  };

  typedef std::function<void()> Task;
  typedef RunQueue<Task, Tag, 1024> Queue;

  ThreadPoolTempl(const CHAR_TYPE* name, int num_threads, bool allow_spinning, Environment& env,
                  const ThreadOptions& thread_options)
      : profiler_(num_threads, name),
        env_(env),
        num_threads_(num_threads),
        allow_spinning_(allow_spinning),
        set_denormal_as_zero_(thread_options.set_denormal_as_zero),
        worker_data_(num_threads),
        all_coprimes_(num_threads),
        blocked_(0),
        done_(false) {
    // Calculate coprimes of all numbers [1, num_threads].
    // Coprimes are used for random walks over all threads in Steal
    // and NonEmptyQueueIndex. Iteration is based on the fact that if we take
    // a random starting thread index t and calculate num_threads - 1 subsequent
    // indices as (t + coprime) % num_threads, we will cover all threads without
    // repetitions (effectively getting a presudo-random permutation of thread
    // indices).
    for (auto i = 1u; i <= num_threads_; ++i) {
      all_coprimes_.emplace_back(i);
      ComputeCoprimes(i, &all_coprimes_.back());
    }

    // Eigen::MaxSizeVector has neither essential exception safety features
    // such as swap, nor it is movable. So we have to join threads right here
    // on exception
    ORT_TRY {
      worker_data_.resize(num_threads_);
      for (auto i = 0u; i < num_threads_; i++) {
        worker_data_[i].thread.reset(env_.CreateThread(name, i, WorkerLoop, this, thread_options));
      }
    }
    ORT_CATCH(...) {
      ORT_HANDLE_EXCEPTION([&]() {
        SignalAllAndWait();
        throw;
      });
    }
  }

  ~ThreadPoolTempl() override {
    SignalAllAndWait();
  }

  // Run fn().  Ordinarily, the function will be added to the thread pool and executed
  // by a worker thread.  If the thread pool rejects the work then fn() will instead
  // execute synchronously during Schedule(fn).  Currently the thread pool will only
  // reject work if the queue of pending work is full.

  void Schedule(std::function<void()> fn) override {
    PerThread* pt = GetPerThread();
    int q_idx = Rand(&pt->rand) % num_threads_;
    WorkerData& td = worker_data_[q_idx];
    Queue& q = td.queue;
    fn = q.PushBack(std::move(fn));
    if (!fn) {
      // The queue accepted the work; ensure that the thread will pick it up
      td.EnsureAwake();
    } else {
      // Run the work directly if the queue rejected the work
      fn();
    }
  }

  //......................................................................
  //
  // Parallel sections
  // -----------------
  //

  // Start a parallel section, using a caller-provided
  // ThreadPoolParallelSection for maintaining the per-section state.
  // Starting a parallel section is just book-keeping; threads are
  // "summoned" to help with the parallel section once it enters
  // parallel loops.  The threads are then retained until the end of the
  // section, being re-used over subsequent loops.

  void StartParallelSectionInternal(PerThread& pt,
                                    ThreadPoolParallelSection& ps) {
    assert((!pt.leading_par_section) && "Nested parallelism not supported");
    assert((!ps.active) && "Starting parallel section, but active already");
    pt.leading_par_section = true;
    if (!pt.tag.Get()) {
      pt.tag = Tag::GetNext();
    }
    ps.dispatch_q_idx = -1;
    ps.dispatch_started = false;
    ps.dispatch_done = false;
    ps.work_done = false;
    ps.tasks_revoked = 0;
    ps.current_dop = 1;
    ps.active = true;
  }

  void StartParallelSection(ThreadPoolParallelSection& ps) override {
    PerThread* pt = GetPerThread();
    StartParallelSectionInternal(*pt, ps);
  }

  // End a parallel section, waiting for all worker threads to exit from
  // section.  Hence, on return, the ThreadPoolParallelSection object
  // can be dealloacted.
  void EndParallelSectionInternal(PerThread& pt,
                                  ThreadPoolParallelSection& ps) {
    assert((pt.leading_par_section) && "Ending parallel section, but none started");
    assert((ps.active) && "Ending parallel section, but not active");
    pt.leading_par_section = false;

    // Notify workers to exit from the section
    ps.active = false;

    // First, attempt to revoke the dispatch task.  If we succeed then
    // we know we revoked _something_ pushed for the current loop.  That
    // may be the dispatch task itself, or it may be a task pushed by
    // the dispatch task.  Those cases are distinguished by whether or
    // not the dispatch task itself has started -- if it has not started
    // then it cannot have pushed tasks.
    if (ps.dispatch_q_idx != -1) {
      Queue& q = worker_data_[ps.dispatch_q_idx].queue;
      if (q.RevokeWithTag(pt.tag, ps.dispatch_w_idx)) {
        if (!ps.dispatch_started.load(std::memory_order_acquire)) {
          // We successfully revoked a task, and saw the dispatch task
          // not started.  Hence we know we revoked the dispatch task.
          // This should be the common case.
          ps.dispatch_q_idx = -1;
        } else {
          // We successfully revoked a task, but saw the dispatch task
          // had started.  Hence we know we revoked one of the _new_
          // tasks created by the dispatcher (not the dispatcher
          // itself).  This should be the rare case, but can occur if
          // one of the tasks created by the dispatcher occupies the
          // exact same slot in a work queue that the dispatcher used.
          ps.tasks_revoked++;
        }
      }
    }

    // Second, if we failed to revoke the dispatch task, wait for it to
    // finish dispatch work.  This avoids new tasks being started
    // concurrently with us attempting to end the parallel section.
    if (ps.dispatch_q_idx != -1) {
      while (!ps.dispatch_done.load(std::memory_order_acquire)) {
        onnxruntime::concurrency::SpinPause();
      }
    }

    // Now we know that dispatch is finshed, we synchronize with the
    // tasks that were created (if any) for the parallel section.  We
    // revoke tasks still in queues, and then wait for any that are
    // still running.
    profiler_.LogStart();
    unsigned tasks_started = static_cast<unsigned>(ps.tasks.size());
    while (!ps.tasks.empty()) {
      const auto& item = ps.tasks.back();
      Queue& q = worker_data_[item.first].queue;
      if (q.RevokeWithTag(pt.tag, item.second)) {
        ps.tasks_revoked++;
      }
      ps.tasks.pop_back();
    }
    profiler_.LogEnd(ThreadPoolProfiler::WAIT_REVOKE);

    // Wait for the dispatch task's own work...
    if (ps.dispatch_q_idx > -1) {
      while (!ps.work_done.load(std::memory_order_acquire)) {
        onnxruntime::concurrency::SpinPause();
      }
    }

    // ...and wait for any other tasks not revoked to finish their work
    auto tasks_to_wait_for = tasks_started - ps.tasks_revoked;
    while (ps.tasks_finished < tasks_to_wait_for) {
      onnxruntime::concurrency::SpinPause();
    }

    // Clear status to allow the ThreadPoolParallelSection to be
    // re-used.
    ps.tasks_finished = 0;
  }

  void EndParallelSection(ThreadPoolParallelSection& ps) override {
    PerThread* pt = GetPerThread();
    EndParallelSectionInternal(*pt, ps);
  }

  //----------------------------------------------------------------------
  //
  // Preferred workers
  // -----------------
  //
  // Initialize the set of hints for preferred worker threads we will
  // use.  We do this once, covering the maximum num_threads_ items,
  // in order to avoid resizing preferred_workers concurrent with
  // access from worker threads.
  //
  // For simplicity we initialize with hints round-robin among the
  // workers.  For simple workloads with 1 main thread this means we
  // will distribute work across the pool of workers.  For workers
  // with multiple main threads it attempts to balance the load.
  //
  // These hints are just used as a starting point, and are updated by
  // the worker thread that actually claims an item (e.g., if an item
  // initially assigned to thread T1 is stolen and executed by T2,
  // then T2 is assigned at the new preferred worker).
  //
  // Note that the hints are held in the _main_ thread that submits
  // work to the pool.  We assume that a thread is primarily
  // submitting work to just one pool, but allow for the pool to
  // change over time.  Hence we allow the hints vector to grow over
  // time.
  //
  // A note on terminology used in the variable names here:
  //
  // dop - degree of parallelism, as seen by the user.  For instance
  //       dop=4 means 4 threads in total: 1 main thread that enters the
  //       loop, plus 1 dispatcher thread, plus 2 additional worker
  //       threads.
  //
  // par_idx - a thread's index within the loop, in the range [0,dop).
  //
  // num_threads_ - the number of worker threads in the thread pool.  A
  //       loop with dop=4 will be common on a pool with 3 threads
  //       (given that the main thread will also participate).
  //
  // q_idx - a worker queue index, in the range [0,num_threads_).
  //
  // preferred_workers - this maps from par_idx values to q_idx.  Hence,
  //        with dop=4 the vector will have length 4, and will identify
  //        which of the workers (0,1,2) should run tasks for the loop.
  //        Note that mapping from par_idx values means that only slots
  //        [1,dop) are actually used in preferred_workers.
  //
  // Here are three examples, all assuming a machine with 4 h/w threads,
  // and ORT configured to use dop=4.
  //
  // * First, suppose that a single job is running a series of loops.
  //   Its main thread enters a parallel loop.  Initially, let's assume
  //   its preferred worker array is [_,0,1,2], writing "_" for the
  //   unusued element for the par_idx=0 work that the main thread will
  //   run.
  //
  //   The main thread schedules the dispatcher task onto worker 0.
  //
  //   The dispatcher task schedules worker tasks onto workers 1 and 2.
  //
  //   The tasks all execute, without any work stealing, on the threads
  //   they were scheduled on.  The preferred worker array remains
  //   [_,0,1,2].
  //
  // * Next, assume we have the same job, and for whatever reason the
  //   preferred workers were initially [_,0,0,0].
  //
  //   The main thread schedules the dispatcher onto worker 0.
  //
  //   This dispatcher task runs on worker 0, and pushes the worker
  //   tasks back onto worker 0's queue.
  //
  //   Workers 1 and 2 are idle, and steal tasks from worker 0.  As the
  //   tasks run, they update the preferred_workers array to record the
  //   workers that execute them.
  //
  //   After the loop, the preferred worker array may now be [_,0,2,1]
  //   or [_,0,1,2], reflecting the fact that the work has got
  //   re-distributed.  The next loop will start out by distributing the
  //   work to those same workers.
  //
  // * Finally, let's assume we have two jobs running on two main
  //   threads, and we are now using DoP=2 in the loops, and have 2
  //   workers in the thread pool (so the machine is not
  //   over-subscribed).
  //
  //   Each main thread has its own preferred_workers, and
  //   let's say initially these are both [_,0].
  //
  //   Here, with DoP=2, each main thread will just dispatch a single
  //   task immediately (there is no need for asynchrony with only one
  //   task to generate).
  //
  //   Initially both main threads will submit these tasks to worker 0.
  //
  //   Once worker 1 steals one of these tasks, the task will update its
  //   preferred worker to be 1.
  //
  //   From that point onwards, the two main threads will dispatch tasks
  //   to separate workers, avoiding the need for further work stealing.

  void InitializePreferredWorkers(InlinedVector<int>& preferred_workers) {
    static std::atomic<unsigned> next_worker{0};

    // preferred_workers[0] isn't supposed to be used, so initializing it with -1 to:
    // a) fault if inappropriately accessed
    // b) avoid wasting next_worker value
    if (preferred_workers.empty()) {
      preferred_workers.push_back(-1);
    }

    // preferred_workers maps from a par_idx to a q_idx, hence we
    // initialize slots in the range [0,num_threads_]
    while (preferred_workers.size() <= num_threads_) {
      preferred_workers.push_back(next_worker++ % num_threads_);
    }
  }

  // Update the preferred worker for par_idx to be the calling thread

  void UpdatePreferredWorker(InlinedVector<int>& preferred_workers,
                             unsigned par_idx) {
    unsigned ran_on_idx = GetPerThread()->thread_id;
    assert(ran_on_idx < num_threads_);
    assert(par_idx < preferred_workers.size());
    preferred_workers[par_idx] = ran_on_idx;
  }

  // Schedule [par_idx_start,par_idx_end) across the preferred workers

  void ScheduleOnPreferredWorkers(PerThread& pt,
                                  ThreadPoolParallelSection& ps,
                                  InlinedVector<int>& preferred_workers,
                                  unsigned par_idx_start,
                                  unsigned par_idx_end,
                                  std::function<void(unsigned)> worker_fn) {
    for (auto par_idx = par_idx_start; par_idx < par_idx_end; ++par_idx) {
      // Look up hint for par_idx.  Note that the hints may have been
      // recorded from a prior thread pool with a different number of
      // threads, hence we must cap at num_threads_.
      assert(par_idx < preferred_workers.size());
      unsigned q_idx = preferred_workers[par_idx] % num_threads_;
      assert(q_idx < num_threads_);
      WorkerData& td = worker_data_[q_idx];
      Queue& q = td.queue;
      unsigned w_idx;

      // Attempt to enqueue the task
      auto push_status = q.PushBackWithTag([worker_fn, par_idx, &preferred_workers, &ps, this]() {
        // Record the worker thread that actually runs this task.
        // This will form the preferred worker for the next loop.
        UpdatePreferredWorker(preferred_workers, par_idx);
        worker_fn(par_idx);
        ps.tasks_finished++;
      },
                                           pt.tag, w_idx);

      // Queue accepted the task; wake the thread that owns the queue.
      // In addition, if the queue was non-empty, attempt to wake
      // another thread (which may then steal the task).
      if (push_status == PushResult::ACCEPTED_IDLE || push_status == PushResult::ACCEPTED_BUSY) {
        ps.tasks.push_back({q_idx, w_idx});
        td.EnsureAwake();
        if (push_status == PushResult::ACCEPTED_BUSY) {
          worker_data_[Rand(&pt.rand) % num_threads_].EnsureAwake();
        }
      }
    }
  }

  //......................................................................
  //
  // Parallel loops
  // --------------
  //
  // Ensure that the ThreadPoolParallelSection has sufficient workers to
  // execute a loop with degree of parallelism n.  We track the number
  // of workers already available to the parallel section, prior to
  // submitting tasks to the work queues to make up the total.
  //
  // Each worker will call in to worker_fn(idx) with a per-worker thread
  // ID.  Note there are different levels of indirection here:
  //
  // - In a single-loop parallel section, worker_fn will directly
  //   execute the threadpool.cc code that implements the parallel loop.
  //
  // - In a multi-loop parallel section, worker_fn is an intermediate
  //   function that is long-lived (i.e., that lasts until the end of
  //   the parallel section, as opposed to just a single loop's
  //   duration).
  //
  // For ordinary parallel sections, RunInParallelInternal dispatch
  // tasks to a number of workers asynchronously.  A worker thread will
  // be selected as the dispatcher that distributes tasks.  This removes
  // the O(n) work off the critical path of starting the first loop
  // iteration, helping maintain good performance on very short loops.
  //
  // See the note on terminology above for the use of variable names
  // here.

  void RunInParallelInternal(PerThread& pt,
                             ThreadPoolParallelSection& ps,
                             unsigned new_dop,
                             bool dispatch_async,
                             std::function<void(unsigned)> worker_fn) {
    // Ensure that the vector of preferred workers is sufficient for the
    // size of the loop we are entering.  We do this before dispatching
    // tasks for the loop in order to avoid any races between changes to
    // the size of the vector and recording the locations that tasks run
    // in as they complete.
    assert(new_dop <= (unsigned)(num_threads_ + 1));
    auto& preferred_workers = pt.preferred_workers;
    InitializePreferredWorkers(preferred_workers);

    // current_dop is the degree of parallelism via any workers already
    // participating in the current parallel section.  Usually, for
    // single-loop parallel sections, current_dop=1.
    unsigned current_dop = ps.current_dop;

    if (current_dop < new_dop) {
      unsigned extra_needed = new_dop - current_dop;

      // Attempt to summon additional workers asynchronously if we
      // need more than one.  Otherwise, we fall back to simple
      // synchronous scheduling.
      if (dispatch_async && extra_needed > 1) {
        assert(current_dop == 1);

        // Task for dispatching work asynchronously.
        Task dispatch_task = [current_dop, new_dop, worker_fn, &preferred_workers, &ps, &pt, this]() {
          // Record that dispatch work has started.  This must occur
          // prior to scheduling tasks, in order to synchronize with
          // EndParallelSectionInternal.  [ If EndParallelSection
          // revoked a task, and then sees distpatch_started=false, then
          // it knows that it revoked the dispatcher.  Conversely, if it
          // revokes a task, and then sees dispatch_started=true, then
          // it knows it revoked a worker task. ]
          ps.dispatch_started.store(true, std::memory_order_seq_cst);

          // Schedule tasks par_idx=[current_dop+1,new_dop)
          ScheduleOnPreferredWorkers(pt, ps, preferred_workers, current_dop + 1, new_dop, worker_fn);
          ps.dispatch_done.store(true, std::memory_order_release);

          // Record the worker thread that actually runs this task.
          // This will form the preferred worker for the next loop.
          UpdatePreferredWorker(preferred_workers, current_dop);

          // Run dispatcher task's own work, par_idx=current_dop
          worker_fn(current_dop);

          // Dispatcher's work complete
          ps.work_done.store(true, std::memory_order_release);
        };

        profiler_.LogStart();
        ps.dispatch_q_idx = preferred_workers[current_dop] % num_threads_;
        WorkerData& dispatch_td = worker_data_[ps.dispatch_q_idx];
        Queue& dispatch_que = dispatch_td.queue;

        // assign dispatch task to selected dispatcher
        auto push_status = dispatch_que.PushBackWithTag(dispatch_task, pt.tag, ps.dispatch_w_idx);
        // Queue accepted the task; wake the thread that owns the queue.
        // In addition, if the queue was non-empty, attempt to wake
        // another thread (which may then steal the task).
        if (push_status == PushResult::ACCEPTED_IDLE || push_status == PushResult::ACCEPTED_BUSY) {
          dispatch_td.EnsureAwake();
          if (push_status == PushResult::ACCEPTED_BUSY) {
            worker_data_[Rand(&pt.rand) % num_threads_].EnsureAwake();
          }
        } else {
          ps.dispatch_q_idx = -1;  // failed to enqueue dispatch_task
        }
        profiler_.LogEnd(ThreadPoolProfiler::DISTRIBUTION_ENQUEUE);
      } else {
        // Synchronous dispatch
        ScheduleOnPreferredWorkers(pt, ps, preferred_workers, current_dop, new_dop, std::move(worker_fn));
      }
      ps.current_dop = new_dop;
    }
  }

  // Run a single parallel loop in an existing parallel section.  This
  // maps directly onto SummonWorkers to create sufficient worker
  // threads for the desired degree of parallelism, followed by
  // dispatching the loop to those workers.
  void RunInParallelSection(ThreadPoolParallelSection& ps,
                            std::function<void(unsigned idx)> fn,
                            unsigned n,
                            std::ptrdiff_t block_size) override {
    ORT_ENFORCE(n <= num_threads_ + 1, "More work items than threads");
    profiler_.LogStartAndCoreAndBlock(block_size);
    PerThread* pt = GetPerThread();
    assert(pt->leading_par_section && "RunInParallel, but not in parallel section");
    assert((n > 1) && "Trivial parallel section; should be avoided by caller");

    // Publish the work to any existing workers in the parallel
    // section, and ensure it is visible to any new threads created
    // below.
    assert((!ps.current_loop) && "RunInParallelSection, but loop already active");
    ThreadPoolLoop loop{std::move(fn), n};
    ps.current_loop = &loop;

    // Increase the worker count if needed.  Each worker will pick up
    // loops to execute from the current parallel section.
    std::function<void(unsigned)> worker_fn = [&ps](unsigned par_idx) {
      while (ps.active) {
        if (ps.current_loop.load() == nullptr) {
          onnxruntime::concurrency::SpinPause();
        } else {
          ps.workers_in_loop++;
          ThreadPoolLoop* work_item = ps.current_loop;
          if (work_item && par_idx < work_item->threads_needed) {
            work_item->fn(par_idx);
          }
          ps.workers_in_loop--;
        }
      }
    };
    RunInParallelInternal(*pt, ps, n, false, std::move(worker_fn));
    assert(ps.dispatch_q_idx == -1);
    profiler_.LogEndAndStart(ThreadPoolProfiler::DISTRIBUTION);

    // Run work in the main thread
    loop.fn(0);
    profiler_.LogEndAndStart(ThreadPoolProfiler::RUN);

    // Wait for workers to exit the loop
    ps.current_loop = 0;
    while (ps.workers_in_loop) {
      onnxruntime::concurrency::SpinPause();
    }
    profiler_.LogEnd(ThreadPoolProfiler::WAIT);
  }

  // Run a single parallel loop _without_ a parallel section.  This is a
  // special case of RunInParallelSection, avoiding code paths for
  // handing off multiple loops to the pool of workers.
  // For main thread:
  //  1. select a dispatcher and do job distribution;
  //  2. run fn(0);
  //  3, wait for all;
  // For dispatcher:
  //  1. distribute jobs to all other threads;
  //  2. run fn(...) itself.
  // For all other threads:
  //  1. run fn(...);
  void RunInParallel(std::function<void(unsigned idx)> fn, unsigned n, std::ptrdiff_t block_size) override {
    ORT_ENFORCE(n <= num_threads_ + 1, "More work items than threads");
    profiler_.LogStartAndCoreAndBlock(block_size);
    PerThread* pt = GetPerThread();
    ThreadPoolParallelSection ps;
    StartParallelSectionInternal(*pt, ps);
    RunInParallelInternal(*pt, ps, n, true, fn);  // select dispatcher and do job distribution;
    profiler_.LogEndAndStart(ThreadPoolProfiler::DISTRIBUTION);
    fn(0);  // run fn(0)
    profiler_.LogEndAndStart(ThreadPoolProfiler::RUN);
    EndParallelSectionInternal(*pt, ps);  // wait for all
    profiler_.LogEnd(ThreadPoolProfiler::WAIT);
  }

  int NumThreads() const final {
    return num_threads_;
  }

  int CurrentThreadId() const final {
    const PerThread* pt = const_cast<ThreadPoolTempl*>(this)->GetPerThread();
    if (pt->pool == this) {
      return pt->thread_id;
    }
    return -1;
  }

  void EnableSpinning() {
    spin_loop_status_ = SpinLoopStatus::kBusy;
  }

  void DisableSpinning() {
    spin_loop_status_ = SpinLoopStatus::kIdle;
  }

 private:
  void ComputeCoprimes(int N, Eigen::MaxSizeVector<unsigned>* coprimes) {
    for (int i = 1; i <= N; i++) {
      unsigned a = i;
      unsigned b = N;
      // If GCD(a, b) == 1, then a and b are coprimes.
      while (b != 0) {
        unsigned tmp = a;
        a = b;
        b = tmp % b;
      }
      if (a == 1) {
        coprimes->push_back(i);
      }
    }
  }

  typedef typename Environment::EnvThread Thread;
  struct WorkerData;

  // PerThread objects are allocated in thread-local storage and
  // allocated on the thread's first call to GetPerThread.  PerThread
  // objects are allocated for all threads that submit work to the
  // thread pool, in addition to threads within the pool.
  //
  // In contrast, the WorkerData objects are allocated only for the
  // threads in the pool, and their lifetime is managed along with the
  // pool.

#ifdef _MSC_VER
#pragma warning(push)
// C4324: structure was padded due to alignment specifier
#pragma warning(disable : 4324)
#endif  // _MSC_VER

  struct ORT_ALIGN_TO_AVOID_FALSE_SHARING PerThread {
    constexpr PerThread() : pool(nullptr) {
    }
    ThreadPoolTempl* pool;            // Parent pool, or null for normal threads.
    bool initialized{false};          // Non-trivial initialization ran (e.g. for RNG)
    uint64_t rand{0};                 // Random generator state.
    int thread_id{-1};                // Worker thread index in pool.
    Tag tag{};                        // Work item tag used to identify this thread.
    bool leading_par_section{false};  // Leading a parallel section (used only for asserts)

    // When this thread is entering a parallel section, it will
    // initially push work to this set of workers.  The aim is to
    // retain cache state within the workers, and to reduce the number
    // of times that the work-stealing code paths are used for
    // rebalancing.
    InlinedVector<int> preferred_workers;
  };

#ifdef _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER

  struct WorkerData {
    constexpr WorkerData() : thread(), queue() {
    }
    std::unique_ptr<Thread> thread;
    Queue queue;

    // Each thread has a status, available read-only without locking, and protected
    // by the mutex field below for updates.  The status is used for three
    // purposes:
    //
    // 1. To identify threads that are good candidates to push work to.
    //    We prefer to push work to threads that are actively spinning (no need
    //    for an OS wake-up, and no need for current work to finish).  After that, we
    //    prefer to push work to threads that are blocked (no need to wait for the
    //    current work to finish).
    //
    // 2. To identify threads that are good candidates to steal work from.  We
    //    prefer to steal work from threads that are active outside the worker loop.
    //    This avoids "snatching" new work away from a thread that has just been
    //    given it but not yet noticed.
    //
    // 3. When pushing work to a thread, we use the status read-only to identify
    //    when we need to wake the thread.  This read-only check avoids the
    //    need for mutex / condvar operations in the case where the thread pool
    //    remains busy.

    enum class ThreadStatus : uint8_t {
      Spinning,  // Spinning in the work loop, and other cases (initialization) where
                 // the thread will soon be in the loop
      Active,    // Running user code, not waiting for work
      Blocking,  // In the process of blocking; may no longer notice work pushed to it
      Blocked,   // Blocked on cv
      Waking,    // Not yet back in the worker loop, but wake-up notification sent
    };

    ThreadStatus GetStatus() const {
      return status;
    }

    // State transitions, called from other threads

    // We employ mutex for synchronizing on Blocked/Waking state (EnsureAwake/SeBlocked)
    // to wakeup the thread in the event it goes to sleep. Because thread status
    // is an atomic member the lock is not necessary to update it.
    // Thus, we do not obtain the mutex when we set Active/Spinning state for the thread.
    // While manipulating under the mutex, we employ relaxed semantics so the compiler is not restricted
    // any further.
    void EnsureAwake() {
      ThreadStatus seen = GetStatus();
      if (seen == ThreadStatus::Blocking ||
          seen == ThreadStatus::Blocked) {
        std::unique_lock<std::mutex> lk(mutex);
        // Blocking state exists only transiently during the SetBlock() method
        // while holding the lock.  We may observe it at the start of this
        // function, but after acquiring the lock then the target thread
        // will either be blocked or not.
        seen = status.load(std::memory_order_relaxed);
        assert(seen != ThreadStatus::Blocking);
        if (seen == ThreadStatus::Blocked) {
          status.store(ThreadStatus::Waking, std::memory_order_relaxed);
          lk.unlock();
          cv.notify_one();
        }
      }
    }

    // State transitions, called only from the thread itself
    // The lock is only used in the synchronization between EnsureAwake and SetBlocked,
    // while the Active vs Spinning states are just used as a hint for work stealing
    // (prefer to steal from a thread that is actively running a task, rather than stealing from
    // a thread that is spinning and likely to pick up the task itself).
    void SetActive() {
      status = ThreadStatus::Active;
    }

    void SetSpinning() {
      status = ThreadStatus::Spinning;
    }

    bool SetBlocked(std::function<bool()> should_block,
                    std::function<void()> post_block) {
      std::unique_lock<std::mutex> lk(mutex);
      auto old_status = status.exchange(ThreadStatus::Blocking, std::memory_order_seq_cst);
      if (old_status != ThreadStatus::Spinning) {
        // Encountered a logical error
        return false;
      }
      if (should_block()) {
        status.store(ThreadStatus::Blocked, std::memory_order_relaxed);
        do {
          cv.wait(lk);
        } while (status.load(std::memory_order_relaxed) == ThreadStatus::Blocked);
        post_block();
      }
      status.store(ThreadStatus::Spinning, std::memory_order_relaxed);
      return true;
    }

   private:
    std::atomic<ThreadStatus> status{ThreadStatus::Spinning};
    std::mutex mutex;
    std::condition_variable cv;
  };

  Environment& env_;
  const unsigned num_threads_;
  const bool allow_spinning_;
  const bool set_denormal_as_zero_;
  Eigen::MaxSizeVector<WorkerData> worker_data_;
  Eigen::MaxSizeVector<Eigen::MaxSizeVector<unsigned>> all_coprimes_;
  std::atomic<unsigned> blocked_;  // Count of blocked workers, used as a termination condition
  std::atomic<bool> done_;

  // SpinLoopStatus indicates whether the main worker spinning (inner) loop should exit immediately when there is
  // no work available (kIdle) or whether it should follow the configured spin-then-block policy (kBusy).
  // This lets the ORT session layer hint to the thread pool that it should stop spinning in between
  // requests.
  enum class SpinLoopStatus {
    kIdle,
    kBusy
  };

  // Default is no control over spinning
  std::atomic<SpinLoopStatus> spin_loop_status_{SpinLoopStatus::kBusy};

  // Wake any blocked workers so that they can cleanly exit WorkerLoop().  For
  // a clean exit, each thread will observe (1) done_ set, indicating that the
  // destructor has been called, (2) all threads blocked, and (3) no
  // items in the work queues.

  void WakeAllWorkersForExit() {
    for (auto& td : worker_data_) {
      td.EnsureAwake();
    }
  }

  // Main worker thread loop.
  void WorkerLoop(int thread_id) {
    PerThread* pt = GetPerThread();
    WorkerData& td = worker_data_[thread_id];
    Queue& q = td.queue;
    bool should_exit = false;
    pt->pool = this;
    pt->thread_id = thread_id;

    assert(td.GetStatus() == WorkerData::ThreadStatus::Spinning);

    constexpr int log2_spin = 20;
    const int spin_count = allow_spinning_ ? (1ull << log2_spin) : 0;
    const int steal_count = spin_count / 100;

    SetDenormalAsZero(set_denormal_as_zero_);
    profiler_.LogThreadId(thread_id);

    while (!should_exit) {
      Task t = q.PopFront();
      if (!t) {
        // Spin waiting for work.
        for (int i = 0; i < spin_count && !done_; i++) {
          if (((i + 1) % steal_count == 0)) {
            t = Steal(StealAttemptKind::TRY_ONE);
          } else {
            t = q.PopFront();
          }
          if (t) break;

          if (spin_loop_status_.load(std::memory_order_relaxed) == SpinLoopStatus::kIdle) {
            break;
          }
          onnxruntime::concurrency::SpinPause();
        }

        // Attempt to block
        if (!t) {
          if (!td.SetBlocked(  // Pre-block test
                  [&]() -> bool {
                    bool should_block = true;
                    // Check whether work was pushed to us while attempting to block.  We make
                    // this test while holding the per-thread status lock, and after setting
                    // our status to ThreadStatus::Blocking.
                    //
                    // This synchronizes with ThreadPool::Schedule which pushes work to the queue
                    // and then tests for ThreadStatus::Blocking/Blocked (via EnsureAwake):
                    //
                    // Main thread:                    Worker:
                    //   #1 Push work                   #A Set status blocking
                    //   #2 Read worker status          #B Check queue
                    //   #3 Wake if blocking/blocked
                    //
                    // If #A is before #2 then main sees worker blocked and wakes
                    //
                    // If #A if after #2 then #B will see #1, and we abandon blocking
                    assert(!t);
                    t = q.PopFront();
                    if (t) {
                      should_block = false;
                    }

                    // No work pushed to us, continue attempting to block.  The remaining
                    // test  is to synchronize with termination requests.  If we are
                    // shutting down and all worker threads blocked without work, that's
                    // we are done.
                    if (should_block) {
                      blocked_++;
                      if (done_ && blocked_ == num_threads_) {
                        should_block = false;
                        // Almost done, but need to re-check queues.
                        // Consider that all queues are empty and all worker threads are preempted
                        // right after incrementing blocked_ above. Now a free-standing thread
                        // submits work and calls destructor (which sets done_). If we don't
                        // re-check queues, we will exit leaving the work unexecuted.
                        if (NonEmptyQueueIndex() != -1) {
                          // Note: we must not pop from queues before we decrement blocked_,
                          // otherwise the following scenario is possible. Consider that instead
                          // of checking for emptiness we popped the only element from queues.
                          // Now other worker threads can start exiting, which is bad if the
                          // work item submits other work. So we just check emptiness here,
                          // which ensures that all worker threads exit at the same time.
                          blocked_--;
                        } else {
                          should_exit = true;
                        }
                      }
                    }
                    return should_block;
                  },
                  // Post-block update (executed only if we blocked)
                  [&]() {
                    blocked_--;
                  })) {
            // Encountered a fatal logic error in SetBlocked
            should_exit = true;
            break;
          }
          // Thread just unblocked.  Unless we picked up work while
          // blocking, or are exiting, then either work was pushed to
          // us, or it was pushed to an overloaded queue
          if (!t) t = q.PopFront();
          if (!t) t = Steal(StealAttemptKind::TRY_ALL);
        }
      }

      if (t) {
        td.SetActive();
        t();
        profiler_.LogRun(thread_id);
        td.SetSpinning();
      }
    }

    // Whichever thread(s) observe the termination conditions are responsible for waking
    // any other threads that have remained blocked.
    if (should_exit) {
      WakeAllWorkersForExit();
    }
  }

  // Steal tries to steal work from other worker threads in a
  // best-effort manner.  We steal only from threads that are running
  // in user code (ThreadStatus::Active).  The intuition behind this
  // is that the thread is busy with other work, and we will avoid
  // "snatching" work from a thread which is just about to notice the
  // work itself.

  Task Steal(StealAttemptKind steal_kind) {
    PerThread* pt = GetPerThread();
    unsigned size = num_threads_;
    unsigned num_attempts = (steal_kind == StealAttemptKind::TRY_ALL) ? size : 1;
    unsigned r = Rand(&pt->rand);
    unsigned inc = all_coprimes_[size - 1][r % all_coprimes_[size - 1].size()];
    unsigned victim = r % size;

    for (unsigned i = 0; i < num_attempts; i++) {
      assert(victim < size);
      if (worker_data_[victim].GetStatus() == WorkerData::ThreadStatus::Active) {
        Task t = worker_data_[victim].queue.PopBack();
        if (t) {
          return t;
        }
      }
      victim += inc;
      if (victim >= size) {
        victim -= size;
      }
    }

    return Task();
  }

  int NonEmptyQueueIndex() {
    PerThread* pt = GetPerThread();
    const unsigned size = static_cast<unsigned>(worker_data_.size());
    unsigned r = Rand(&pt->rand);
    unsigned inc = all_coprimes_[size - 1][r % all_coprimes_[size - 1].size()];
    unsigned victim = r % size;
    for (unsigned i = 0; i < size; i++) {
      if (!worker_data_[victim].queue.Empty()) {
        return victim;
      }
      victim += inc;
      if (victim >= size) {
        victim -= size;
      }
    }
    return -1;
  }

  static EIGEN_STRONG_INLINE uint64_t GlobalThreadIdHash() {
    return std::hash<std::thread::id>()(std::this_thread::get_id());
  }

  static EIGEN_STRONG_INLINE PerThread* GetPerThread() {
    static thread_local PerThread per_thread_;
    PerThread* pt = &per_thread_;
    if (!pt->initialized) {
      pt->rand = GlobalThreadIdHash();
      pt->initialized = true;
    }
    return pt;
  }

  static EIGEN_STRONG_INLINE unsigned Rand(uint64_t* state) {
    uint64_t current = *state;
    // Update the internal state
    *state = current * 6364136223846793005ULL + 0xda3e39cb94b95bdbULL;
    // Generate the random output (using the PCG-XSH-RS scheme)
    return static_cast<unsigned>((current ^ (current >> 22)) >> (22 + (current >> 61)));
  }
};

}  // namespace concurrency

}  // namespace onnxruntime
