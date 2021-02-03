// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rendering_pipeline.h"

#include "base/task/current_thread.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/rendering_stage_scheduler.h"

namespace gfx {
namespace {

class ThreadSafeTimeObserver : public base::sequence_manager::TaskTimeObserver {
 public:
  explicit ThreadSafeTimeObserver(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(std::move(task_runner)) {}
  ~ThreadSafeTimeObserver() override {
    // If the observer is being used on the target thread, unregister now. If it
    // was being used on a different thread, then the target thread should have
    // been torn down already.
    SetEnabled(false);
  }

  ThreadSafeTimeObserver(const ThreadSafeTimeObserver&) = delete;
  ThreadSafeTimeObserver& operator=(const ThreadSafeTimeObserver&) = delete;

  void SetEnabled(bool enabled) {
    {
      base::AutoLock hold(time_lock_);
      if (enabled_ == enabled)
        return;
      enabled_ = enabled;
    }

    if (!task_runner_)
      return;

    if (task_runner_->BelongsToCurrentThread()) {
      UpdateOnTargetThread(enabled);
      return;
    }

    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ThreadSafeTimeObserver::UpdateOnTargetThread,
                                  base::Unretained(this), enabled));
  }

  base::TimeDelta GetAndResetTimeSinceLastFrame() {
    base::AutoLock hold(time_lock_);

    if (!start_time_active_task_.is_null()) {
      auto now = base::TimeTicks::Now();
      time_since_last_frame_ += now - start_time_active_task_;
      start_time_active_task_ = now;
    }

    auto result = time_since_last_frame_;
    time_since_last_frame_ = base::TimeDelta();
    return result;
  }

  // TaskTimeObserver impl.
  void WillProcessTask(base::TimeTicks start_time) override {
    base::AutoLock hold(time_lock_);
    if (!enabled_)
      return;

    DCHECK(start_time_active_task_.is_null());
    start_time_active_task_ = start_time;
  }

  void DidProcessTask(base::TimeTicks start_time,
                      base::TimeTicks end_time) override {
    base::AutoLock hold(time_lock_);
    if (!enabled_) {
      start_time_active_task_ = base::TimeTicks();
      return;
    }

    // This should be null for the task which adds this object to the observer
    // list.
    if (start_time_active_task_.is_null())
      return;

    if (start_time_active_task_ <= end_time) {
      time_since_last_frame_ += (end_time - start_time_active_task_);
    } else {
      // This could happen if |GetAndResetTimeSinceLastFrame| is called on a
      // different thread and the observed thread had to wait to acquire the
      // lock to call DidProcessTask. Assume the time for this task is already
      // recorded in |GetAndResetTimeSinceLastFrame|.
      DCHECK_NE(start_time_active_task_, start_time);
    }

    start_time_active_task_ = base::TimeTicks();
  }

 private:
  void UpdateOnTargetThread(bool enabled) {
    if (enabled) {
      base::CurrentThread::Get().AddTaskTimeObserver(this);

      base::AutoLock hold(time_lock_);
      start_time_active_task_ = base::TimeTicks();
      time_since_last_frame_ = base::TimeDelta();
    } else {
      base::CurrentThread::Get().RemoveTaskTimeObserver(this);
    }
  }

  // Accessed only on the calling thread. The caller ensures no concurrent
  // access.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Accessed on calling and target thread.
  base::Lock time_lock_;
  bool enabled_ GUARDED_BY(time_lock_) = false;
  base::TimeTicks start_time_active_task_ GUARDED_BY(time_lock_);
  base::TimeDelta time_since_last_frame_ GUARDED_BY(time_lock_);
};

}  // namespace

class RenderingPipelineImpl final : public RenderingPipeline {
 public:
  explicit RenderingPipelineImpl(const char* pipeline_type)
      : pipeline_type_(pipeline_type) {
    RenderingStageScheduler::EnsureInitialized();
    DETACH_FROM_THREAD(bound_thread_);
  }
  ~RenderingPipelineImpl() override { TearDown(); }

  RenderingPipelineImpl(const RenderingPipelineImpl&) = delete;
  RenderingPipelineImpl& operator=(const RenderingPipelineImpl&) = delete;

  void SetTargetDuration(base::TimeDelta target_duration) override {
    DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
    DCHECK(!target_duration.is_zero());

    if (target_duration_ == target_duration)
      return;

    target_duration_ = target_duration;
    if (should_use_scheduler())
      SetUp();
  }

  void AddSequenceManagerThread(
      base::PlatformThreadId thread_id,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    base::AutoLock lock(lock_);
    DCHECK(time_observers_.find(thread_id) == time_observers_.end());
    time_observers_[thread_id] =
        std::make_unique<ThreadSafeTimeObserver>(task_runner);
    if (scheduler_)
      CreateSchedulerAndEnableWithLockAcquired();
  }

