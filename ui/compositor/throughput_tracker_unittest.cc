// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/animation_throughput_reporter_test_base.h"
#include "ui/compositor/test/throughput_report_checker.h"

namespace ui {

using ThroughputReporterTest = AnimationThroughputReporterTestBase;

TEST_F(ThroughputReporterTest, ThreadCheck) {
  Layer layer;
  root_layer()->Add(&layer);

  LayerAnimator* animator = new LayerAnimator(base::Milliseconds(32));
  layer.SetAnimator(animator);

  ThroughputReportChecker checker(this);
  auto once_callback = checker.once_callback();

  ui::Compositor* c = compositor();
  auto callback = [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
    c->ScheduleDraw();
    std::move(once_callback).Run(data);
  };

  auto tracker = c->RequestNewThroughputTracker();
  tracker.Start(base::BindLambdaForTesting(callback));
  tracker.Stop();
  EXPECT_TRUE(checker.WaitUntilReported());
}

}  // namespace ui
