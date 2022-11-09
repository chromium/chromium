// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_WEBRTC_OVERRIDES_METRONOME_SOURCE_H_
#define THIRD_PARTY_BLINK_WEBRTC_OVERRIDES_METRONOME_SOURCE_H_

#include <atomic>
#include <set>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
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
// The public API of the class except construction is meant to run on
// `metronome_task_runner`.
//
// `webrtc_component` does not have a test binary. See
// /third_party/blink/renderer/platform/peerconnection/metronome_source_test.cc
// for testing.
class RTC_EXPORT MetronomeSource final {
 public:
  // The tick phase.
  static base::TimeTicks Phase();
  // The tick frequency.
  static base::TimeDelta Tick();
  // The next metronome tick that is at or after |time|.
  static base::TimeTicks TimeSnappedToNextTick(base::TimeTicks time);

  explicit MetronomeSource(
      const scoped_refptr<base::SequencedTaskRunner>& metronome_task_runner);
  ~MetronomeSource();

  MetronomeSource(const MetronomeSource&) = delete;
  MetronomeSource& operator=(const MetronomeSource&) = delete;

  // Creates a webrtc::Metronome which is backed by this metronome.
  std::unique_ptr<webrtc::Metronome> CreateWebRtcMetronome();

 private:
  friend class WebRtcMetronomeAdapter;

  // Called by metronome when a callback is available for execution on the next
  // tick.
  void RequestCallOnNextTick(absl::AnyInvocable<void() &&> callback);

  // Called when a tick happens.
  void OnMetronomeTick();

  // Reschedules an invocation of OnMetronomeTick.
  void Reschedule();

  const scoped_refptr<base::SequencedTaskRunner> metronome_task_runner_;
  SEQUENCE_CHECKER(metronome_sequence_checker_);
  std::vector<absl::AnyInvocable<void() &&>> callbacks_
      GUARDED_BY_CONTEXT(metronome_sequence_checker_);
  base::WeakPtrFactory<MetronomeSource> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_WEBRTC_OVERRIDES_METRONOME_SOURCE_H_