  base::sequence_manager::TaskTimeObserver* AddSimpleThread(
      base::PlatformThreadId thread_id) override {
    base::AutoLock lock(lock_);
    DCHECK(time_observers_.find(thread_id) == time_observers_.end());
    time_observers_[thread_id] =
        std::make_unique<ThreadSafeTimeObserver>(nullptr);

    if (scheduler_)
      CreateSchedulerAndEnableWithLockAcquired();
    return time_observers_[thread_id].get();
  }

  void NotifyFrameFinished() override {
    DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

    base::AutoLock lock(lock_);
    if (!scheduler_)
      return;

    // TODO(crbug.com/1157620): This can be optimized to exclude tasks which can
    // be paused during rendering. The best use-case is idle tasks on the
    // renderer main thread. If all non-optional work is close to the frame
    // budget then the scheduler dynamically adjusts to pause work like idle
    // tasks.
    base::TimeDelta total_time;
    for (auto& it : time_observers_) {
      total_time += it.second->GetAndResetTimeSinceLastFrame();
    }
    scheduler_->ReportCpuCompletionTime(total_time + gpu_latency_);
  }

  void SetGpuLatency(base::TimeDelta gpu_latency) override {
    DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
    gpu_latency_ = gpu_latency;
  }

  void UpdateActiveCount(bool active) override {
    DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);

    if (active) {
      active_count_++;
    } else {
      DCHECK_GT(active_count_, 0);
      active_count_--;
    }

    if (should_use_scheduler()) {
      SetUp();
    } else {
      TearDown();
    }
  }

 private:
  bool should_use_scheduler() const {
    // TODO(crbug.com/1157620) : Figure out what we should be doing if multiple
    // independent pipelines of a type are running simultaneously. The common
    // use-case for this in practice would be multi-window. The tabs could be
    // hosted in the same renderer process and each window is composited
    // independently by the GPU process.
    return active_count_ == 1 && !target_duration_.is_zero();
  }

  void SetUp() {
    base::AutoLock lock(lock_);
    CreateSchedulerAndEnableWithLockAcquired();
  }

  void CreateSchedulerAndEnableWithLockAcquired() {
    lock_.AssertAcquired();
    scheduler_.reset();

    std::vector<base::PlatformThreadId> platform_threads;
    for (auto& it : time_observers_) {
      platform_threads.push_back(it.first);
      it.second->SetEnabled(true);
    }

    scheduler_ = RenderingStageScheduler::CreateAdpf(
        pipeline_type_, std::move(platform_threads), target_duration_);
  }

  void TearDown() {
    base::AutoLock lock(lock_);
    for (auto& it : time_observers_)
      it.second->SetEnabled(false);
    scheduler_.reset();
  }

  THREAD_CHECKER(bound_thread_);

  base::Lock lock_;
  base::flat_map<base::PlatformThreadId,
                 std::unique_ptr<ThreadSafeTimeObserver>>
      time_observers_ GUARDED_BY(lock_);
  std::unique_ptr<RenderingStageScheduler> scheduler_ GUARDED_BY(lock_);

  // Pipeline name, for tracing and metrics.
  const char* pipeline_type_;

  // The number of currently active pipelines of this type.
  int active_count_ = 0;

  // The target time for this rendering stage for a frame.
  base::TimeDelta target_duration_;

  base::TimeDelta gpu_latency_;
};

RenderingPipeline::ScopedPipelineActive::ScopedPipelineActive(
    RenderingPipeline* pipeline)
    : pipeline_(pipeline) {
  pipeline_->UpdateActiveCount(true);
}

RenderingPipeline::ScopedPipelineActive::~ScopedPipelineActive() {
  pipeline_->UpdateActiveCount(false);
}

std::unique_ptr<RenderingPipeline> RenderingPipeline::CreateRendererMain() {
  static constexpr char kRendererMain[] = "RendererMain";
  return std::make_unique<RenderingPipelineImpl>(kRendererMain);
}

std::unique_ptr<RenderingPipeline>
RenderingPipeline::CreateRendererCompositor() {
  static constexpr char kRendererCompositor[] = "RendererCompositor";
  return std::make_unique<RenderingPipelineImpl>(kRendererCompositor);
}

std::unique_ptr<RenderingPipeline> RenderingPipeline::CreateGpu() {
  static constexpr char kGpu[] = "Gpu";
  return std::make_unique<RenderingPipelineImpl>(kGpu);
}

}  // namespace gfx
