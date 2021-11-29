// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_task_queue_factory.h"

#include <map>
#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace blink {

const base::Feature kWebRtcMetronomeTaskQueue{
    "WebRtcMetronomeTaskQueue", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<base::TimeDelta> kWebRtcMetronomeTaskQueueTick{
    &kWebRtcMetronomeTaskQueue, "tick",
    // 64 Hz default value for the WebRtcMetronomeTaskQueue experiment.
    base::Hertz(64)};

const base::FeatureParam<bool> kWebRtcMetronomeTaskQueueExcludePacer{
    &kWebRtcMetronomeTaskQueue, "exclude_pacer", /*default_value=*/true};

const base::FeatureParam<bool> kWebRtcMetronomeTaskQueueExcludeDecoders{
    &kWebRtcMetronomeTaskQueue, "exclude_decoders", /*default_value=*/true};

const base::FeatureParam<bool> kWebRtcMetronomeTaskQueueExcludeMisc{
    &kWebRtcMetronomeTaskQueue, "exclude_misc", /*default_value=*/false};

namespace {

class WebRtcMetronomeTaskQueue : public webrtc::TaskQueueBase {
 public:
  explicit WebRtcMetronomeTaskQueue(
      scoped_refptr<MetronomeSource> metronome_source);

  // webrtc::TaskQueueBase implementation.
  void Delete() override;
  void PostTask(std::unique_ptr<webrtc::QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<webrtc::QueuedTask> task,
                       uint32_t milliseconds) override;

 private:
  struct DelayedTaskInfo {
    DelayedTaskInfo(base::TimeTicks ready_time, uint64_t task_id);

    // Used for std::map<> ordering.
    bool operator<(const DelayedTaskInfo& other) const;

    base::TimeTicks ready_time;
    uint64_t task_id;
  };

  // Runs a single PostTask-task.
  void RunTask(std::unique_ptr<webrtc::QueuedTask> task);

  void UpdateWakeupTime() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Run all delayed tasks, in order, that are ready (target time <= now).
  void OnMetronomeTick();

  const scoped_refptr<MetronomeSource> metronome_source_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle_;
  base::Lock lock_;
  // The next delayed task gets assigned this ID which then increments. Used for
  // task execution ordering, see |delayed_tasks_| comment.
  uint64_t next_task_id_ GUARDED_BY(lock_) = 0;
  // The map's order ensures tasks are ordered by desired execution time. If two
  // tasks have the same |ready_time| then they are ordered by the ID, i.e. the
  // order they were posted.
  std::map<DelayedTaskInfo, std::unique_ptr<webrtc::QueuedTask>> delayed_tasks_
      GUARDED_BY(lock_);
};

WebRtcMetronomeTaskQueue::DelayedTaskInfo::DelayedTaskInfo(
    base::TimeTicks ready_time,
    uint64_t task_id)
    : ready_time(std::move(ready_time)), task_id(task_id) {}

bool WebRtcMetronomeTaskQueue::DelayedTaskInfo::operator<(
    const DelayedTaskInfo& other) const {
  if (ready_time < other.ready_time)
    return true;
  if (ready_time == other.ready_time)
    return task_id < other.task_id;
  return false;
}

WebRtcMetronomeTaskQueue::WebRtcMetronomeTaskQueue(
    scoped_refptr<MetronomeSource> metronome_source)
    : metronome_source_(std::move(metronome_source)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {
  listener_handle_ = metronome_source_->AddListener(
      task_runner_,
      base::BindRepeating(&WebRtcMetronomeTaskQueue::OnMetronomeTick,
                          base::Unretained(this)),
      base::TimeTicks::Max());
  DCHECK(listener_handle_);
}

void WebRtcMetronomeTaskQueue::Delete() {
  // Ensure OnMetronomeTick() will not be invoked again.
  metronome_source_->RemoveListener(listener_handle_);
  // Ensure there are no in-flight PostTask-tasks when deleting.
  base::WaitableEvent event;
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                   base::Unretained(&event)));
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

void WebRtcMetronomeTaskQueue::RunTask(
    std::unique_ptr<webrtc::QueuedTask> task) {
  CurrentTaskQueueSetter set_current(this);
  if (!task->Run()) {
    task.release();
  }
}

void WebRtcMetronomeTaskQueue::PostDelayedTask(
    std::unique_ptr<webrtc::QueuedTask> task,
    uint32_t milliseconds) {
  base::AutoLock auto_lock(lock_);
  delayed_tasks_.insert(std::make_pair(
      DelayedTaskInfo(base::TimeTicks::Now() + base::Milliseconds(milliseconds),
                      next_task_id_++),
      std::move(task)));
  UpdateWakeupTime();
}

// EXCLUSIVE_LOCKS_REQUIRED(lock_)
void WebRtcMetronomeTaskQueue::UpdateWakeupTime() {
  listener_handle_->SetWakeupTime(!delayed_tasks_.empty()
                                      ? delayed_tasks_.begin()->first.ready_time
                                      : base::TimeTicks::Max());
}

void WebRtcMetronomeTaskQueue::OnMetronomeTick() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  CurrentTaskQueueSetter set_current(this);
  while (true) {
    std::unique_ptr<webrtc::QueuedTask> task;
    {
      base::AutoLock auto_lock(lock_);
      if (delayed_tasks_.empty()) {
        // No more delayed tasks to run.
        UpdateWakeupTime();
        break;
      }
      auto it = delayed_tasks_.begin();
      if (it->first.ready_time > base::TimeTicks::Now()) {
        // The next delayed task is in the future.
        UpdateWakeupTime();
        break;
      }
      // Transfer ownership of the task and remove it from the queue.
      task = std::move(it->second);
      delayed_tasks_.erase(it);
    }
    // Run the task while not holding the lock. The task may manipulate the task
    // queue.
    if (!task->Run()) {
      task.release();
    }
  }
}

class WebrtcMetronomeTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  explicit WebrtcMetronomeTaskQueueFactory(
      scoped_refptr<MetronomeSource> metronome_source)
      : metronome_source_(std::move(metronome_source)),
        high_priority_task_queue_factory_(CreateWebRtcTaskQueueFactory()),
        exclude_pacer_(kWebRtcMetronomeTaskQueueExcludePacer.Get()),
        exclude_decoders_(kWebRtcMetronomeTaskQueueExcludeDecoders.Get()),
        exclude_misc_(kWebRtcMetronomeTaskQueueExcludeMisc.Get()) {}

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view name, Priority priority) const override {
    bool use_metronome;
    if (name.compare("TaskQueuePacedSender") == 0) {
      use_metronome = !exclude_pacer_;
    } else if (name.compare("DecodingQueue") == 0) {
      use_metronome = !exclude_decoders_;
    } else if (priority == webrtc::TaskQueueFactory::Priority::HIGH) {
      use_metronome = false;
    } else {
      use_metronome = !exclude_misc_;
    }
    if (!use_metronome) {
      return high_priority_task_queue_factory_->CreateTaskQueue(name, priority);
    }
    return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
        new WebRtcMetronomeTaskQueue(metronome_source_));
  }

 private:
  const scoped_refptr<MetronomeSource> metronome_source_;
  // An implementation of the task queue factory whose task queues do not run on
  // the metronome, i.e. at higher timer precision.
  const std::unique_ptr<webrtc::TaskQueueFactory>
      high_priority_task_queue_factory_;
  const bool exclude_pacer_;
  const bool exclude_decoders_;
  const bool exclude_misc_;
};

}  // namespace

}  // namespace blink

std::unique_ptr<webrtc::TaskQueueFactory> CreateWebRtcMetronomeTaskQueueFactory(
    scoped_refptr<blink::MetronomeSource> metronome_source) {
  return std::unique_ptr<webrtc::TaskQueueFactory>(
      new blink::WebrtcMetronomeTaskQueueFactory(std::move(metronome_source)));
}
