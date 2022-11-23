// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_PRESENTATION_TIME_RECORDER_H_
#define UI_COMPOSITOR_PRESENTATION_TIME_RECORDER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
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
  class COMPOSITOR_EXPORT TestApi {
   public:
    explicit TestApi(PresentationTimeRecorder* recorder);

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    void OnCompositingDidCommit(ui::Compositor* compositor);
    void OnPresented(int count,
                     base::TimeTicks requested_time,
                     base::TimeTicks presentation_timestamp);

   private:
    raw_ptr<PresentationTimeRecorder> recorder_;
  };

  explicit PresentationTimeRecorder(
      std::unique_ptr<PresentationTimeRecorderInternal> internal);

  PresentationTimeRecorder(const PresentationTimeRecorder&) = delete;
  PresentationTimeRecorder& operator=(const PresentationTimeRecorder&) = delete;

  ~PresentationTimeRecorder();

  // Start recording next frame. It skips requesting next frame and returns
  // false if the previous frame has not been committed yet.
  bool RequestNext();

  // Enable this to report the presentation time immediately with
  // fake value when RequestNext is called.
  static void SetReportPresentationTimeImmediatelyForTest(bool enable);

 private:
  std::unique_ptr<PresentationTimeRecorderInternal> recorder_internal_;
};

// Creates a PresentationTimeRecorder that records timing histograms of
// presentation time and (if given) max latency. The time range is 1 to 200 ms,
// with 50 buckets.
COMPOSITOR_EXPORT std::unique_ptr<PresentationTimeRecorder>
CreatePresentationTimeHistogramRecorder(
    ui::Compositor* compositor,
    const char* presentation_time_histogram_name,
    const char* max_latency_histogram_name = "");

}  // namespace ui

#endif  // UI_COMPOSITOR_PRESENTATION_TIME_RECORDER_H_
