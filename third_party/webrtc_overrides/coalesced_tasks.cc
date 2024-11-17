// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/coalesced_tasks.h"

#include <vector>

namespace blink {

CoalescedTasks::UniqueTimeTicks::UniqueTimeTicks(base::TimeTicks time_ticks,
                                                 uint64_t unique_id)
    : time_ticks(std::move(time_ticks)), unique_id(unique_id) {}

bool CoalescedTasks::UniqueTimeTicks::operator<(
    const UniqueTimeTicks& other) const {
  if (time_ticks < other.time_ticks)
    return true;
  if (time_ticks == other.time_ticks)
    return unique_id < other.unique_id;
  return false;
}

bool CoalescedTasks::QueueDelayedTask(base::TimeTicks task_time,
                                      absl::AnyInvocable<void() &&> task,
                                      base::TimeTicks scheduled_time) {
  DCHECK_GE(scheduled_time, task_time);
  base::AutoLock auto_lock(lock_);
  bool is_new_schedule_time = scheduled_ticks_.insert(scheduled_time).second;
  delayed_tasks_.insert(std::make_pair(
      UniqueTimeTicks(task_time, next_unique_id_++), std::move(task)));
  return is_new_schedule_time;
}

CoalescedTasks::~CoalescedTasks() {
  DCHECK(delayed_tasks_.empty());
}

void CoalescedTasks::RunScheduledTasks(
    base::TimeTicks scheduled_time,
    PrepareRunTaskCallback prepare_run_task_callback,
    FinalizeRunTaskCallback finalize_run_task_callback) {
  std::vector<absl::AnyInvocable<void() &&>> ready_tasks;
  {
    base::AutoLock auto_lock(lock_);
    // `scheduled_time` is no longer scheduled.
    auto scheduled_ticks_it = scheduled_ticks_.find(scheduled_time);
    CHECK(scheduled_ticks_it != scheduled_ticks_.end());
    scheduled_ticks_.erase(scheduled_ticks_it);
    // Obtain ready tasks so that we can run them whilst not holding the lock.
    while (!delayed_tasks_.empty()) {
      // `delayed_tasks_` is ordered, so the first element is the earliest task.
      auto first_delayed_task_it = delayed_tasks_.begin();
      if (first_delayed_task_it->first.time_ticks > scheduled_time) {
        // The remaining tasks are not ready yet.
        break;
      }
      ready_tasks.push_back(std::move(first_delayed_task_it->second));
      delayed_tasks_.erase(first_delayed_task_it);
    }
  }
  // Run ready tasks.
  for (auto& ready_task : ready_tasks) {
    std::optional<base::TimeTicks> task_start_timestamp;
    if (prepare_run_task_callback) {
      task_start_timestamp = prepare_run_task_callback.Run();
    }

    std::move(ready_task)();

    if (finalize_run_task_callback) {
      finalize_run_task_callback.Run(std::move(task_start_timestamp));
    }
  }
}

void CoalescedTasks::Clear() {
  base::AutoLock auto_lock(lock_);
  scheduled_ticks_.clear();
  delayed_tasks_.clear();
}

}  // namespace blink
