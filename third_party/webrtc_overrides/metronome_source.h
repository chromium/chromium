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
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/webrtc/rtc_base/system/rtc_export.h"

namespace blink {

// The MetronomeSource posts tasks every metronome tick to its listeners.
// When coelescing a large number of wakeup sources onto the metronome this can
// greatly reduce the number of Idle Wake Ups, but it must be used with caution:
// when initiating a MetronomeSource we pay the wakeup cost up-front, regardless
// of the number of listeners.
//
// This class is thread safe. Listeners can be added and removed from any
// thread, but a listener cannot remove itself from within its own callback.
//
// The MetronomeSource is active while it has listeners. Removing all listeners
// deactivates it. Forgetting to remove a listener is a memory leak and leaves
// it running indefinitely.
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
    ListenerHandle(scoped_refptr<base::SequencedTaskRunner> task_runner,
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

    void OnMetronomeTick();
    void MaybeRunCallbackOnTaskRunner();

    void Inactivate();

    const scoped_refptr<base::SequencedTaskRunner> task_runner_;
    const base::RepeatingCallback<void()> callback_;
    base::Lock is_active_lock_;
    bool is_active_ GUARDED_BY(is_active_lock_) = true;
    base::Lock wakeup_time_lock_;
    // The earliest time that the metronome may invoke the listener's callback.
    base::TimeTicks wakeup_time_ GUARDED_BY(wakeup_time_lock_);
  };

  explicit MetronomeSource(base::TimeDelta metronome_tick);
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

  // The source is active as long as it has listeners. The source stays alive
  // until it has been deactivated by removing all listeners.
  bool IsActive();

 private:
  friend class base::RefCountedThreadSafe<MetronomeSource>;

  ~MetronomeSource();

  void StartTimer() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void StopTimer() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void OnMetronomeTick();

  const scoped_refptr<base::SequencedTaskRunner> metronome_task_runner_;
  const base::TimeDelta metronome_tick_;
  base::Lock lock_;
  // Set to true in StartTimer() and false in StopTimer() but the creation or
  // destruction of the timer happens asynchronously (e.g. we can be active but
  // not yet have a timer).
  bool is_active_ GUARDED_BY(lock_) = false;
  // Only accessed on |metronome_task_runner_|.
  std::unique_ptr<base::RepeatingTimer> timer_;
  std::set<scoped_refptr<ListenerHandle>> listeners_ GUARDED_BY(lock_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_WEBRTC_OVERRIDES_METRONOME_SOURCE_H_
