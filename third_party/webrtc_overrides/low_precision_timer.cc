// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/low_precision_timer.h"

#include <optional>

#include "base/check.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"
#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

namespace blink {

LowPrecisionTimer::SchedulableCallback::SchedulableCallback(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void()> callback,
    base::TimeDelta repeated_delay)
    : task_runner_(std::move(task_runner)),
      callback_(std::move(callback)),
      repeated_delay_(std::move(repeated_delay)) {}

LowPrecisionTimer::SchedulableCallback::~SchedulableCallback() {
  DCHECK(!is_active_);
}

void LowPrecisionTimer::SchedulableCallback::Schedule(
    base::TimeTicks scheduled_time) {
  base::AutoLock auto_scheduled_time_lock(scheduled_time_lock_);
  DCHECK_EQ(scheduled_time_, base::TimeTicks::Max())
      << "The callback has already been scheduled.";
  scheduled_time_ = scheduled_time;
  // Snap target time to metronome tick!
  base::TimeTicks target_time = TimerBasedTickProvider::TimeSnappedToNextTick(
      scheduled_time_, TimerBasedTickProvider::kDefaultPeriod);
  task_runner_->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&LowPrecisionTimer::SchedulableCallback::MaybeRun, this),
      target_time, base::subtle::DelayPolicy::kPrecise);
}

bool LowPrecisionTimer::SchedulableCallback::IsScheduled() {
  base::AutoLock auto_scheduled_time_lock(scheduled_time_lock_);
  return scheduled_time_ != base::TimeTicks::Max();
}

base::TimeTicks LowPrecisionTimer::SchedulableCallback::Inactivate() {
  // If we're inside the task runner and the task is currently running, that
  // means Inactivate() was called from inside the callback, and |active_lock_|
  // is already aquired on the current task runner. Acquiring it again would
  // cause deadlock.
  bool is_inactivated_by_callback =
      task_runner_->RunsTasksInCurrentSequence() && is_currently_running_;
  std::optional<base::AutoLock> auto_active_lock;
  if (!is_inactivated_by_callback) {
    auto_active_lock.emplace(active_lock_);
  }
  is_active_ = false;
  repeated_delay_ = base::TimeDelta();  // Prevent automatic re-schedule.
  base::AutoLock auto_scheduled_time_lock(scheduled_time_lock_);
  return scheduled_time_;
}

void LowPrecisionTimer::SchedulableCallback::MaybeRun() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Run unless we've been cancelled.
  base::AutoLock auto_active_lock(active_lock_);
  if (!is_active_) {
    return;
  }
  {
    // Reset scheduled time as to allow re-scheduling. Release the lock to allow
    // the callback to re-schedule without deadlock.
    base::AutoLock auto_scheduled_time_lock(scheduled_time_lock_);
    scheduled_time_ = base::TimeTicks::Max();
  }
  is_currently_running_ = true;
  callback_.Run();
  is_currently_running_ = false;
  if (!repeated_delay_.is_zero()) {
    Schedule(base::TimeTicks::Now() + repeated_delay_);
  }
}

LowPrecisionTimer::LowPrecisionTimer(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void()> callback)
    : callback_(std::move(callback)), task_runner_(std::move(task_runner)) {}

LowPrecisionTimer::~LowPrecisionTimer() {
  DCHECK(is_shutdown_);
  DCHECK(!schedulable_callback_);
}

void LowPrecisionTimer::Shutdown() {
  base::AutoLock auto_lock(lock_);
  if (is_shutdown_) {
    // Already shut down.
    return;
  }
  if (schedulable_callback_) {
    schedulable_callback_->Inactivate();
    schedulable_callback_ = nullptr;
  }
  is_shutdown_ = true;
}

void LowPrecisionTimer::StartOneShot(base::TimeDelta delay) {
  DCHECK_GE(delay, base::TimeDelta());
  base::AutoLock auto_lock(lock_);
  DCHECK(!is_shutdown_);
  repeated_delay_ = base::TimeDelta();  // Not repeating.
  ScheduleCallback(base::TimeTicks::Now() + delay);
}

void LowPrecisionTimer::StartRepeating(base::TimeDelta delay) {
  DCHECK_GE(delay, base::TimeDelta());
  base::AutoLock auto_lock(lock_);
  DCHECK(!is_shutdown_);
  repeated_delay_ = delay;
  ScheduleCallback(base::TimeTicks::Now() + delay);
}

bool LowPrecisionTimer::IsActive() {
  base::AutoLock auto_lock(lock_);
  if (!schedulable_callback_) {
    return false;
  }
  if (!repeated_delay_.is_zero()) {
    return true;
  }
  return schedulable_callback_->IsScheduled();
}

void LowPrecisionTimer::Stop() {
  base::AutoLock auto_lock(lock_);
  if (!schedulable_callback_)
    return;
  repeated_delay_ = base::TimeDelta();  // Not repeating.
  schedulable_callback_->Inactivate();
  schedulable_callback_ = nullptr;
}

// EXCLUSIVE_LOCKS_REQUIRED(lock_)
void LowPrecisionTimer::ScheduleCallback(base::TimeTicks scheduled_time) {
  if (!schedulable_callback_) {
    schedulable_callback_ = base::MakeRefCounted<SchedulableCallback>(
        task_runner_, callback_, repeated_delay_);
  }
  schedulable_callback_->Schedule(scheduled_time);
}

// EXCLUSIVE_LOCKS_REQUIRED(lock_)
void LowPrecisionTimer::RescheduleCallback() {
  if (!schedulable_callback_)
    return;
  base::TimeTicks cancelled_scheduled_time =
      schedulable_callback_->Inactivate();
  schedulable_callback_ = nullptr;
  if (cancelled_scheduled_time == base::TimeTicks::Max())
    return;  // We don't have a scheduled time.
  ScheduleCallback(cancelled_scheduled_time);
}

void LowPrecisionTimer::MoveToNewTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  base::AutoLock auto_lock(lock_);
  DCHECK(task_runner);
  task_runner_ = std::move(task_runner);
  RescheduleCallback();
}

}  // namespace blink
