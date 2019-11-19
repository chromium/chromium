// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"

#include <stdint.h>

#include <list>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"

namespace ui {
namespace {

class WrappedTask;
class PumpableTaskRunner;
using WrappedTaskQueue = std::list<WrappedTask*>;
using EventTimedWaitCallback =
    base::RepeatingCallback<void(base::WaitableEvent*, base::TimeDelta)>;

// A wrapper for IPCs and tasks that we may potentially execute in
// WaitForSingleTaskToRun. Because these tasks are sent to two places to run,
// we have to wrap them in this structure and track whether or not they have run
// yet, to avoid running them twice.
class WrappedTask {
 public:
  WrappedTask(base::OnceClosure closure, base::TimeDelta delay);
  ~WrappedTask();
  bool ShouldRunBefore(const WrappedTask& other);
  void Run();
  void AddToTaskRunnerQueue(PumpableTaskRunner* pumpable_task_runner);
  void RemoveFromTaskRunnerQueue();
  const base::TimeTicks& can_run_time() const { return can_run_time_; }

 private:
  base::OnceClosure closure_;
  base::TimeTicks can_run_time_;
  bool has_run_;
  uint64_t sequence_number_;
  WrappedTaskQueue::iterator iterator_;

  // Back pointer to the pumpable task runner that this task is enqueued in.
  scoped_refptr<PumpableTaskRunner> pumpable_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WrappedTask);
};

// The PumpableTaskRunner is a task runner that will wrap tasks in an
// WrappedTask, enqueues that wrapped task in the queue to be pumped via
// WaitForSingleWrappedTaskToRun during resizes, and posts the task to a
// target task runner. The posted task will run only once, either through a
// WaitForSingleWrappedTaskToRun call or through the target task runner.
class PumpableTaskRunner : public base::SingleThreadTaskRunner {
 public:
  PumpableTaskRunner(
      const EventTimedWaitCallback& event_timed_wait_callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& target_task_runner);

  // Enqueue WrappedTask and post it to |target_task_runner_|.
  bool EnqueueAndPostWrappedTask(const base::Location& from_here,
                                 std::unique_ptr<WrappedTask> task,
                                 base::TimeDelta delay);

  // Wait at most |max_delay| to run an enqueued task.
  bool WaitForSingleWrappedTaskToRun(const base::TimeDelta& max_delay);

  // base::SingleThreadTaskRunner implementation:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;

  bool RunsTasksInCurrentSequence() const override;

 private:
  friend class WrappedTask;

  ~PumpableTaskRunner() override;

  // A queue of live messages.  Must hold |task_queue_lock_| to access. Tasks
  // are added only on the IO thread and removed only on the UI thread.  The
  // WrappedTask objects are removed from the queue when they are run (by
  // |target_task_runner_| or by a call to WaitForSingleWrappedTaskToRun
  // removing them out of the queue, or by TaskRunner when it is destroyed).
  WrappedTaskQueue task_queue_;
  base::Lock task_queue_lock_;

  // Event used to wake up the UI thread if it is sleeping in
  // WaitForSingleTaskToRun.
  base::WaitableEvent event_;

  // Callback to call TimedWait on |event_| from an appropriate class.
  EventTimedWaitCallback event_timed_wait_callback_;

  scoped_refptr<base::SingleThreadTaskRunner> target_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PumpableTaskRunner);
};

base::LazyInstance<WindowResizeHelperMac>::Leaky g_window_resize_helper =
    LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// WrappedTask

WrappedTask::WrappedTask(base::OnceClosure closure, base::TimeDelta delay)
    : closure_(std::move(closure)),
      can_run_time_(base::TimeTicks::Now() + delay),
      has_run_(false),
      sequence_number_(0) {}

WrappedTask::~WrappedTask() {
  RemoveFromTaskRunnerQueue();
}

bool WrappedTask::ShouldRunBefore(const WrappedTask& other) {
  if (can_run_time_ < other.can_run_time_)
    return true;
  if (can_run_time_ > other.can_run_time_)
    return false;
  if (sequence_number_ < other.sequence_number_)
    return true;
  if (sequence_number_ > other.sequence_number_)
    return false;
  // Sequence numbers are unique, so this should never happen.
  NOTREACHED();
  return false;
}

void WrappedTask::Run() {
  if (has_run_)
    return;
  RemoveFromTaskRunnerQueue();
  has_run_ = true;
  std::move(closure_).Run();
}

void WrappedTask::AddToTaskRunnerQueue(
    PumpableTaskRunner* pumpable_task_runner) {
  pumpable_task_runner_ = pumpable_task_runner;
  base::AutoLock lock(pumpable_task_runner_->task_queue_lock_);
  static uint64_t last_sequence_number = 0;
  last_sequence_number += 1;
  sequence_number_ = last_sequence_number;
  iterator_ = pumpable_task_runner_->task_queue_.insert(
      pumpable_task_runner_->task_queue_.end(), this);
}

