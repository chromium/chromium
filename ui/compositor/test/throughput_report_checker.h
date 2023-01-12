// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_THROUGHPUT_REPORT_CHECKER_H_
#define UI_COMPOSITOR_TEST_THROUGHPUT_REPORT_CHECKER_H_

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "cc/metrics/frame_sequence_metrics.h"

namespace ui {
class AnimationThroughputReporterTestBase;

class ThroughputReportChecker {
 public:
  using ReportRepeatingCallback = base::RepeatingCallback<void(
      const cc::FrameSequenceMetrics::CustomReportData&)>;
  using ReportOnceCallback = base::OnceCallback<void(
      const cc::FrameSequenceMetrics::CustomReportData&)>;

  explicit ThroughputReportChecker(
      AnimationThroughputReporterTestBase* test_base,
      bool fail_if_reported = false)
      : test_base_(test_base), fail_if_reported_(fail_if_reported) {}
  ThroughputReportChecker(const ThroughputReportChecker&) = delete;
  ThroughputReportChecker& operator=(const ThroughputReportChecker&) = delete;
  ~ThroughputReportChecker() = default;

  bool reported() const { return reported_; }

  void reset() { reported_ = false; }

  ReportRepeatingCallback repeating_callback() {
    return base::BindRepeating(&ThroughputReportChecker::OnReport,
                               base::Unretained(this));
  }
  ReportOnceCallback once_callback() {
    return base::BindOnce(&ThroughputReportChecker::OnReport,
                          base::Unretained(this));
  }

  // It waits until reported up to 5 seconds timeout. Returns true if it's
  // reported.
  bool WaitUntilReported();

 private:
  void OnReport(const cc::FrameSequenceMetrics::CustomReportData&);

  raw_ptr<AnimationThroughputReporterTestBase> test_base_;
  bool reported_ = false;
  bool fail_if_reported_ = false;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_THROUGHPUT_REPORT_CHECKER_H_
