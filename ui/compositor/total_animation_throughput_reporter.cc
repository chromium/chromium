// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/total_animation_throughput_reporter.h"

#include "base/logging.h"
#include "base/observer_list.h"
#include "ui/compositor/compositor.h"

namespace ui {

TotalAnimationThroughputReporter::ScopedThroughputReporterBlocker::
    ScopedThroughputReporterBlocker(
        base::WeakPtr<TotalAnimationThroughputReporter> reporter)
    : reporter_(std::move(reporter)) {
  reporter_->scoped_blocker_count_++;
}

TotalAnimationThroughputReporter::ScopedThroughputReporterBlocker::
    ~ScopedThroughputReporterBlocker() {
  if (reporter_)
    reporter_->scoped_blocker_count_--;
}

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
  compositor_ = nullptr;
}

void TotalAnimationThroughputReporter::OnFirstAnimationStarted(
    ui::Compositor* compositor) {
  if (!throughput_tracker_) {
    timestamp_first_animation_started_at_ = base::TimeTicks::Now();

    throughput_tracker_ = compositor->RequestNewThroughputTracker();
    throughput_tracker_->Start(base::BindRepeating(
        &TotalAnimationThroughputReporter::Report, ptr_factory_.GetWeakPtr()));
  }
}

void TotalAnimationThroughputReporter::OnFirstNonAnimatedFrameStarted(
    ui::Compositor* compositor) {
  if (IsBlocked())
    return;

  timestamp_last_animation_finished_at_ = base::TimeTicks::Now();

  throughput_tracker_->Stop();
  throughput_tracker_.reset();
  // Stop observing if no need to report multiple times.
  if (report_repeating_callback_.is_null())
    compositor_->RemoveObserver(this);
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

base::WeakPtr<ui::TotalAnimationThroughputReporter>
TotalAnimationThroughputReporter::GetWeakPtr() {
  return ptr_factory_.GetWeakPtr();
}

std::unique_ptr<
    TotalAnimationThroughputReporter::ScopedThroughputReporterBlocker>
TotalAnimationThroughputReporter::NewScopedBlocker() {
  return std::make_unique<
      ui::TotalAnimationThroughputReporter::ScopedThroughputReporterBlocker>(
      ptr_factory_.GetWeakPtr());
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
    std::move(report_once_callback_)
        .Run(data, timestamp_first_animation_started_at_,
             timestamp_last_animation_finished_at_);
    if (should_delete_)
      delete this;
    return;
  }
  if (!report_repeating_callback_.is_null())
    report_repeating_callback_.Run(data);
}

bool TotalAnimationThroughputReporter::IsBlocked() const {
  return scoped_blocker_count_;
}

}  // namespace ui
