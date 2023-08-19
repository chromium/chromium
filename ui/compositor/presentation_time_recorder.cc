// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/presentation_time_recorder.h"

#include <ostream>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"

namespace ui {

namespace {

bool report_immediately_for_test = false;

}  // namespace

// PresentationTimeRecorderInternal -------------------------------------------

class PresentationTimeRecorder::PresentationTimeRecorderInternal
    : public ui::CompositorObserver {
 public:
  explicit PresentationTimeRecorderInternal(ui::Compositor* compositor)
      : compositor_(compositor) {
    compositor_->AddObserver(this);
    VLOG(1) << "Start Recording Frame Time";
  }

  PresentationTimeRecorderInternal(const PresentationTimeRecorderInternal&) =
      delete;
  PresentationTimeRecorderInternal& operator=(
      const PresentationTimeRecorderInternal&) = delete;

  ~PresentationTimeRecorderInternal() override {
    const int average_latency_ms =
        present_count_ ? total_latency_ms_ / present_count_ : 0;
    VLOG(1) << "Finished Recording FrameTime: average latency="
            << average_latency_ms << "ms, max latency=" << max_latency_ms_
            << "ms";
    if (compositor_)
      compositor_->RemoveObserver(this);
  }

  // Start recording next frame. It skips requesting next frame and returns
  // false if the previous frame has not been committed yet.
  bool RequestNext();

  // ui::CompositorObserver:
  void OnCompositingDidCommit(ui::Compositor* compositor) override {
    // Skip updating the state if commit happened after present without
    // request because the commit is for unrelated activity.
    if (state_ != PRESENTED)
      state_ = COMMITTED;
  }
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    DCHECK_EQ(compositor_, compositor);
    compositor_->RemoveObserver(this);
    compositor_ = nullptr;
    if (!recording_)
      delete this;
  }

  // Mark the recorder to be deleted when the last presentation feedback
  // is reported.
  void EndRecording() {
    recording_ = false;
    if (state_ == PRESENTED)
      delete this;
  }

 protected:
  int max_latency_ms() const { return max_latency_ms_; }
  int present_count() const { return present_count_; }

 private:
  friend class TestApi;

  enum State {
    // The frame has been presented to the screen. This is the initial state.
    PRESENTED,
    // The presentation feedback has been requested.
    REQUESTED,
    // The changes to layers have been submitted, and waiting to be presented.
    COMMITTED,
  };

  // |delta| is the duration between the successful request time and
  // presentation time.
  virtual void ReportTime(base::TimeDelta delta) = 0;

  void OnPresented(int count,
                   base::TimeTicks requested_time,
                   base::TimeTicks presentation_timestamp);

  State state_ = PRESENTED;

  int present_count_ = 0;
  int request_count_ = 0;
  int total_latency_ms_ = 0;
  int max_latency_ms_ = 0;

  raw_ptr<ui::Compositor> compositor_ = nullptr;
  bool recording_ = true;

  base::WeakPtrFactory<PresentationTimeRecorderInternal> weak_ptr_factory_{
      this};
};

bool PresentationTimeRecorder::PresentationTimeRecorderInternal::RequestNext() {
  if (!compositor_)
    return false;

  if (state_ == REQUESTED)
    return false;

  const base::TimeTicks now = base::TimeTicks::Now();

  VLOG(1) << "Start Next (" << request_count_
          << ") state=" << (state_ == COMMITTED ? "Committed" : "Presented");
  state_ = REQUESTED;

  if (report_immediately_for_test) {
    state_ = COMMITTED;
    OnPresented(request_count_++, now, now);
    return true;
  }

  compositor_->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindOnce(&PresentationTimeRecorderInternal::OnPresented,
                     weak_ptr_factory_.GetWeakPtr(), request_count_++, now));
  return true;
}

void PresentationTimeRecorder::PresentationTimeRecorderInternal::OnPresented(
    int count,
    base::TimeTicks requested_time,
    base::TimeTicks presentation_timestamp) {
  std::unique_ptr<PresentationTimeRecorderInternal> deleter;
  if (!recording_ && (count == (request_count_ - 1)))
    deleter = base::WrapUnique(this);

  if (state_ == COMMITTED)
    state_ = PRESENTED;

  if (presentation_timestamp.is_null()) {
    // TODO(b/165951963): ideally feedback.timestamp should not be null.
    // Consider replacing this by DCHECK or CHECK.
    LOG(ERROR) << "Invalid feedback timestamp (" << count << "):"
               << " timestamp is not set";
    return;
  }
  const base::TimeDelta delta = presentation_timestamp - requested_time;
  if (delta.InMilliseconds() < 0) {
    LOG(ERROR) << "Invalid timestamp for presentation feedback (" << count
               << "): requested_time=" << requested_time
               << " presentation_timestamp=" << presentation_timestamp;
    return;
  }
  if (delta.InMilliseconds() > max_latency_ms_)
    max_latency_ms_ = delta.InMilliseconds();

  present_count_++;
  total_latency_ms_ += delta.InMilliseconds();
  ReportTime(delta);
  VLOG(1) << "OnPresented (" << count << "):" << delta.InMilliseconds();
}

