// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/delegated_ink_point.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

// Test confirms that DelegatedInkPoint and DelegatedInkMetadata will both
// report that they match each other if the timestamp and point are the same,
// regardless of any other members.
TEST(DelegatedInkTest, PointAndMetadataMatch) {
  const gfx::PointF point(10, 40);
  const base::TimeTicks timestamp = base::TimeTicks::Now();

  DelegatedInkPoint ink_point(point, timestamp, /*pointer_id=*/1);
  DelegatedInkMetadata metadata(point, /*diameter=*/10, SK_ColorBLACK,
                                timestamp, gfx::RectF(10, 10, 100, 100),
                                /*hovering=*/false);

  EXPECT_TRUE(ink_point.MatchesDelegatedInkMetadata(&metadata));

  DelegatedInkPoint point_no_pointer_id(point, timestamp);

  EXPECT_TRUE(point_no_pointer_id.MatchesDelegatedInkMetadata(&metadata));

  DelegatedInkMetadata metadata_with_frame_time(
      point, /*diameter=*/7.777f, SK_ColorCYAN, timestamp,
      gfx::RectF(6, 14, 240, 307), base::TimeTicks::Now(), /*hovering=*/true,
      /*render_pass_id=*/0);

  EXPECT_TRUE(ink_point.MatchesDelegatedInkMetadata(&metadata_with_frame_time));
  EXPECT_TRUE(point_no_pointer_id.MatchesDelegatedInkMetadata(
      &metadata_with_frame_time));
}

// Confirms that if the timestamps are the same and point locations are within
// |kEpsilon|, then they will be considered matching.
TEST(DelegatedInkTest, PointAndMetadataAreClose) {
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::PointF point_location(34, 95.002f);
  const gfx::PointF metadata_location(33.994f, 94.9524f);

  DelegatedInkPoint ink_point(point_location, timestamp);
  DelegatedInkMetadata metadata(
      metadata_location, /*diameter=*/3.5f, SK_ColorRED, timestamp,
      gfx::RectF(0, 3, 14.6f, 78.2f), /*hovering=*/false);

  EXPECT_TRUE(ink_point.MatchesDelegatedInkMetadata(&metadata));
}

// Confirms that if timestamps or points are different (or the metadata is null)
// the DelegatedInkPoint and DelegatedInkMetadata are not considered matching.
TEST(DelegatedInkTest, PointAndMetadataDoNotMatch) {
  const base::TimeTicks timestamp = base::TimeTicks::Now();
  const gfx::PointF point_location(34, 95.002f);

  DelegatedInkPoint ink_point(point_location, timestamp);
  EXPECT_FALSE(ink_point.MatchesDelegatedInkMetadata(nullptr));

  const gfx::PointF metadata_location_within_tolerance(33.0f, 94.002f);
  DelegatedInkMetadata close_metadata(
      metadata_location_within_tolerance, /*diameter=*/5, SK_ColorWHITE,
      timestamp, gfx::RectF(40, 30.6f, 43, 7.2f), /*hovering=*/true);
  EXPECT_TRUE(ink_point.MatchesDelegatedInkMetadata(&close_metadata));

  const gfx::PointF metadata_location_outside_tolerance(32.999f, 94.0f);
  DelegatedInkMetadata just_outside_metadata(
      metadata_location_outside_tolerance, /*diameter=*/5, SK_ColorWHITE,
      timestamp, gfx::RectF(40, 30.6f, 43, 7.2f), /*hovering=*/true);
  EXPECT_FALSE(ink_point.MatchesDelegatedInkMetadata(&just_outside_metadata));

  const gfx::PointF metadata_location_far(23.789f, 20);
  DelegatedInkMetadata far_metadata(
      metadata_location_far, /*diameter=*/1, SK_ColorYELLOW, timestamp,
      gfx::RectF(12.44f, 3, 1000, 1000.1f), /*hovering=*/false);
  EXPECT_FALSE(ink_point.MatchesDelegatedInkMetadata(&far_metadata));

  DelegatedInkMetadata metadata_different_timestamp(
      point_location, /*diameter=*/10, SK_ColorGREEN,
      timestamp + base::Milliseconds(10), gfx::RectF(0, 0, 0, 0),
      /*hovering*/ true);
  EXPECT_FALSE(
      ink_point.MatchesDelegatedInkMetadata(&metadata_different_timestamp));
}

}  // namespace gfx
