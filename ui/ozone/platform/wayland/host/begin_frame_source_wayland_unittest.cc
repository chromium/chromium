// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/begin_frame_source_wayland.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

// Common vsync intervals as compositors typically report them via
// wp_presentation feedback, which can differ slightly from the nominal values
// in viz.
constexpr base::TimeDelta k60Hz = base::Microseconds(16667);
constexpr base::TimeDelta k120Hz = base::Microseconds(8333);
constexpr base::TimeDelta k144Hz = base::Microseconds(6944);
constexpr base::TimeDelta k165Hz = base::Microseconds(6061);
constexpr base::TimeDelta k240Hz = base::Microseconds(4166);

base::TimeDelta Compute(base::TimeDelta preferred, base::TimeDelta vsync) {
  return BeginFrameSourceWayland::ComputeEffectiveInterval(preferred, vsync);
}

}  // namespace

// 60fps content on a 60hz display.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     VsyncPreferenceReturnsVsync) {
  EXPECT_EQ(k60Hz, Compute(k60Hz, k60Hz));
}
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     EvenlyDivisibleRatesAreSubsampled) {
  EXPECT_EQ(2 * k60Hz, Compute(base::Microseconds(33334), k60Hz));
  EXPECT_EQ(5 * k120Hz, Compute(base::Microseconds(41667), k120Hz));
  EXPECT_EQ(6 * k144Hz, Compute(base::Microseconds(41667), k144Hz));
  EXPECT_EQ(8 * k240Hz, Compute(base::Microseconds(33334), k240Hz));
  EXPECT_EQ(4 * k240Hz, Compute(base::Microseconds(16667), k240Hz));
}

// Close matches

// 23.976fps (NTSC) content on a 120Hz display is treated as 24fps.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, NtscSnapsToNearestGrid) {
  EXPECT_EQ(5 * k120Hz, Compute(base::Microseconds(41708), k120Hz));
}
// Viz asks for a 16666us interval (60Hz) but the 60Hz display reports 16667us.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     AlignsWithMeasuredVsyncGrid60Hz) {
  EXPECT_EQ(k60Hz, Compute(base::Microseconds(16666), k60Hz));
}
// Viz asks for a 33332us interval (30Hz) but the 60Hz display reports 16667us.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     AlignsWithMeasuredVsyncGrid30Hz) {
  EXPECT_EQ(2 * k60Hz, Compute(base::Microseconds(33332), k60Hz));
}

// A request within kDeltaAlmostEqual (10us) per vsync of a whole multiple
// still counts as that multiple; anything further under floors to the next
// faster grid rate.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, ToleranceBoundary) {
  EXPECT_EQ(2 * k60Hz, Compute(2 * k60Hz - base::Microseconds(20), k60Hz));
  EXPECT_EQ(k60Hz, Compute(2 * k60Hz - base::Microseconds(21), k60Hz));
}

TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     NonDivisibleRatesAreFlooredToNextFasterGrid) {
  EXPECT_EQ(2 * k60Hz, Compute(base::Microseconds(41667), k60Hz));
  EXPECT_EQ(2 * k144Hz, Compute(base::Microseconds(16667), k144Hz));
  EXPECT_EQ(2 * k165Hz, Compute(base::Microseconds(16667), k165Hz));
}

// Edge cases/caps

// No preference was set or it was invalidated.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, NoPreferenceIsFullRate) {
  EXPECT_EQ(k60Hz, Compute(base::TimeDelta(), k60Hz));
}
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, NoVsyncReturnsVsync) {
  EXPECT_EQ(base::TimeDelta(), Compute(k60Hz, base::TimeDelta()));
}
// A preference that is too fast maxes at the display's full rate.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     FasterThanDisplayIsFullRate) {
  EXPECT_EQ(k60Hz, Compute(k120Hz, k60Hz));
}
// Negative preferences are discarded.
TEST(BeginFrameSourceWaylandEffectiveIntervalTest,
     NegativePreferenceIsFullRate) {
  EXPECT_EQ(k60Hz, Compute(base::Microseconds(-16667), k60Hz));
  EXPECT_EQ(k60Hz, Compute(base::Microseconds(-4000), k60Hz));
}
// Slow preferences are capped at the nearest grid rate not slower than
// kMaxEffectiveInterval (24fps).
TEST(BeginFrameSourceWaylandEffectiveIntervalTest, SlowPreferenceIsCapped) {
  EXPECT_EQ(2 * k60Hz, Compute(base::Seconds(1), k60Hz));
  EXPECT_EQ(10 * k240Hz, Compute(base::Seconds(1), k240Hz));
}

}  // namespace ui
