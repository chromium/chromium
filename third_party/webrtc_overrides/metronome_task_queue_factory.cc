// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_task_queue_factory.h"

#include <map>
#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"
#include "third_party/webrtc_overrides/coalesced_tasks.h"
#include "third_party/webrtc_overrides/metronome_source.h"

namespace blink {

const base::Feature kWebRtcMetronomeTaskQueue{
    "WebRtcMetronomeTaskQueue", base::FEATURE_DISABLED_BY_DEFAULT};

class WebRtcMetronomeTaskQueue : public webrtc::TaskQueueBase {
 public:
  WebRtcMetronomeTaskQueue();

  // webrtc::TaskQueueBase implementation.
  void Delete() override;
  void PostTask(std::unique_ptr<webrtc::QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<webrtc::QueuedTask> task,
                       uint32_t milliseconds) override;
  void PostDelayedHighPrecisionTask(std::unique_ptr<webrtc::QueuedTask> task,
                                    uint32_t milliseconds) override;

 private:
  // Runs a single PostTask-task.
  static void MaybeRunTask(WebRtcMetronomeTaskQueue* metronome_task_queue,
                           scoped_refptr<base::RefCountedData<bool>> is_active,
                           std::unique_ptr<webrtc::QueuedTask> task);
  void RunTask(std::unique_ptr<webrtc::QueuedTask> task);
  // Runs all ready PostDelayedTask-tasks that have been scheduled to run at
  // |scheduled_time_now|.
  static void MaybeRunCoalescedTasks(
      WebRtcMetronomeTaskQueue* metronome_task_queue,
      scoped_refptr<base::RefCountedData<bool>> is_active,
      base::TimeTicks scheduled_time_now);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Value of |is_active_| is checked and set on |task_runner_|.
  const scoped_refptr<base::RefCountedData<bool>> is_active_;
  // Low precision tasks are coalesced onto metronome ticks and stored in
  // |coalesced_tasks_| until they are ready to run.
  CoalescedTasks coalesced_tasks_;
};

WebRtcMetronomeTaskQueue::WebRtcMetronomeTaskQueue()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
      is_active_(new base::RefCountedData<bool>(true)) {}

void Deactivate(scoped_refptr<base::RefCountedData<bool>> is_active,
                CoalescedTasks* coalesced_tasks,
                base::WaitableEvent* event) {
  is_active->data = false;
  coalesced_tasks->Clear();
  event->Signal();
}

void WebRtcMetronomeTaskQueue::Delete() {
  // Ensure there are no in-flight PostTask-tasks when deleting.
  base::WaitableEvent event;
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&Deactivate, is_active_,
                                                   &coalesced_tasks_, &event));
  event.Wait();
  delete this;
}

void WebRtcMetronomeTaskQueue::PostTask(
    std::unique_ptr<webrtc::QueuedTask> task) {
  // Delete() ensures there are no in-flight tasks at destruction, so passing an
  // unretained pointer to |this| is safe.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcMetronomeTaskQueue::RunTask,
                                base::Unretained(this), std::move(task)));
}

// static
void WebRtcMetronomeTaskQueue::MaybeRunTask(
    WebRtcMetronomeTaskQueue* metronome_task_queue,
    scoped_refptr<base::RefCountedData<bool>> is_active,
    std::unique_ptr<webrtc::QueuedTask> task) {
  if (!is_active->data)
    return;
  metronome_task_queue->RunTask(std::move(task));
}

void WebRtcMetronomeTaskQueue::RunTask(
    std::unique_ptr<webrtc::QueuedTask> task) {
  CurrentTaskQueueSetter set_current(this);
  if (!task->Run()) {
    task.release();
  }
}

// static
void WebRtcMetronomeTaskQueue::MaybeRunCoalescedTasks(
    WebRtcMetronomeTaskQueue* metronome_task_queue,
    scoped_refptr<base::RefCountedData<bool>> is_active,
    base::TimeTicks scheduled_time_now) {
  if (!is_active->data)
    return;
  CurrentTaskQueueSetter set_current(metronome_task_queue);
  metronome_task_queue->coalesced_tasks_.RunScheduledTasks(scheduled_time_now);
}

void WebRtcMetronomeTaskQueue::PostDelayedTask(
    std::unique_ptr<webrtc::QueuedTask> task,
    uint32_t milliseconds) {
  base::TimeTicks target_time =
      base::TimeTicks::Now() + base::Milliseconds(milliseconds);
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
        base::BindOnce(&WebRtcMetronomeTaskQueue::MaybeRunCoalescedTasks,
                       base::Unretained(this), is_active_, snapped_target_time),
        snapped_target_time, base::subtle::DelayPolicy::kPrecise);
  }
}

void WebRtcMetronomeTaskQueue::PostDelayedHighPrecisionTask(
    std::unique_ptr<webrtc::QueuedTask> task,
    uint32_t milliseconds) {
  base::TimeTicks target_time =
      base::TimeTicks::Now() + base::Milliseconds(milliseconds);
  // The posted task might outlive |this|, but access to |this| is guarded by
  // the ref-counted |is_active_| flag.
  task_runner_->PostDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&WebRtcMetronomeTaskQueue::MaybeRunTask,
                     base::Unretained(this), is_active_, std::move(task)),
      target_time, base::subtle::DelayPolicy::kPrecise);
}

namespace {

class WebrtcMetronomeTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view name, Priority priority) const override {
    return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
        new WebRtcMetronomeTaskQueue());
  }
};

}  // namespace

}  // namespace blink

std::unique_ptr<webrtc::TaskQueueFactory>
CreateWebRtcMetronomeTaskQueueFactory() {
  return std::unique_ptr<webrtc::TaskQueueFactory>(
      new blink::WebrtcMetronomeTaskQueueFactory());
}