// PresentationTimeRecorder ---------------------------------------------------

PresentationTimeRecorder::PresentationTimeRecorder(
    std::unique_ptr<PresentationTimeRecorderInternal> internal)
    : recorder_internal_(std::move(internal)) {}

PresentationTimeRecorder::~PresentationTimeRecorder() {
  auto* recorder_internal = recorder_internal_.release();
  // The internal recorder self destruct when finished its job.
  recorder_internal->EndRecording();
}

bool PresentationTimeRecorder::RequestNext() {
  return recorder_internal_->RequestNext();
}

// static
void PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
    bool enable) {
  report_immediately_for_test = enable;
}

namespace {

base::HistogramBase* CreateTimesHistogram(const char* name,
                                          base::TimeDelta maximum) {
  return base::Histogram::FactoryTimeGet(
      name, base::Milliseconds(1), maximum, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}

// PresentationTimeHistogramRecorder ------------------------------------------

class PresentationTimeHistogramRecorder
    : public PresentationTimeRecorder::PresentationTimeRecorderInternal {
 public:
  // |presentation_time_histogram_name| records latency reported on
  // |ReportTime()|. If |max_latency_histogram_name| is not empty, it records
  // the maximum latency reported during the lifetime of this object.  Histogram
  // names must be the name of the UMA histogram defined in histograms.xml.
  PresentationTimeHistogramRecorder(
      ui::Compositor* compositor,
      const char* presentation_time_histogram_name,
      const char* max_latency_histogram_name,
      base::TimeDelta maximum)
      : PresentationTimeRecorderInternal(compositor),
        presentation_time_histogram_(
            CreateTimesHistogram(presentation_time_histogram_name, maximum)),
        max_latency_histogram_name_(max_latency_histogram_name),
        maximum_(maximum) {}

  PresentationTimeHistogramRecorder(const PresentationTimeHistogramRecorder&) =
      delete;
  PresentationTimeHistogramRecorder& operator=(
      const PresentationTimeHistogramRecorder&) = delete;

  ~PresentationTimeHistogramRecorder() override {
    if (present_count() > 0 && !max_latency_histogram_name_.empty()) {
      CreateTimesHistogram(max_latency_histogram_name_.c_str(), maximum_)
          ->AddTimeMillisecondsGranularity(
              base::Milliseconds(max_latency_ms()));
    }
  }

  // PresentationTimeRecorderInternal:
  void ReportTime(base::TimeDelta delta) override {
    presentation_time_histogram_->AddTimeMillisecondsGranularity(delta);
  }

 private:
  raw_ptr<base::HistogramBase> presentation_time_histogram_;
  std::string max_latency_histogram_name_;
  base::TimeDelta maximum_;
};

}  // namespace

std::unique_ptr<PresentationTimeRecorder>
CreatePresentationTimeHistogramRecorder(
    ui::Compositor* compositor,
    const char* presentation_time_histogram_name,
    const char* max_latency_histogram_name,
    base::TimeDelta maximum) {
  return std::make_unique<PresentationTimeRecorder>(
      std::make_unique<PresentationTimeHistogramRecorder>(
          compositor, presentation_time_histogram_name,
          max_latency_histogram_name, maximum));
}

// TestApi --------------------------------------------------------------------

PresentationTimeRecorder::TestApi::TestApi(PresentationTimeRecorder* recorder)
    : recorder_(recorder) {}

void PresentationTimeRecorder::TestApi::OnCompositingDidCommit(
    ui::Compositor* compositor) {
  recorder_->recorder_internal_->OnCompositingDidCommit(compositor);
}

void PresentationTimeRecorder::TestApi::OnPresented(
    int count,
    base::TimeTicks requested_time,
    base::TimeTicks presentation_timestamp) {
  recorder_->recorder_internal_->OnPresented(count, requested_time,
                                             presentation_timestamp);
}

}  // namespace ui
