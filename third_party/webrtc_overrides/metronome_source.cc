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
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/webrtc/api/metronome/metronome.h"
#include "third_party/webrtc/api/task_queue/pending_task_safety_flag.h"

namespace blink {

constexpr base::TimeDelta kMetronomeTick = base::Hertz(64);

namespace {

// Stores a MetronomeSource::ListenerHandle which handles listening to handle
// ticks, and an atomic flag for cancelling the task attached to the listener.
// When a TickListener invokes, it will check that the cancel flag was not set.
// To avoid a race between the cancel flag and `OnTick` being invoked after
// `RemoveListener`, `RemoveListener` needs to be called from
// `listener->OnTickTaskQueue()`. This is the case for the only user -
// webrtc::DecodeSynchronizer.
//
// TODO(http://crbug.com/1253787): Clarify threading requirements of
// webrtc::Metronome, or change interface.
struct HandleWithCancelation {
  HandleWithCancelation(
      scoped_refptr<MetronomeSource::ListenerHandle> handle,
      rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> task_safety)
      : handle(std::move(handle)), task_safety(std::move(task_safety)) {}

  scoped_refptr<MetronomeSource::ListenerHandle> handle;
  rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> task_safety;
};

void InvokeOnTickOnWebRtcTaskQueue(
    webrtc::Metronome::TickListener* listener,
    rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> task_safety) {
  listener->OnTickTaskQueue()->PostTask(webrtc::SafeTask(
      std::move(task_safety), [listener] { listener->OnTick(); }));
}

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
    auto task_safety = webrtc::PendingTaskSafetyFlag::Create();
    // `listener` can be unretained since the `handle` will be cancelled when
    // `listener` is removed.
    auto handle = metronome_source_->AddListener(
        nullptr, base::BindRepeating(&InvokeOnTickOnWebRtcTaskQueue,
                                     base::Unretained(listener), task_safety));
    base::AutoLock auto_lock(lock_);
    auto [it, inserted] = listeners_.emplace(
        std::piecewise_construct, std::forward_as_tuple(listener),
        std::forward_as_tuple(std::move(handle), std::move(task_safety)));
    DCHECK(inserted);
  }

  // Removes the tick listener from the metronome. Once this method has returned
  // OnTick will never be called again. This method must not be called from
  // within OnTick.
  void RemoveListener(TickListener* listener) override {
    DCHECK(listener);
    scoped_refptr<MetronomeSource::ListenerHandle> handle;
    rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> task_safety;

    {
      base::AutoLock auto_lock(lock_);
      auto it = listeners_.find(listener);
      if (it == listeners_.end()) {
        DLOG(WARNING) << __FUNCTION__ << " called with unregistered listener.";
        return;
      }
      handle = std::move(it->second.handle);
      task_safety = std::move(it->second.task_safety);
      listeners_.erase(listener);
    }
    task_safety->SetNotAlive();
    metronome_source_->RemoveListener(std::move(handle));
  }

  // Returns the current tick period of the metronome.
  webrtc::TimeDelta TickPeriod() const override {
    return webrtc::TimeDelta::Micros(MetronomeSource::Tick().InMicroseconds());
  }

 private:
  const scoped_refptr<MetronomeSource> metronome_source_;
  base::Lock lock_;
  base::flat_map<TickListener*, HandleWithCancelation> listeners_
      GUARDED_BY(lock_);
};

}  // namespace

MetronomeSource::ListenerHandle::ListenerHandle(
    scoped_refptr<MetronomeSource> metronome_source,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void()> callback,
    base::TimeTicks wakeup_time)
    : metronome_source_(std::move(metronome_source)),
      task_runner_(std::move(task_runner)),
      callback_(std::move(callback)),
      wakeup_time_(std::move(wakeup_time)) {}

MetronomeSource::ListenerHandle::~ListenerHandle() = default;

void MetronomeSource::ListenerHandle::SetWakeupTime(
    base::TimeTicks wakeup_time) {
  metronome_source_->metronome_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MetronomeSource::ListenerHandle::SetWakeUpTimeOnMetronomeTaskRunner,
          this, wakeup_time));
}

