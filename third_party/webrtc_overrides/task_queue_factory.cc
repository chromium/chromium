// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/task_queue_factory.h"

#include <map>
#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/functional/any_invocable.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"
#include "third_party/webrtc/api/units/time_delta.h"
#include "third_party/webrtc_overrides/coalesced_tasks.h"
#include "third_party/webrtc_overrides/metronome_source.h"

namespace blink {

class WebRtcTaskQueue : public webrtc::TaskQueueBase {
 public:
  explicit WebRtcTaskQueue(base::TaskTraits traits);

  // webrtc::TaskQueueBase implementation.
  void Delete() override;
  void PostTask(absl::AnyInvocable<void() &&> task) override;
  void PostDelayedTask(absl::AnyInvocable<void() &&> task,
                       webrtc::TimeDelta delay) override;
  void PostDelayedHighPrecisionTask(absl::AnyInvocable<void() &&> task,
                                    webrtc::TimeDelta delay) override;

 private:
  // Runs a single PostTask-task.
  static void MaybeRunTask(WebRtcTaskQueue* task_queue,
                           scoped_refptr<base::RefCountedData<bool>> is_active,
                           absl::AnyInvocable<void() &&> task);
  void RunTask(absl::AnyInvocable<void() &&> task);
  // Runs all ready PostDelayedTask-tasks that have been scheduled to run at
  // |scheduled_time_now|.
  static void MaybeRunCoalescedTasks(
      WebRtcTaskQueue* task_queue,
      scoped_refptr<base::RefCountedData<bool>> is_active,
      base::TimeTicks scheduled_time_now);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Value of |is_active_| is checked and set on |task_runner_|.
  const scoped_refptr<base::RefCountedData<bool>> is_active_;
  // Low precision tasks are coalesced onto metronome ticks and stored in
  // |coalesced_tasks_| until they are ready to run.
  CoalescedTasks coalesced_tasks_;
};

WebRtcTaskQueue::WebRtcTaskQueue(base::TaskTraits traits)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(std::move(traits))),
      is_active_(new base::RefCountedData<bool>(true)) {}

void Deactivate(scoped_refptr<base::RefCountedData<bool>> is_active,
                CoalescedTasks* coalesced_tasks,
                base::WaitableEvent* event) {
  is_active->data = false;
  coalesced_tasks->Clear();
  event->Signal();
}

void WebRtcTaskQueue::Delete() {
  // Ensure there are no in-flight PostTask-tasks when deleting.
  base::WaitableEvent event;
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&Deactivate, is_active_,
                                                   &coalesced_tasks_, &event));
  event.Wait();
  delete this;
}

void WebRtcTaskQueue::PostTask(absl::AnyInvocable<void() &&> task) {
  // Delete() ensures there are no in-flight tasks at destruction, so passing an
  // unretained pointer to |this| is safe.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcTaskQueue::RunTask,
                                base::Unretained(this), std::move(task)));
}

// static
void WebRtcTaskQueue::MaybeRunTask(
    WebRtcTaskQueue* task_queue,
    scoped_refptr<base::RefCountedData<bool>> is_active,
    absl::AnyInvocable<void() &&> task) {
  if (!is_active->data)
    return;
  task_queue->RunTask(std::move(task));
}

void WebRtcTaskQueue::RunTask(absl::AnyInvocable<void() &&> task) {
  CurrentTaskQueueSetter set_current(this);
  std::move(task)();
}

// static
void WebRtcTaskQueue::MaybeRunCoalescedTasks(
    WebRtcTaskQueue* task_queue,
    scoped_refptr<base::RefCountedData<bool>> is_active,
    base::TimeTicks scheduled_time_now) {
  if (!is_active->data)
    return;
  CurrentTaskQueueSetter set_current(task_queue);
  task_queue->coalesced_tasks_.RunScheduledTasks(scheduled_time_now);
}

void WebRtcTaskQueue::PostDelayedTask(absl::AnyInvocable<void() &&> task,
                                      webrtc::TimeDelta delay) {
  base::TimeTicks target_time =
      base::TimeTicks::Now() + base::Microseconds(delay.us());
  base::TimeTicks snapped_target_time =
      MetronomeSource::TimeSnappedToNextTick(target_time);
  // Queue to run the delayed task at |snapped_target_time|. If the snapped time
  // has not been scheduled before, schedule it with PostDelayedTaskAt().
  if (coalesced_tasks_.QueueDelayedTask(target_time, std::move(task),
                                        snapped_target_time)) {
    // The posted task might outlive |this|, but access to |this| is guarded by
    // the ref-counted |is_active_| flag.
    task_runner_->PostDelayedTaskAt(
        base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
        base::BindOnce(&WebRtcTaskQueue::MaybeRunCoalescedTasks,
                       base::Unretained(this), is_active_, snapped_target_time),
        snapped_target_time, base::subtle::DelayPolicy::kPrecise);
  }
}

void WebRtcTaskQueue::PostDelayedHighPrecisionTask(
    absl::AnyInvocable<void() &&> task,
    webrtc::TimeDelta delay) {
  base::TimeTicks target_time =
      base::TimeTicks::Now() + base::Microseconds(delay.us());
  // The posted task might outlive |this|, but access to |this| is guarded by
  // the ref-counted |is_active_| flag.
  task_runner_->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&WebRtcTaskQueue::MaybeRunTask, base::Unretained(this),
                     is_active_, std::move(task)),
      target_time, base::subtle::DelayPolicy::kPrecise);
}

namespace {

base::TaskTraits TaskQueuePriority2Traits(
    webrtc::TaskQueueFactory::Priority priority) {
  // The content/renderer/media/webrtc/rtc_video_encoder.* code
  // employs a PostTask/Wait pattern that uses TQ in a way that makes it
  // blocking and synchronous, which is why we allow WithBaseSyncPrimitives()
  // for OS_ANDROID.
  switch (priority) {
    case webrtc::TaskQueueFactory::Priority::HIGH:
#if defined(OS_ANDROID)
      return {base::WithBaseSyncPrimitives(), base::TaskPriority::HIGHEST};
#else
      return {base::TaskPriority::HIGHEST};
#endif
    case webrtc::TaskQueueFactory::Priority::LOW:
      return {base::MayBlock(), base::TaskPriority::BEST_EFFORT};
    case webrtc::TaskQueueFactory::Priority::NORMAL:
    default:
#if defined(OS_ANDROID)
      return {base::WithBaseSyncPrimitives()};
#else
      return {};
#endif
  }
}

class WebrtcTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view name, Priority priority) const override {
    return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
        new WebRtcTaskQueue(TaskQueuePriority2Traits(priority)));
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
  return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
      new blink::WebRtcTaskQueue(blink::TaskQueuePriority2Traits(priority)));
}
