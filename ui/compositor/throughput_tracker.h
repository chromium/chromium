// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_THROUGHPUT_TRACKER_H_
#define UI_COMPOSITOR_THROUGHPUT_TRACKER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/throughput_tracker_host.h"

namespace ui {

class Compositor;
class ThroughputTrackerHost;

// A class to track the throughput of Compositor. The tracking is identified by
// an id. The id is passed into impl side and be used as the sequence id to
// create and stop a kCustom typed cc::FrameSequenceTracker. The class is
// move-only to have only one holder of the id. When ThroughputTracker is
// destroyed with an active tracking, the tracking will be canceled and report
// callback will not be invoked.
class COMPOSITOR_EXPORT ThroughputTracker {
 public:
  using TrackerId = ThroughputTrackerHost::TrackerId;

  // Move only.
  ThroughputTracker(ThroughputTracker&& other);
  ThroughputTracker& operator=(ThroughputTracker&& other);

  ~ThroughputTracker();

  // Starts tracking Compositor and provides a callback for reporting. The
  // throughput data collection starts after the next commit.
  void Start(ThroughputTrackerHost::ReportCallback callback);

  // Stops tracking. Returns true when the supplied callback will be invoked
  // when the data collection finishes. Returns false when the data collection
  // is finished before Stop() is called, e.g. when gpu process crashes.
  // Note that no data will be reported if Stop() is not called,
  bool Stop();

  // Cancels tracking. The supplied callback will not be invoked.
  void Cancel();

  // Cancels the pending report after `Stop()` is called. The supplied callback
  // will not be invoked after this. Note do not call this in the supplied
  // callback.
  // TODO(b/345297869): Merge with `Cancel`.
  void CancelReport();

 private:
  friend class Compositor;

  enum class State {
    kNotStarted,     // Tracking is not started.
    kStarted,        // Tracking is started.
    kWaitForReport,  // Tracking is stopped and waits for report.
    kCanceled,       // Tracking is stopped and reporting is canceled.
  };

  // Private since it should only be created via Compositor's
  // RequestNewThroughputTracker call.
  ThroughputTracker(TrackerId id, base::WeakPtr<ThroughputTrackerHost> host);

  static const TrackerId kInvalidId = 0u;
  TrackerId id_ = kInvalidId;
  base::WeakPtr<ThroughputTrackerHost> host_;
  State state_ = State::kNotStarted;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_THROUGHPUT_TRACKER_H_