void MetronomeSource::ListenerHandle::SetWakeUpTimeOnMetronomeTaskRunner(
    base::TimeTicks wakeup_time) {
  DCHECK(
      metronome_source_->metronome_task_runner_->RunsTasksInCurrentSequence());
  wakeup_time_ = wakeup_time;
  metronome_source_->EnsureNextTickIsScheduled(wakeup_time);
}

void MetronomeSource::ListenerHandle::OnMetronomeTickOnMetronomeTaskRunner(
    base::TimeTicks now) {
  DCHECK(
      metronome_source_->metronome_task_runner_->RunsTasksInCurrentSequence());
  if (wakeup_time_.is_max()) {
    // This listener is sleeping indefinitely.
    return;
  }
  if (now < wakeup_time_) {
    // It is not time for this listener to fire yet, but ensure that the next
    // tick is scheduled.
    metronome_source_->EnsureNextTickIsScheduled(wakeup_time_);
    return;
  }
  if (!wakeup_time_.is_min()) {
    // A wakeup time had been specified (set to anything other than "min").
    // Reset the wakeup time to "infinity", meaning SetWakeupTime() has to be
    // called again in order to wake up again.
    wakeup_time_ = base::TimeTicks::Max();
  }
  if (task_runner_ == nullptr) {
    // Run the task directly if |task_runner_| is null.
    MaybeRunCallback();
  } else {
    // Post to run on target |task_runner_|.
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

// static
base::TimeTicks MetronomeSource::Phase() {
  return base::TimeTicks();
}

// static
base::TimeDelta MetronomeSource::Tick() {
  return kMetronomeTick;
}

// static
base::TimeTicks MetronomeSource::TimeSnappedToNextTick(base::TimeTicks time) {
  return time.SnappedToNextTick(MetronomeSource::Phase(),
                                MetronomeSource::Tick());
}

MetronomeSource::MetronomeSource()
    : metronome_task_runner_(
          // HIGHEST priority is used to reduce risk of jitter.
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::TaskPriority::HIGHEST})) {
  base::TimeTicks now = base::TimeTicks::Now();
  prev_tick_ = MetronomeSource::TimeSnappedToNextTick(now);
  if (prev_tick_ > now)
    prev_tick_ -= MetronomeSource::Tick();
}

MetronomeSource::~MetronomeSource() {
  DCHECK(listeners_.empty());
  DCHECK(!next_tick_handle_.IsValid());
}

scoped_refptr<MetronomeSource::ListenerHandle> MetronomeSource::AddListener(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void()> callback,
    base::TimeTicks wakeup_time) {
  // Ref-counting keeps |this| alive until all listeners have been removed.
  scoped_refptr<ListenerHandle> listener_handle =
      base::MakeRefCounted<ListenerHandle>(this, std::move(task_runner),
                                           std::move(callback), wakeup_time);
  metronome_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MetronomeSource::AddListenerOnMetronomeTaskRunner,
                     scoped_refptr<MetronomeSource>(this), listener_handle));
  return listener_handle;
}

void MetronomeSource::RemoveListener(
    scoped_refptr<ListenerHandle> listener_handle) {
  listener_handle->Inactivate();
  metronome_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MetronomeSource::RemoveListenerOnMetronomeTaskRunner,
                     scoped_refptr<MetronomeSource>(this), listener_handle));
}

void MetronomeSource::AddListenerOnMetronomeTaskRunner(
    scoped_refptr<ListenerHandle> listener_handle) {
  DCHECK(metronome_task_runner_->RunsTasksInCurrentSequence());
  listeners_.insert(listener_handle);
  EnsureNextTickIsScheduled(listener_handle->wakeup_time_);
}

void MetronomeSource::RemoveListenerOnMetronomeTaskRunner(
    scoped_refptr<ListenerHandle> listener_handle) {
  DCHECK(metronome_task_runner_->RunsTasksInCurrentSequence());
  listeners_.erase(listener_handle);
  // To avoid additional complexity we do not reschedule the next tick, but we
  // do cancel the next tick if there are no more listeners.
  if (listeners_.empty()) {
    next_tick_ = base::TimeTicks::Min();
    next_tick_handle_.CancelTask();
  }
}

