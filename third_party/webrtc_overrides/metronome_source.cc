// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_source.h"
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/webrtc/api/metronome/metronome.h"
#include "third_party/webrtc/rtc_base/task_utils/to_queued_task.h"
#include "third_party/webrtc_overrides/task_queue_factory.h"

namespace blink {

namespace {

// Wraps a webrtc::Metronome::TickListener to ensure that OnTick is not called
// after it is removed from the WebRtcMetronomeAdapter, using the Deactive
// method.
class WebRtcMetronomeListenerWrapper
    : public base::RefCountedThreadSafe<WebRtcMetronomeListenerWrapper> {
 public:
  explicit WebRtcMetronomeListenerWrapper(
      webrtc::Metronome::TickListener* listener)
      : listener_(listener) {}

  void Deactivate() {
    base::AutoLock auto_lock(lock_);
    active_ = false;
  }

  webrtc::Metronome::TickListener* listener() { return listener_; }

  void OnTick() {
    base::AutoLock auto_lock(lock_);
    if (!active_)
      return;
    listener_->OnTick();
  }

 private:
  friend class base::RefCountedThreadSafe<WebRtcMetronomeListenerWrapper>;
  ~WebRtcMetronomeListenerWrapper() = default;

  webrtc::Metronome::TickListener* const listener_;

  base::Lock lock_;
  bool active_ GUARDED_BY(lock_) = true;
};

class WebRtcMetronomeAdapter : public webrtc::Metronome {
 public:
  explicit WebRtcMetronomeAdapter(
      scoped_refptr<MetronomeSource> metronome_source)
      : metronome_source_(std::move(metronome_source)) {
    DCHECK(metronome_source_);
  }

  ~WebRtcMetronomeAdapter() override { DCHECK(listeners_.empty()); }

  // Adds a tick listener to the metronome. Once this method has returned
  // OnTick will be invoked on each metronome tick. A listener may
  // only be added to the metronome once.
  void AddListener(TickListener* listener) override {
    DCHECK(listener);
    base::AutoLock auto_lock(lock_);
    auto [it, inserted] = listeners_.emplace(
        listener,
        base::MakeRefCounted<WebRtcMetronomeListenerWrapper>(listener));
    DCHECK(inserted);
    if (listeners_.size() == 1) {
      DCHECK(!tick_handle_);
      tick_handle_ = metronome_source_->AddListener(
          nullptr,
          base::BindRepeating(&WebRtcMetronomeAdapter::OnTick,
                              base::Unretained(this)),
          base::TimeTicks::Min());
    }
  }

  // Removes the tick listener from the metronome. Once this method has returned
  // OnTick will never be called again. This method must not be called from
  // within OnTick.
  void RemoveListener(TickListener* listener) override {
    DCHECK(listener);
    base::AutoLock auto_lock(lock_);
    auto it = listeners_.find(listener);
    if (it == listeners_.end()) {
      DLOG(WARNING) << __FUNCTION__ << " called with unregistered listener.";
      return;
    }
    it->second->Deactivate();
    listeners_.erase(it);
    if (listeners_.size() == 0) {
      metronome_source_->RemoveListener(std::move(tick_handle_));
    }
  }

  // Returns the current tick period of the metronome.
  webrtc::TimeDelta TickPeriod() const override {
    return webrtc::TimeDelta::Micros(
        metronome_source_->metronome_tick().InMicroseconds());
  }

 private:
  void OnTick() {
    base::AutoLock auto_lock(lock_);
    for (auto [listener, wrapper] : listeners_) {
      listener->OnTickTaskQueue()->PostTask(webrtc::ToQueuedTask(
          [wrapper = std::move(wrapper)] { wrapper->OnTick(); }));
    }
  }

  const scoped_refptr<MetronomeSource> metronome_source_;
  base::Lock lock_;
  base::flat_map<TickListener*, scoped_refptr<WebRtcMetronomeListenerWrapper>>
      listeners_ GUARDED_BY(lock_);
  scoped_refptr<MetronomeSource::ListenerHandle> tick_handle_ GUARDED_BY(lock_);
};

}  // namespace

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
  if (task_runner_ == nullptr) {
    // Run the task directly if task_runner_ is null.
    MaybeRunCallback();
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MetronomeSource::ListenerHandle::MaybeRunCallback,
                       this));
  }
}

void MetronomeSource::ListenerHandle::MaybeRunCallback() {
  DCHECK(task_runner_ == nullptr || task_runner_->RunsTasksInCurrentSequence());
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

std::unique_ptr<webrtc::Metronome> MetronomeSource::CreateWebRtcMetronome() {
  return std::make_unique<WebRtcMetronomeAdapter>(base::WrapRefCounted(this));
}

}  // namespace blink
