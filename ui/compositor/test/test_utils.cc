// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/test_utils.h"

#include "base/cancelable_callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/timer/timer.h"
#include "components/viz/common/frame_timing_details.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {

// TODO(avallee): Use macros in ui/gfx/geometry/test/geometry_util.h.
void CheckApproximatelyEqual(const gfx::Transform& lhs,
                             const gfx::Transform& rhs) {
  unsigned int errors = 0;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_FLOAT_EQ(lhs.rc(i, j), rhs.rc(i, j))
          << "(i, j) = (" << i << ", " << j << "), error count: " << ++errors;
    }
  }

  if (errors) {
    ADD_FAILURE() << "Expected matrix:\n"
                  << lhs.ToString() << "\n"
                  << "Actual matrix:\n"
                  << rhs.ToString();
  }
}

void CheckApproximatelyEqual(const gfx::Rect& lhs, const gfx::Rect& rhs) {
  EXPECT_FLOAT_EQ(lhs.x(), rhs.x());
  EXPECT_FLOAT_EQ(lhs.y(), rhs.y());
  EXPECT_FLOAT_EQ(lhs.width(), rhs.width());
  EXPECT_FLOAT_EQ(lhs.height(), rhs.height());
}

void CheckApproximatelyEqual(const gfx::RoundedCornersF& lhs,
                             const gfx::RoundedCornersF& rhs) {
  EXPECT_FLOAT_EQ(lhs.upper_left(), rhs.upper_left());
  EXPECT_FLOAT_EQ(lhs.upper_right(), rhs.upper_right());
  EXPECT_FLOAT_EQ(lhs.lower_left(), rhs.lower_left());
  EXPECT_FLOAT_EQ(lhs.lower_right(), rhs.lower_right());
}

bool WaitForNextFrameToBePresented(ui::Compositor* compositor,
                                   std::optional<base::TimeDelta> timeout) {
  bool frames_presented = false;
  base::RunLoop runloop;
  base::CancelableOnceCallback<void(
      const viz::FrameTimingDetails& frame_timing_details)>
      cancelable_callback(base::BindLambdaForTesting(
          [&](const viz::FrameTimingDetails& frame_timing_details) {
            frames_presented = true;
            runloop.Quit();
          }));
  compositor->RequestSuccessfulPresentationTimeForNextFrame(
      cancelable_callback.callback());

  std::optional<base::OneShotTimer> timer;
  if (timeout.has_value()) {
    timer.emplace();
    timer->Start(FROM_HERE, timeout.value(), runloop.QuitClosure());
  }

  runloop.Run();

  return frames_presented;
}

}  // namespace ui