void MetronomeSource::EnsureNextTickIsScheduled(base::TimeTicks wakeup_time) {
  DCHECK(metronome_task_runner_->RunsTasksInCurrentSequence());
  if (wakeup_time.is_max()) {
    return;
  }
  if (wakeup_time <= prev_tick_) {
    // Do not reschedule a tick that already fired, such as when adding a
    // listener on a tick.
    wakeup_time = prev_tick_ + MetronomeSource::Tick();
  }
  base::TimeTicks wakeup_tick =
      MetronomeSource::TimeSnappedToNextTick(wakeup_time);
  if (!next_tick_.is_min() && wakeup_tick >= next_tick_) {
    //  We already have the next tick scheduled.
    return;
  }
  // If we already have a tick scheduled but too far in the future, cancel it.
  next_tick_handle_.CancelTask();
  next_tick_ = wakeup_tick;
  next_tick_handle_ = metronome_task_runner_->PostCancelableDelayedTaskAt(
      base::subtle::PostDelayedTaskPassKey(), FROM_HERE,
      base::BindOnce(&MetronomeSource::OnMetronomeTick,
                     // Unretained is safe because tasks are cancelled prior to
                     // destruction.
                     base::Unretained(this), next_tick_),
      next_tick_, base::subtle::DelayPolicy::kPrecise);
}

void MetronomeSource::OnMetronomeTick(base::TimeTicks now_tick) {
  TRACE_EVENT_INSTANT0("webrtc", "MetronomeSource::OnMetronomeTick",
                       TRACE_EVENT_SCOPE_PROCESS);
  DCHECK(metronome_task_runner_->RunsTasksInCurrentSequence());
  // We no longer have a tick scheduled.
  prev_tick_ = now_tick;
  next_tick_ = base::TimeTicks::Min();
  bool schedule_next_tick = false;
  base::TimeTicks now = base::TimeTicks::Now();
  // On some platforms (Android), base::TimeTicks::Now() may in some cases lag
  // behind by ~1 ms due to clock caching. To ensure listeners with the same
  // wake up time as |now_tick| runs, ensure |now| is at least |now_tick|.
  if (now < now_tick) {
    now = now_tick;
  }
  for (auto& listener : listeners_) {
    listener->OnMetronomeTickOnMetronomeTaskRunner(now);
    schedule_next_tick |= listener->wakeup_time_.is_min();
  }
  if (schedule_next_tick) {
    // The next tick is `now_tick + metronome_tick_`, but if late wakeup happens
    // due to load we could in extreme cases miss ticks. To avoid posting
    // immediate "catch-up" tasks, make it possible to skip metronome ticks.
    constexpr double kTickThreshold = 0.5;
    EnsureNextTickIsScheduled(base::TimeTicks::Now() +
                              MetronomeSource::Tick() * kTickThreshold);
  }
}

std::unique_ptr<webrtc::Metronome> MetronomeSource::CreateWebRtcMetronome() {
  return std::make_unique<WebRtcMetronomeAdapter>(base::WrapRefCounted(this));
}

base::TimeDelta MetronomeSource::EnsureNextTickAndGetDelayForTesting() {
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks next_tick = MetronomeSource::TimeSnappedToNextTick(now);
  // Ensure next tick is scheduled, even if there are no listeners. This makes
  // it so that when mock time is advanced to |next_tick|, |prev_tick_| will be
  // updated. This avoids the initial tick firing "now" in testing environments.
  metronome_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MetronomeSource::EnsureNextTickIsScheduled,
                                this, next_tick));
  return next_tick - now;
}

bool MetronomeSource::HasListenersForTesting() {
  base::WaitableEvent event;
  bool has_listeners = false;
  metronome_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](MetronomeSource* thiz, bool* has_listeners,
                        base::WaitableEvent* event) {
                       *has_listeners = !thiz->listeners_.empty();
                       event->Signal();
                     },
                     base::Unretained(this), base::Unretained(&has_listeners),
                     base::Unretained(&event)));
  event.Wait();
  return has_listeners;
}

}  // namespace blink
