// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/webrtc_timer.h"

#include "base/check.h"
#include "third_party/webrtc_overrides/metronome_task_queue_factory.h"

namespace blink {

const base::Feature kWebRtcTimerUsesMetronome{
    "WebRtcTimerUsesMetronome", base::FEATURE_DISABLED_BY_DEFAULT};

WebRtcTimer::SchedulableCallback::SchedulableCallback(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void()> callback,
    bool use_metronome,
    base::TimeDelta repeated_delay)
    : task_runner_(std::move(task_runner)),
      callback_(std::move(callback)),
      use_metronome_(use_metronome),
      repeated_delay_(std::move(repeated_delay)) {}

WebRtcTimer::SchedulableCallback::~SchedulableCallback() {
  DCHECK(!is_active_);
}

void WebRtcTimer::SchedulableCallback::Schedule(
    base::TimeTicks scheduled_time) {
  base::AutoLock auto_scheduled_time_lock(scheduled_time_lock_);
  DCHECK_EQ(scheduled_time_, base::TimeTicks::Max())
      << "The callback has already been scheduled.";
  scheduled_time_ = scheduled_time;
  base::TimeTicks target_time = scheduled_time_;
  if (use_metronome_) {
    // Snap target time to metronome tick!
    target_time = MetronomeSource::TimeSnappedToNextTick(target_time);
  }
  task_runner_->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&WebRtcTimer::SchedulableCallback::MaybeRun, this),
      target_time, base::subtle::DelayPolicy::kPrecise);
}

bool WebRtcTimer::SchedulableCallback::IsScheduled() {
  base::AutoLock auto_scheduled_time_lock(scheduled_time_lock_);
  return scheduled_time_ != base::TimeTicks::Max();
}

base::TimeTicks WebRtcTimer::SchedulableCallback::Inactivate() {
  // If we're inside the task runner and the task is currently running, that
  // means Inactivate() was called from inside the callback, and |active_lock_|
  // is already aquired on the current task runner. Acquiring it again would
  // cause deadlock.
  bool is_inactivated_by_callback =
      task_runner_->RunsTasksInCurrentSequence() && is_currently_running_;
  std::unique_ptr<base::AutoLock> auto_active_lock;
  if (!is_inactivated_by_callback) {
    auto_active_lock = std::make_unique<base::AutoLock>(active_lock_);
  }
  is_active_ = false;
  repeated_delay_ = base::TimeDelta();  // Prevent automatic re-schedule.
  base::AutoLock auto_scheduled_time_lock(scheduled_time_lock_);
  return scheduled_time_;
}

void WebRtcTimer::SchedulableCallback::MaybeRun() {
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

WebRtcTimer::WebRtcTimer(scoped_refptr<base::SequencedTaskRunner> task_runner,
                         base::RepeatingCallback<void()> callback)
    : callback_(std::move(callback)),
      use_metronome_(base::FeatureList::IsEnabled(kWebRtcTimerUsesMetronome)),
      task_runner_(std::move(task_runner)) {}

WebRtcTimer::~WebRtcTimer() {
  DCHECK(is_shutdown_);
  DCHECK(!schedulable_callback_);
}

void WebRtcTimer::Shutdown() {
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

void WebRtcTimer::StartOneShot(base::TimeDelta delay) {
  DCHECK_GE(delay, base::TimeDelta());
  base::AutoLock auto_lock(lock_);
  DCHECK(!is_shutdown_);
  repeated_delay_ = base::TimeDelta();  // Not repeating.
  ScheduleCallback(base::TimeTicks::Now() + delay);
}

void WebRtcTimer::StartRepeating(base::TimeDelta delay) {
  DCHECK_GE(delay, base::TimeDelta());
  base::AutoLock auto_lock(lock_);
  DCHECK(!is_shutdown_);
  repeated_delay_ = delay;
  ScheduleCallback(base::TimeTicks::Now() + delay);
}

bool WebRtcTimer::IsActive() {
  base::AutoLock auto_lock(lock_);
  if (!schedulable_callback_) {
    return false;
  }
  if (!repeated_delay_.is_zero()) {
    return true;
  }
  return schedulable_callback_->IsScheduled();
}

void WebRtcTimer::Stop() {
  base::AutoLock auto_lock(lock_);
  if (!schedulable_callback_)
    return;
  repeated_delay_ = base::TimeDelta();  // Not repeating.
  schedulable_callback_->Inactivate();
  schedulable_callback_ = nullptr;
}

// EXCLUSIVE_LOCKS_REQUIRED(lock_)
void WebRtcTimer::ScheduleCallback(base::TimeTicks scheduled_time) {
  if (!schedulable_callback_) {
    schedulable_callback_ = base::MakeRefCounted<SchedulableCallback>(
        task_runner_, callback_, use_metronome_, repeated_delay_);
  }
  schedulable_callback_->Schedule(scheduled_time);
}

// EXCLUSIVE_LOCKS_REQUIRED(lock_)
void WebRtcTimer::RescheduleCallback() {
  if (!schedulable_callback_)
    return;
  base::TimeTicks cancelled_scheduled_time =
      schedulable_callback_->Inactivate();
  schedulable_callback_ = nullptr;
  if (cancelled_scheduled_time == base::TimeTicks::Max())
    return;  // We don't have a scheduled time.
  ScheduleCallback(cancelled_scheduled_time);
}

void WebRtcTimer::MoveToNewTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  base::AutoLock auto_lock(lock_);
  DCHECK(task_runner);
  task_runner_ = std::move(task_runner);
  RescheduleCallback();
}

}  // namespace blink
