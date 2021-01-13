// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/total_animation_throughput_reporter.h"

#include "base/logging.h"
#include "ui/compositor/compositor.h"

namespace ui {

TotalAnimationThroughputReporter::TotalAnimationThroughputReporter(
    ui::Compositor* compositor,
    ReportOnceCallback once_callback,
    bool should_delete)
    : TotalAnimationThroughputReporter(compositor,
                                       ReportRepeatingCallback(),
                                       std::move(once_callback),
                                       should_delete) {}

TotalAnimationThroughputReporter::TotalAnimationThroughputReporter(
    ui::Compositor* compositor,
    ReportRepeatingCallback repeating_callback)
    : TotalAnimationThroughputReporter(compositor,
                                       repeating_callback,
                                       ReportOnceCallback(),
                                       /*should_delete=*/false) {}

TotalAnimationThroughputReporter::~TotalAnimationThroughputReporter() {
  if (throughput_tracker_)
    throughput_tracker_->Cancel();
  if (compositor_)
    compositor_->RemoveObserver(this);
}

void TotalAnimationThroughputReporter::OnFirstAnimationStarted(
    ui::Compositor* compositor) {
  throughput_tracker_ = compositor->RequestNewThroughputTracker();
  throughput_tracker_->Start(base::BindRepeating(
      &TotalAnimationThroughputReporter::Report, ptr_factory_.GetWeakPtr()));
}

void TotalAnimationThroughputReporter::OnLastAnimationEnded(
    ui::Compositor* compositor) {
  throughput_tracker_->Stop();
  throughput_tracker_.reset();
}

void TotalAnimationThroughputReporter::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  if (throughput_tracker_) {
    throughput_tracker_->Cancel();
    throughput_tracker_.reset();
  }
  compositor->RemoveObserver(this);
  compositor_ = nullptr;
  if (should_delete_)
    delete this;
}

TotalAnimationThroughputReporter::TotalAnimationThroughputReporter(
    ui::Compositor* compositor,
    ReportRepeatingCallback repeating_callback,
    ReportOnceCallback once_callback,
    bool should_delete)
    : compositor_(compositor),
      report_repeating_callback_(repeating_callback),
      report_once_callback_(std::move(once_callback)),
      should_delete_(should_delete) {
  DCHECK_NE(report_repeating_callback_.is_null(),
            report_once_callback_.is_null());

  compositor_->AddObserver(this);
  if (!compositor->animation_observer_list_.empty())
    OnFirstAnimationStarted(compositor_);
}

void TotalAnimationThroughputReporter::Report(
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  if (!report_once_callback_.is_null()) {
    compositor_->RemoveObserver(this);
    std::move(report_once_callback_).Run(data);
    if (should_delete_)
      delete this;
    return;
  }
  if (!report_repeating_callback_.is_null())
    report_repeating_callback_.Run(data);
}

}  // namespace ui
