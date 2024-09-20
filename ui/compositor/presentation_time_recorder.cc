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
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"

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

  // Start recording next frame. It skips requesting next frame and returns
  // false if the previous frame has not been committed yet.
  bool RequestNext();

  std::optional<base::TimeDelta> GetAverageLatency() const {
    if (present_count_) {
      return base::Milliseconds(total_latency_ms_ / present_count_);
    }
    return std::nullopt;
  }

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
      SelfDestruct();
  }

  // Mark the recorder to be deleted when the last presentation feedback
  // is reported.
  void EndRecording() {
    recording_ = false;
    const bool has_compositing_shut_down = !compositor_;
    // If compositing has shut down and the frame hasn't been presented yet,
    // it won't happen at this point. This class must self destruct in this case
    // otherwise it will leak.
    if (state_ == PRESENTED || has_compositing_shut_down) {
      SelfDestruct();
    }
  }

 protected:
  int max_latency_ms() const { return max_latency_ms_; }
  int present_count() const { return present_count_; }

  ~PresentationTimeRecorderInternal() override {
    DCHECK(!recording_);
    const std::optional<base::TimeDelta> average_latency = GetAverageLatency();
    VLOG(1) << "Finished Recording FrameTime: average latency="
            << (average_latency ? average_latency->InMilliseconds() : 0)
            << "ms, max latency=" << max_latency_ms_ << "ms";
    if (compositor_) {
      compositor_->RemoveObserver(this);
    }
  }

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

  class Deleter {
   public:
    explicit Deleter(PresentationTimeRecorderInternal* recorder_internal)
        : recorder_internal_(recorder_internal) {}

    Deleter(const Deleter&) = delete;
    Deleter& operator=(const Deleter&) = delete;

    ~Deleter() { recorder_internal_.ExtractAsDangling()->SelfDestruct(); }

   private:
    raw_ptr<PresentationTimeRecorderInternal> recorder_internal_ = nullptr;
  };

  // |delta| is the duration between the successful request time and
  // presentation time.
  virtual void ReportTime(base::TimeDelta delta) = 0;

  void OnPresented(int count,
                   base::TimeTicks requested_time,
                   const viz::FrameTimingDetails& frame_timing_details);

  void SelfDestruct();

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
    viz::FrameTimingDetails details;
    details.presentation_feedback.timestamp = now;
    OnPresented(request_count_++, now, details);
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
    const viz::FrameTimingDetails& frame_timing_details) {
  base::TimeTicks presentation_timestamp =
      frame_timing_details.presentation_feedback.timestamp;
  std::optional<Deleter> deleter;
  if (!recording_ && (count == (request_count_ - 1)))
    deleter.emplace(this);

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

void PresentationTimeRecorder::PresentationTimeRecorderInternal::
    SelfDestruct() {
  delete this;
}

// PresentationTimeRecorder ---------------------------------------------------

PresentationTimeRecorder::PresentationTimeRecorder(
    raw_ptr<PresentationTimeRecorderInternal> internal)
    : recorder_internal_(std::move(internal)) {}

PresentationTimeRecorder::~PresentationTimeRecorder() {
  // The internal recorder self destruct when finished its job.
  recorder_internal_.ExtractAsDangling()->EndRecording();
}

bool PresentationTimeRecorder::RequestNext() {
  return recorder_internal_->RequestNext();
}

std::optional<base::TimeDelta> PresentationTimeRecorder::GetAverageLatency()
    const {
  return recorder_internal_->GetAverageLatency();
}

// static
void PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
    bool enable) {
  report_immediately_for_test = enable;
}

namespace {

base::HistogramBase* CreateTimesHistogram(
    const char* name,
    const PresentationTimeRecorder::BucketParams& bucket_params) {
  return base::Histogram::FactoryTimeGet(
      name, bucket_params.min_latency, bucket_params.max_latency,
      bucket_params.num_buckets,
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
      const PresentationTimeRecorder::BucketParams& bucket_params,
      bool emit_trace_event)
      : PresentationTimeRecorderInternal(compositor),
        presentation_time_histogram_(
            CreateTimesHistogram(presentation_time_histogram_name,
                                 bucket_params)),
        max_latency_histogram_name_(max_latency_histogram_name),
        bucket_params_(bucket_params),
        presentation_time_histogram_name_(
            emit_trace_event ? presentation_time_histogram_name : nullptr) {
    if (presentation_time_histogram_name_) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ui", presentation_time_histogram_name_,
                                        this);
    }
  }

  PresentationTimeHistogramRecorder(const PresentationTimeHistogramRecorder&) =
      delete;
  PresentationTimeHistogramRecorder& operator=(
      const PresentationTimeHistogramRecorder&) = delete;

  // PresentationTimeRecorderInternal:
  void ReportTime(base::TimeDelta delta) override {
    if (presentation_time_histogram_name_) {
      TRACE_EVENT_NESTABLE_ASYNC_END0("ui", presentation_time_histogram_name_,
                                      this);
    }
    presentation_time_histogram_->AddTimeMillisecondsGranularity(delta);
  }

 private:
  ~PresentationTimeHistogramRecorder() override {
    if (present_count() > 0 && !max_latency_histogram_name_.empty()) {
      CreateTimesHistogram(max_latency_histogram_name_.c_str(), bucket_params_)
          ->AddTimeMillisecondsGranularity(
              base::Milliseconds(max_latency_ms()));
    }
  }

  raw_ptr<base::HistogramBase> presentation_time_histogram_;
  std::string max_latency_histogram_name_;
  const PresentationTimeRecorder::BucketParams bucket_params_;
  // Only set if `emit_trace_event_` is true since that's its only use.
  const char* const presentation_time_histogram_name_ = nullptr;
};

}  // namespace

// BucketParams ------------------------------------------

PresentationTimeRecorder::BucketParams::BucketParams() = default;

PresentationTimeRecorder::BucketParams::BucketParams(
    base::TimeDelta min_latency,
    base::TimeDelta max_latency,
    int num_buckets)
    : min_latency(min_latency),
      max_latency(max_latency),
      num_buckets(num_buckets) {}

PresentationTimeRecorder::BucketParams::BucketParams(const BucketParams&) =
    default;

PresentationTimeRecorder::BucketParams&
PresentationTimeRecorder::BucketParams::operator=(const BucketParams&) =
    default;

PresentationTimeRecorder::BucketParams::~BucketParams() = default;

// static
PresentationTimeRecorder::BucketParams
PresentationTimeRecorder::BucketParams::CreateWithMaximum(
    base::TimeDelta max_latency) {
  BucketParams params;
  params.max_latency = max_latency;
  return params;
}

std::unique_ptr<PresentationTimeRecorder>
CreatePresentationTimeHistogramRecorder(
    ui::Compositor* compositor,
    const char* presentation_time_histogram_name,
    const char* max_latency_histogram_name,
    PresentationTimeRecorder::BucketParams bucket_params,
    bool emit_trace_event) {
  return std::make_unique<PresentationTimeRecorder>(
      new PresentationTimeHistogramRecorder(
          compositor, presentation_time_histogram_name,
          max_latency_histogram_name, bucket_params, emit_trace_event));
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
    const viz::FrameTimingDetails& frame_timing_details) {
  recorder_->recorder_internal_->OnPresented(count, requested_time,
                                             frame_timing_details);
}

}  // namespace ui
