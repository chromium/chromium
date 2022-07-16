// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_source.h"

#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace blink {

MetronomeSource::ListenerHandle::ListenerHandle(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void()> callback,
    base::TimeTicks wakeup_time)
    : task_runner_(std::move(task_runner)),
      callback_(std::move(callback)),
      wakeup_time_(std::move(wakeup_time)) {}

MetronomeSource::ListenerHandle::~ListenerHandle() {
  DCHECK(!is_active_);
}

void MetronomeSource::ListenerHandle::SetWakeupTime(
    base::TimeTicks wakeup_time) {
  base::AutoLock auto_lock(wakeup_time_lock_);
  wakeup_time_ = std::move(wakeup_time);
}

void MetronomeSource::ListenerHandle::OnMetronomeTick() {
  base::TimeTicks now = base::TimeTicks::Now();
  {
    base::AutoLock auto_lock(wakeup_time_lock_);
    if (now < wakeup_time_) {
      // The listener is still sleeping.
      return;
    }
    if (!wakeup_time_.is_min()) {
      // A wakeup time had been specified (set to anything other than "min").
      // Reset the wakeup time to "infinity", meaning SetWakeupTime() has to be
      // called again in order to wake up again.
      wakeup_time_ = base::TimeTicks::Max();
    }
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MetronomeSource::ListenerHandle::MaybeRunCallbackOnTaskRunner,
          this));
}

void MetronomeSource::ListenerHandle::MaybeRunCallbackOnTaskRunner() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(is_active_lock_);
  if (!is_active_)
    return;
  callback_.Run();
}

void MetronomeSource::ListenerHandle::Inactivate() {
  base::AutoLock auto_lock(is_active_lock_);
  is_active_ = false;
}

MetronomeSource::MetronomeSource(base::TimeDelta metronome_tick)
    : metronome_task_runner_(
          // Single thread is used to allow tracing the metronome's ticks to
          // consistently happen on the same thread. HIGHEST priority is used to
          // reduce risk of jitter.
          base::ThreadPool::CreateSingleThreadTaskRunner(
              {base::TaskPriority::HIGHEST})),
      metronome_tick_(std::move(metronome_tick)) {}

MetronomeSource::~MetronomeSource() {
  DCHECK(!is_active_);
  DCHECK(!timer_);
  DCHECK(listeners_.empty());
}

bool MetronomeSource::IsActive() {
  base::AutoLock auto_lock(lock_);
  return is_active_;
}

// EXCLUSIVE_LOCKS_REQUIRED(lock_)
void MetronomeSource::StartTimer() {
  DCHECK(!is_active_);
  is_active_ = true;
  // Ref-counting ensures |this| stays alive until the timer has started.
  metronome_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<MetronomeSource> metronome_source) {
            base::AutoLock auto_lock(metronome_source->lock_);
            DCHECK(!metronome_source->timer_);
            metronome_source->timer_ = std::make_unique<base::RepeatingTimer>();
            metronome_source->timer_->Start(
                FROM_HERE, metronome_source->metronome_tick_,
                metronome_source.get(), &MetronomeSource::OnMetronomeTick);
          },
          scoped_refptr<MetronomeSource>(this)));
}

// EXCLUSIVE_LOCKS_REQUIRED(lock_)
void MetronomeSource::StopTimer() {
  DCHECK(is_active_);
  is_active_ = false;
  // Ref-counting ensures |this| stays alive until the timer has stopped.
  metronome_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](scoped_refptr<MetronomeSource> metronome_source) {
                       base::AutoLock auto_lock(metronome_source->lock_);
                       DCHECK(metronome_source->timer_);
                       metronome_source->timer_->Stop();
                       metronome_source->timer_.reset();
                     },
                     scoped_refptr<MetronomeSource>(this)));
}

scoped_refptr<MetronomeSource::ListenerHandle> MetronomeSource::AddListener(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void()> callback,
    base::TimeTicks wakeup_time) {
  base::AutoLock auto_lock(lock_);
  scoped_refptr<ListenerHandle> listener_handle =
      base::MakeRefCounted<ListenerHandle>(
          std::move(task_runner), std::move(callback), std::move(wakeup_time));
  listeners_.insert(listener_handle);
  if (listeners_.size() == 1u) {
    // The first listener was just added.
    StartTimer();
  }
  return listener_handle;
}

void MetronomeSource::RemoveListener(
    scoped_refptr<ListenerHandle> listener_handle) {
  {
    base::AutoLock auto_lock(lock_);
    size_t elements_removed = listeners_.erase(listener_handle);
    if (!elements_removed) {
      return;
    }
    if (listeners_.empty()) {
      // The last listener was just removed.
      StopTimer();
    }
  }
  // Inactivate the listener whilst not holding the |lock_| to avoid deadlocks
  // (e.g. a listener callback attempting to add another listener).
  listener_handle->Inactivate();
}

void MetronomeSource::OnMetronomeTick() {
  TRACE_EVENT_INSTANT0("webrtc", "MetronomeSource::OnMetronomeTick",
                       TRACE_EVENT_SCOPE_PROCESS);
  DCHECK(metronome_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock auto_lock(lock_);
  for (auto& listener : listeners_) {
    listener->OnMetronomeTick();
  }
}

}  // namespace blink
