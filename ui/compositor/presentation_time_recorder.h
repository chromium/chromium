// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_PRESENTATION_TIME_RECORDER_H_
#define UI_COMPOSITOR_PRESENTATION_TIME_RECORDER_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"

namespace ui {

// PresentationTimeRecorder records the time between when an UI update is
// requested, and the requested UI change has been presented to the user
// (screen). This measure the longest presentation time for each commit by
// skipping updates made after the last request and next commit.  Use this if
// you want to measure the drawing performance in continuous operation that
// doesn't involve animations (such as window dragging). Call |RequestNext()|
// when you made modification to UI that should expect it will be presented.
class COMPOSITOR_EXPORT PresentationTimeRecorder {
 public:
  class PresentationTimeRecorderInternal;

  struct COMPOSITOR_EXPORT BucketParams {
    BucketParams();
    BucketParams(base::TimeDelta min_latency,
                 base::TimeDelta max_latency,
                 int num_buckets);
    BucketParams(const BucketParams&);
    BucketParams& operator=(const BucketParams&);
    ~BucketParams();

    static BucketParams CreateWithMaximum(base::TimeDelta max_latency);

    // Minimum expected latency. All samples less than this will go in underflow
    // bucket.
    base::TimeDelta min_latency = base::Milliseconds(1);
    // Maximum expected latency. All samples greater than this will go in
    // overflow bucket.
    base::TimeDelta max_latency = base::Milliseconds(200);
    // Number of buckets between `min_latency` and `max_latency` (uses default
    // exponential bucketing).
    int num_buckets = 50;
  };

  class COMPOSITOR_EXPORT TestApi {
   public:
    explicit TestApi(PresentationTimeRecorder* recorder);

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    void OnCompositingDidCommit(ui::Compositor* compositor);
    void OnPresented(int count,
                     base::TimeTicks requested_time,
                     const viz::FrameTimingDetails& frame_timing_details);

   private:
    raw_ptr<PresentationTimeRecorder> recorder_;
  };

  explicit PresentationTimeRecorder(
      raw_ptr<PresentationTimeRecorderInternal> internal);

  PresentationTimeRecorder(const PresentationTimeRecorder&) = delete;
  PresentationTimeRecorder& operator=(const PresentationTimeRecorder&) = delete;

  ~PresentationTimeRecorder();

  // Start recording next frame. It skips requesting next frame and returns
  // false if the previous frame has not been committed yet.
  bool RequestNext();

  // Returns the average latency of all recordings thus far. Returns `nullopt`
  // if no recordings have been made.
  std::optional<base::TimeDelta> GetAverageLatency() const;

  // Enable this to report the presentation time immediately with
  // fake value when RequestNext is called.
  static void SetReportPresentationTimeImmediatelyForTest(bool enable);

 private:
  // `PresentationTimeRecorderInternal` owns itself. Self destruct when
  // recording is done or on shutdown (whichever comes first).
  raw_ptr<PresentationTimeRecorderInternal> recorder_internal_ = nullptr;
};

// Creates a PresentationTimeRecorder that records timing histograms of
// presentation time and (if given) max latency. The time range is 1 to 200 ms,
// with 50 buckets.
COMPOSITOR_EXPORT std::unique_ptr<PresentationTimeRecorder>
CreatePresentationTimeHistogramRecorder(
    ui::Compositor* compositor,
    const char* presentation_time_histogram_name,
    const char* max_latency_histogram_name = "",
    PresentationTimeRecorder::BucketParams bucket_params =
        PresentationTimeRecorder::BucketParams(),
    bool emit_trace_event = false);

}  // namespace ui

#endif  // UI_COMPOSITOR_PRESENTATION_TIME_RECORDER_H_
