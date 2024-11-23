// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_metrics_tracker.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "ui/compositor/compositor_metrics_tracker_host.h"

namespace ui {

CompositorMetricsTracker::CompositorMetricsTracker(
    TrackerId id,
    base::WeakPtr<CompositorMetricsTrackerHost> host)
    : id_(id), host_(std::move(host)) {
  DCHECK(host_);
}

CompositorMetricsTracker::CompositorMetricsTracker(
    CompositorMetricsTracker&& other) {
  *this = std::move(other);
}

CompositorMetricsTracker& CompositorMetricsTracker::operator=(
    CompositorMetricsTracker&& other) {
  id_ = other.id_;
  host_ = std::move(other.host_);
  state_ = other.state_;

  other.id_ = kInvalidId;
  other.host_.reset();
  other.state_ = State::kNotStarted;
  return *this;
}

CompositorMetricsTracker::~CompositorMetricsTracker() {
  // Auto cancel if `Stop()` is not called.
  if (state_ == State::kStarted) {
    Cancel();
  }
}

void CompositorMetricsTracker::Start(
    CompositorMetricsTrackerHost::ReportCallback callback) {
  // Start after `host_` destruction is likely an error.
  DCHECK(host_);
  DCHECK_EQ(state_, State::kNotStarted);

  state_ = State::kStarted;
  host_->StartMetricsTracker(id_, std::move(callback));
}

bool CompositorMetricsTracker::Stop() {
  DCHECK_EQ(state_, State::kStarted);

  if (host_ && host_->StopMetricsTracker(id_)) {
    state_ = State::kWaitForReport;
    return true;
  }

  // No data will be reported. This could happen when gpu process crashed.
  // Treat the case as `kCanceled`.
  state_ = State::kCanceled;
  return false;
}

void CompositorMetricsTracker::Cancel() {
  // Some code calls `Cancel()` indirectly after receiving report. Allow this to
  // happen and make it a no-op. See https://crbug.com/1193382.
  if (state_ != State::kStarted) {
    return;
  }

  CancelReport();
}

void CompositorMetricsTracker::CancelReport() {
  // Report is only possible in `kStarted` and `kWaitForReport` state.
  if (state_ != State::kStarted && state_ != State::kWaitForReport) {
    return;
  }

  state_ = State::kCanceled;
  if (host_) {
    host_->CancelMetricsTracker(id_);
  }
}

}  // namespace ui