void WrappedTask::RemoveFromTaskRunnerQueue() {
  if (!pumpable_task_runner_.get())
    return;
  // The scope of the task runner's lock must be limited because removing
  // this reference to the task runner may destroy it.
  {
    base::AutoLock lock(pumpable_task_runner_->task_queue_lock_);
    pumpable_task_runner_->task_queue_.erase(iterator_);
    iterator_ = pumpable_task_runner_->task_queue_.end();
  }
  pumpable_task_runner_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// PumpableTaskRunner

PumpableTaskRunner::PumpableTaskRunner(
    const EventTimedWaitCallback& event_timed_wait_callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& target_task_runner)
    : event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
             base::WaitableEvent::InitialState::NOT_SIGNALED),
      event_timed_wait_callback_(event_timed_wait_callback),
      target_task_runner_(target_task_runner) {}

PumpableTaskRunner::~PumpableTaskRunner() {
  // Because tasks hold a reference to the task runner, the task queue must
  // be empty when it is destroyed.
  DCHECK(task_queue_.empty());
}

bool PumpableTaskRunner::WaitForSingleWrappedTaskToRun(
    const base::TimeDelta& max_delay) {
  base::TimeTicks stop_waiting_time = base::TimeTicks::Now() + max_delay;

  for (;;) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    base::TimeTicks next_task_time = stop_waiting_time;

    // Find the first task to execute in the list. This lookup takes O(n) time,
    // but n is rarely more than 2, and has never been observed to be more than
    // 12.
    WrappedTask* task_to_execute = NULL;
    {
      base::AutoLock lock(task_queue_lock_);

      for (WrappedTaskQueue::iterator it = task_queue_.begin();
           it != task_queue_.end(); ++it) {
        WrappedTask* potential_task = *it;

        // If this task is scheduled for the future, take it into account when
        // deciding how long to sleep, and continue on to the next task.
        if (potential_task->can_run_time() > current_time) {
          if (potential_task->can_run_time() < next_task_time)
            next_task_time = potential_task->can_run_time();
          continue;
        }
        // If there is a better candidate than this task, continue to the next
        // task.
        if (task_to_execute &&
            task_to_execute->ShouldRunBefore(*potential_task)) {
          continue;
        }
        task_to_execute = potential_task;
      }
    }

    if (task_to_execute) {
      task_to_execute->Run();
      return true;
    }

    // Calculate how much time we have left before we have to stop waiting or
    // until a currently-enqueued task will be ready to run.
    base::TimeDelta max_sleep_time = next_task_time - current_time;
    if (max_sleep_time <= base::TimeDelta::FromMilliseconds(0))
      break;

    event_timed_wait_callback_.Run(&event_, max_sleep_time);
  }

  return false;
}

bool PumpableTaskRunner::EnqueueAndPostWrappedTask(
    const base::Location& from_here,
    std::unique_ptr<WrappedTask> task,
    base::TimeDelta delay) {
  task->AddToTaskRunnerQueue(this);

  // Notify anyone waiting on the UI thread that there is a new entry in the
  // task map.  If they don't find the entry they are looking for, then they
  // will just continue waiting.
  event_.Signal();

  return target_task_runner_->PostDelayedTask(
      from_here, base::BindOnce(&WrappedTask::Run, std::move(task)), delay);
}

////////////////////////////////////////////////////////////////////////////////
// PumpableTaskRunner, base::SingleThreadTaskRunner implementation:

bool PumpableTaskRunner::PostDelayedTask(const base::Location& from_here,
                                         base::OnceClosure task,
                                         base::TimeDelta delay) {
  return EnqueueAndPostWrappedTask(
      from_here, std::make_unique<WrappedTask>(std::move(task), delay), delay);
}

bool PumpableTaskRunner::PostNonNestableDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  // The correctness of non-nestable events hasn't been proven for this
  // structure.
  NOTREACHED();
  return false;
}

bool PumpableTaskRunner::RunsTasksInCurrentSequence() const {
  return target_task_runner_->RunsTasksInCurrentSequence();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowResizeHelperMac

scoped_refptr<base::SingleThreadTaskRunner> WindowResizeHelperMac::task_runner()
    const {
  return task_runner_;
}

// static
WindowResizeHelperMac* WindowResizeHelperMac::Get() {
  return g_window_resize_helper.Pointer();
}

void WindowResizeHelperMac::Init(
    const scoped_refptr<base::SingleThreadTaskRunner>& target_task_runner) {
  DCHECK(!task_runner_);
  task_runner_ = new PumpableTaskRunner(
      base::BindRepeating(&WindowResizeHelperMac::EventTimedWait),
      target_task_runner);
}

void WindowResizeHelperMac::ShutdownForTests() {
  task_runner_ = nullptr;
}

bool WindowResizeHelperMac::WaitForSingleTaskToRun(
    const base::TimeDelta& max_delay) {
  PumpableTaskRunner* pumpable_task_runner =
      static_cast<PumpableTaskRunner*>(task_runner_.get());
  if (!pumpable_task_runner)
    return false;
  return pumpable_task_runner->WaitForSingleWrappedTaskToRun(max_delay);
}

WindowResizeHelperMac::WindowResizeHelperMac() {}
WindowResizeHelperMac::~WindowResizeHelperMac() {}

void WindowResizeHelperMac::EventTimedWait(base::WaitableEvent* event,
                                           base::TimeDelta delay) {
  // http://crbug.com/902829
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
  event->TimedWait(delay);
}

}  // namespace ui
