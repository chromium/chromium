// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_WEBRTC_OVERRIDES_METRONOME_SOURCE_H_
#define THIRD_PARTY_BLINK_WEBRTC_OVERRIDES_METRONOME_SOURCE_H_

#include <atomic>
#include <set>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/delayed_task_handle.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/webrtc/api/metronome/metronome.h"
#include "third_party/webrtc/rtc_base/system/rtc_export.h"

namespace blink {

// The MetronomeSource ticks at a constant frequency, scheduling to wake up on
// ticks where listeners have work to do, and not scheduling to wake up on ticks
// where there is no work to do.
//
// When coalescing a large number of wakeup sources onto the MetronomeSource,
// this should reduce package Idle Wake Ups with potential to improve
// performance.
//
// The public API of this class is thread-safe and can be called from any
// sequence.
//
// |webrtc_component| does not have a test binary. See
// /third_party/blink/renderer/platform/peerconnection/metronome_source_test.cc
// for testing.
class RTC_EXPORT MetronomeSource final
    : public base::RefCountedThreadSafe<MetronomeSource> {
 public:
  // Identifies a listener and controls when its callback should be active.
  class RTC_EXPORT ListenerHandle
      : public base::RefCountedThreadSafe<ListenerHandle> {
   public:
    ListenerHandle(scoped_refptr<MetronomeSource> metronome_source,
                   scoped_refptr<base::SequencedTaskRunner> task_runner,
                   base::RepeatingCallback<void()> callback,
                   base::TimeTicks wakeup_time);

    // Sets the earliest time that the metronome may invoke the listener's
    // callback. If set to base::TimeTicks::Min(), the callback is called on
    // every metronome tick. If set to anything else, the callback is called a
    // single time until SetWakeupTime() is called again.
    void SetWakeupTime(base::TimeTicks wakeup_time);

   private:
    friend class base::RefCountedThreadSafe<ListenerHandle>;
    friend class MetronomeSource;

    ~ListenerHandle();

    void SetWakeUpTimeOnMetronomeTaskRunner(base::TimeTicks wakeup_time);
    void OnMetronomeTickOnMetronomeTaskRunner(base::TimeTicks now);
    void MaybeRunCallback();
    void Inactivate();

    const scoped_refptr<MetronomeSource> metronome_source_;
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;
    const base::RepeatingCallback<void()> callback_;
    base::Lock is_active_lock_;
    bool is_active_ GUARDED_BY(is_active_lock_) = true;
    // The earliest time to fire |callback_|. base::TimeTicks::Min() means to
    // fire on every tick, base::TimeTicks::Max() means never to fire.
    // Only touched on |metronome_source_->metronome_task_runner_|.
    base::TimeTicks wakeup_time_;
  };

  // The tick phase.
  static base::TimeTicks Phase();
  // The tick frequency.
  static base::TimeDelta Tick();
  // The next metronome tick that is at or after |time|.
  static base::TimeTicks TimeSnappedToNextTick(base::TimeTicks time);

  MetronomeSource();
  MetronomeSource(const MetronomeSource&) = delete;
  MetronomeSource& operator=(const MetronomeSource&) = delete;

  // Creates a new listener whose |callback| will be invoked on |task_runner|.
  // If |wakeup_time| is set to base::TimeTicks::Min() then the listener will be
  // called on every metronome tick. Otherwise |wakeup_time| is the earliest
  // time where the listener will be called a single time, after which
  // ListenerHandle::SetWakeupTime() has to be called for the listener to be
  // called again.
  scoped_refptr<ListenerHandle> AddListener(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::RepeatingCallback<void()> callback,
      base::TimeTicks wakeup_time = base::TimeTicks::Min());
  // After this call, the listener's callback is guaranteed not to be running
  // and won't ever run again. The listener can be removed from any thread, but
  // the listener cannot remove itself from within its own callback.
  void RemoveListener(scoped_refptr<ListenerHandle> listener_handle);

  // Creates a webrtc::Metronome which is backed by this metronome.
  std::unique_ptr<webrtc::Metronome> CreateWebRtcMetronome();

  // Ensures the next tick is scheduled and get the time to advance to reach
  // that tick. After advancing mock time by the returned time delta, the next
  // tick is guaranteed to happen MetronomeTick::Tick() from now.
  base::TimeDelta EnsureNextTickAndGetDelayForTesting();
  bool HasListenersForTesting();

 private:
  friend class base::RefCountedThreadSafe<MetronomeSource>;
  friend class ListenerHandle;

  ~MetronomeSource();

  void AddListenerOnMetronomeTaskRunner(
      scoped_refptr<ListenerHandle> listener_handle);
  void RemoveListenerOnMetronomeTaskRunner(
      scoped_refptr<ListenerHandle> listener_handle);
  // Ensures the "next tick" is scheduled. The next tick is the next metronome
  // tick where we have work to do. If there is no work between now and
  // |wakeup_time| we will reschedule such that the next tick happens at or
  // after |wakeup_time|, but if there is already a tick scheduled earlier than
  // |wakeup_time| this is a NO-OP and more ticks may be needed before
  // |wakeup_time| is reached.
  void EnsureNextTickIsScheduled(base::TimeTicks wakeup_time);

  // |now_tick| is the time that this tick was scheduled to run, so it should be
  // very close to base::TimeTicks::Now() but is guaranteed to be aligned with
  // the current metronome tick.
  void OnMetronomeTick(base::TimeTicks now_tick);

  // All non-const members are only accessed on |metronome_task_runner_|.
  const scoped_refptr<base::SequencedTaskRunner> metronome_task_runner_;
  std::set<scoped_refptr<ListenerHandle>> listeners_;
  base::DelayedTaskHandle next_tick_handle_;
  base::TimeTicks next_tick_ = base::TimeTicks::Min();
  base::TimeTicks prev_tick_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_WEBRTC_OVERRIDES_METRONOME_SOURCE_H_
