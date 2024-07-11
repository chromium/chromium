// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/task_queue_factory.h"

#include <map>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/functional/any_invocable.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"
#include "third_party/webrtc/api/units/time_delta.h"
#include "third_party/webrtc_overrides/api/location.h"
#include "third_party/webrtc_overrides/coalesced_tasks.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

namespace blink {

class WebRtcTaskQueue : public base::RefCountedThreadSafe<WebRtcTaskQueue>,
                        public webrtc::TaskQueueBase {
 public:
  explicit WebRtcTaskQueue(base::TaskTraits traits);

  // webrtc::TaskQueueBase implementation.
  void Delete() override;
  void PostTaskImpl(absl::AnyInvocable<void() &&> task,
                    const PostTaskTraits& traits,
                    const webrtc::Location& location) override;
  void PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                           webrtc::TimeDelta delay,
                           const PostDelayedTaskTraits& traits,
                           const webrtc::Location& location) override;

 private:
  friend class base::RefCountedThreadSafe<WebRtcTaskQueue>;
  ~WebRtcTaskQueue() override = default;

  // Runs a single PostTask-task.
  void RunTask(absl::AnyInvocable<void() &&> task);
  // Runs all ready PostDelayedTask-tasks that have been scheduled to run at
  // |scheduled_time_now|.
  void MaybeRunCoalescedTasks(base::TimeTicks scheduled_time_now);
  // Runs a specific high precision task.
  void RunHighPrecisionTask(int id);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Kept during task execution to guarantee Delete semantics. Only contended
  // in case both Delete and a task runs concurrently. All tasks run and get
  // destroyed serially.
  base::Lock alive_lock_;
  // Turns to false in Delete.
  bool alive_ GUARDED_BY(alive_lock_) = true;

  // Low precision tasks are coalesced onto metronome ticks and stored in
  // |coalesced_tasks_| until they are ready to run.
  CoalescedTasks coalesced_tasks_;
};

WebRtcTaskQueue::WebRtcTaskQueue(base::TaskTraits traits)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(std::move(traits))) {
  // This reference is eventually released by Delete being called.
  AddRef();
}

void WebRtcTaskQueue::Delete() {
  {
    // Ensure no more tasks are going to be run.
    base::AutoLock lock(alive_lock_);
    alive_ = false;

    // Pretend to be the current task queue and clear the other tasks. This
    // works because we're always deleting or running tasks under the
    // `alive_lock_`, which we keep here.
    CurrentTaskQueueSetter setter(this);
    coalesced_tasks_.Clear();
  }
  // Drop the first reference we took when creating the task queue. We are
  // deleted when all closures posted to the task runner has run, or right here
  // in Release().
  Release();
}

void WebRtcTaskQueue::RunTask(absl::AnyInvocable<void() &&> task) {
  CurrentTaskQueueSetter set_current(this);
  base::AutoLock lock(alive_lock_);
  if (alive_)
    std::move(task)();
  // Ensure task is destroyed before `set_current` goes out of scope.
  task = nullptr;
}

void WebRtcTaskQueue::PostTaskImpl(absl::AnyInvocable<void() &&> task,
                                   const PostTaskTraits& traits,
                                   const webrtc::Location& location) {
  task_runner_->PostTask(
      location, base::BindOnce(&WebRtcTaskQueue::RunTask,
                               base::RetainedRef(this), std::move(task)));
}

void WebRtcTaskQueue::MaybeRunCoalescedTasks(
    base::TimeTicks scheduled_time_now) {
  base::AutoLock lock(alive_lock_);
  if (alive_) {
    CurrentTaskQueueSetter set_current(this);
    coalesced_tasks_.RunScheduledTasks(scheduled_time_now);
  }
}

void WebRtcTaskQueue::PostDelayedTaskImpl(absl::AnyInvocable<void() &&> task,
                                          webrtc::TimeDelta delay,
                                          const PostDelayedTaskTraits& traits,
                                          const webrtc::Location& location) {
  const base::TimeTicks target_time =
      base::TimeTicks::Now() + base::Microseconds(delay.us());
  const base::TimeTicks snapped_target_time =
      TimerBasedTickProvider::TimeSnappedToNextTick(
          target_time, TimerBasedTickProvider::kDefaultPeriod);
  if (!traits.high_precision &&
      coalesced_tasks_.QueueDelayedTask(target_time, std::move(task),
                                        snapped_target_time)) {
    task_runner_->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), location,
        base::BindOnce(&WebRtcTaskQueue::MaybeRunCoalescedTasks,
                       base::RetainedRef(this), snapped_target_time),
        snapped_target_time, base::subtle::DelayPolicy::kPrecise);
  } else if (traits.high_precision) {
    task_runner_->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), location,
        base::BindOnce(&WebRtcTaskQueue::RunTask, base::RetainedRef(this),
                       std::move(task)),
        target_time, base::subtle::DelayPolicy::kPrecise);
  }
}

namespace {

base::TaskTraits TaskQueuePriority2Traits(
    webrtc::TaskQueueFactory::Priority priority) {
  // The content/renderer/media/webrtc/rtc_video_encoder.* code
  // employs a PostTask/Wait pattern that uses TQ in a way that makes it
  // blocking and synchronous, which is why we allow WithBaseSyncPrimitives()
  // for OS_ANDROID.
  // The libvpx threading adapters also need to wait for an event.
  switch (priority) {
    case webrtc::TaskQueueFactory::Priority::HIGH:
#if defined(OS_ANDROID)
      return {base::MayBlock(), base::WithBaseSyncPrimitives(),
              base::TaskPriority::HIGHEST};
#else
      return {base::MayBlock(), base::TaskPriority::HIGHEST};
#endif
    case webrtc::TaskQueueFactory::Priority::LOW:
      return {base::MayBlock(), base::TaskPriority::BEST_EFFORT};
    case webrtc::TaskQueueFactory::Priority::NORMAL:
    default:
#if defined(OS_ANDROID)
      return {base::MayBlock(), base::WithBaseSyncPrimitives()};
#else
      // On Windows, software encoders need to map HW frames which requires
      // blocking calls.
      // The libvpx threading adapters also need to wait for an event.
      return {base::MayBlock()};
#endif
  }
}

std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
CreateTaskQueueHelper(webrtc::TaskQueueFactory::Priority priority) {
  return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
      new WebRtcTaskQueue(TaskQueuePriority2Traits(priority)));
}

class WebrtcTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(std::string_view name, Priority priority) const override {
    return CreateTaskQueueHelper(priority);
  }
};

}  // namespace

}  // namespace blink

std::unique_ptr<webrtc::TaskQueueFactory> CreateWebRtcTaskQueueFactory() {
  return std::unique_ptr<webrtc::TaskQueueFactory>(
      new blink::WebrtcTaskQueueFactory());
}

std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
CreateWebRtcTaskQueue(webrtc::TaskQueueFactory::Priority priority) {
  return blink::CreateTaskQueueHelper(priority);
}
