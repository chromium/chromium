// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/throughput_tracker.h"

#include <utility>

#include "base/callback.h"
#include "base/check.h"

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
  started_ = other.started_;

  other.id_ = kInvalidId;
  other.host_.reset();
  other.started_ = false;
  return *this;
}

ThroughputTracker::~ThroughputTracker() {
  if (started_)
    Cancel();
}

void ThroughputTracker::Start(ThroughputTrackerHost::ReportCallback callback) {
  // Start after |host_| destruction is likely an error.
  DCHECK(host_);
  DCHECK(!started_);

  started_ = true;
  host_->StartThroughputTracker(id_, std::move(callback));
}

bool ThroughputTracker::Stop() {
  DCHECK(started_);

  started_ = false;
  if (host_)
    return host_->StopThroughtputTracker(id_);

  return false;
}

void ThroughputTracker::Cancel() {
  // Some code calls Cancel() indirectly after receiving report. Allow this to
  // happen and make it a no-op. See https://crbug.com/1193382.
  if (!started_)
    return;

  started_ = false;
  if (host_)
    host_->CancelThroughtputTracker(id_);
}

}  // namespace ui
