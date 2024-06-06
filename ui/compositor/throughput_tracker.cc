// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/throughput_tracker.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"

namespace ui {

ThroughputTracker::ThroughputTracker(TrackerId id,
                                     base::WeakPtr<ThroughputTrackerHost> host)
    : id_(id), host_(std::move(host)) {
  DCHECK(host_);
}

ThroughputTracker::ThroughputTracker(ThroughputTracker&& other) {
  *this = std::move(other);
}

ThroughputTracker& ThroughputTracker::operator=(ThroughputTracker&& other) {
  id_ = other.id_;
  host_ = std::move(other.host_);
  state_ = other.state_;

  other.id_ = kInvalidId;
  other.host_.reset();
  other.state_ = State::kNotStarted;
  return *this;
}

ThroughputTracker::~ThroughputTracker() {
  // Auto cancel if `Stop` is not called.
  if (state_ == State::kStarted) {
    Cancel();
  }
}

void ThroughputTracker::Start(ThroughputTrackerHost::ReportCallback callback) {
  // Start after |host_| destruction is likely an error.
  DCHECK(host_);
  DCHECK_EQ(state_, State::kNotStarted);

  state_ = State::kStarted;
  host_->StartThroughputTracker(id_, std::move(callback));
}

bool ThroughputTracker::Stop() {
  DCHECK_EQ(state_, State::kStarted);

  state_ = State::kWaitForReport;
  if (host_)
    return host_->StopThroughputTracker(id_);

  return false;
}

void ThroughputTracker::Cancel() {
  // Some code calls Cancel() indirectly after receiving report. Allow this to
  // happen and make it a no-op. See https://crbug.com/1193382.
  if (state_ != State::kStarted) {
    return;
  }

  CancelReport();
}

void ThroughputTracker::CancelReport() {
  DCHECK(state_ == State::kStarted || state_ == State::kWaitForReport);

  state_ = State::kCanceled;
  if (host_)
    host_->CancelThroughputTracker(id_);
}

}  // namespace ui
