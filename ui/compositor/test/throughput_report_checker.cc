// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/throughput_report_checker.h"

#include "base/time/time.h"
#include "ui/compositor/test/animation_throughput_reporter_test_base.h"

namespace ui {

bool ThroughputReportChecker::WaitUntilReported() {
  DCHECK(!reported_);
  test_base_->Advance(base::Seconds(5));
  return reported_;
}

void ThroughputReportChecker::OnReport(
    const cc::FrameSequenceMetrics::CustomReportData&) {
  reported_ = true;
  if (fail_if_reported_)
    ADD_FAILURE() << "It should not be reported.";
  test_base_->QuitRunLoop();
}

}  // namespace ui
