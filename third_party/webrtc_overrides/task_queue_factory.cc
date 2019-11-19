// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/task_queue_factory.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"

namespace {

class WebrtcTaskQueue final : public webrtc::TaskQueueBase {
 public:
  explicit WebrtcTaskQueue(const base::TaskTraits& traits)
      : task_runner_(base::CreateSequencedTaskRunner(traits)),
        is_active_(new base::RefCountedData<bool>(true)) {
    DCHECK(task_runner_);
  }

  void Delete() override;
  void PostTask(std::unique_ptr<webrtc::QueuedTask> task) override;
  void PostDelayedTask(std::unique_ptr<webrtc::QueuedTask> task,
                       uint32_t milliseconds) override;

 private:
  ~WebrtcTaskQueue() override = default;

  static void RunTask(WebrtcTaskQueue* task_queue,
                      scoped_refptr<base::RefCountedData<bool>> is_active,
                      std::unique_ptr<webrtc::QueuedTask> task);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Value of |is_active_| is checked and set on |task_runner_|.
  const scoped_refptr<base::RefCountedData<bool>> is_active_;
};

void Deactivate(scoped_refptr<base::RefCountedData<bool>> is_active,
                base::WaitableEvent* event) {
  is_active->data = false;
  event->Signal();
}

void WebrtcTaskQueue::Delete() {
  DCHECK(!IsCurrent());
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&Deactivate, is_active_, &event));
  event.Wait();
  delete this;
}

void WebrtcTaskQueue::RunTask(
    WebrtcTaskQueue* task_queue,
    scoped_refptr<base::RefCountedData<bool>> is_active,
    std::unique_ptr<webrtc::QueuedTask> task) {
  if (!is_active->data)
    return;

  CurrentTaskQueueSetter set_current(task_queue);
  webrtc::QueuedTask* task_ptr = task.release();
  if (task_ptr->Run()) {
    // Delete task_ptr before CurrentTaskQueueSetter clears state that this code
    // is running on the task queue.
    delete task_ptr;
  }
}

void WebrtcTaskQueue::PostTask(std::unique_ptr<webrtc::QueuedTask> task) {
  // Posted Task might outlive this, but access to this is guarded by
  // ref-counted |is_active_| flag.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcTaskQueue::RunTask, base::Unretained(this),
                     is_active_, std::move(task)));
}

void WebrtcTaskQueue::PostDelayedTask(std::unique_ptr<webrtc::QueuedTask> task,
                                      uint32_t milliseconds) {
  // Posted Task might outlive this, but access to this is guarded by
  // ref-counted |is_active_| flag.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebrtcTaskQueue::RunTask, base::Unretained(this),
                     is_active_, std::move(task)),
      base::TimeDelta::FromMilliseconds(milliseconds));
}

base::TaskTraits TaskQueuePriority2Traits(
    webrtc::TaskQueueFactory::Priority priority) {
  // The content/renderer/media/webrtc/rtc_video_encoder.* code
  // employs a PostTask/Wait pattern that uses TQ in a way that makes it
  // blocking and synchronous, which is why we allow WithBaseSyncPrimitives()
  // for OS_ANDROID.
  switch (priority) {
    case webrtc::TaskQueueFactory::Priority::HIGH:
#if defined(OS_ANDROID)
      return {base::ThreadPool(), base::WithBaseSyncPrimitives(),
              base::TaskPriority::HIGHEST};
#else
      return {base::ThreadPool(), base::TaskPriority::HIGHEST};
#endif
      break;
    case webrtc::TaskQueueFactory::Priority::LOW:
      return {base::ThreadPool(), base::MayBlock(),
              base::TaskPriority::BEST_EFFORT};
    case webrtc::TaskQueueFactory::Priority::NORMAL:
    default:
#if defined(OS_ANDROID)
      return {base::ThreadPool(), base::WithBaseSyncPrimitives()};
#else
      return {base::ThreadPool()};
#endif
  }
}

class WebrtcTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  WebrtcTaskQueueFactory() = default;

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view /*name*/,
                  Priority priority) const override {
    return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
        new WebrtcTaskQueue(TaskQueuePriority2Traits(priority)));
  }
};

}  // namespace

std::unique_ptr<webrtc::TaskQueueFactory> CreateWebRtcTaskQueueFactory() {
  return std::make_unique<WebrtcTaskQueueFactory>();
}

std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
CreateWebRtcTaskQueue(webrtc::TaskQueueFactory::Priority priority) {
  return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
      new WebrtcTaskQueue(TaskQueuePriority2Traits(priority)));
}
