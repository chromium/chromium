// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_metrics_tracker.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/compositor_metrics_report_checker.h"
#include "ui/compositor/test/compositor_metrics_reporter_test_base.h"

namespace ui {

using CompositorMetricsReporterTest = CompositorMetricsReporterTestBase;

TEST_F(CompositorMetricsReporterTest, ThreadCheck) {
  Layer layer;
  root_layer()->Add(&layer);

  LayerAnimator* animator = new LayerAnimator(base::Milliseconds(32));
  layer.SetAnimator(animator);

  CompositorMetricsReportChecker checker(this);
  auto once_callback = checker.once_callback();

  ui::Compositor* c = compositor();
  auto callback = [&](const cc::FrameSequenceMetrics::CustomReportData& data) {
    c->ScheduleDraw();
    std::move(once_callback).Run(data);
  };

  auto tracker = c->RequestNewCompositorMetricsTracker();
  tracker.Start(base::BindLambdaForTesting(callback));
  tracker.Stop();
  EXPECT_TRUE(checker.WaitUntilReported());
}

}  // namespace ui
