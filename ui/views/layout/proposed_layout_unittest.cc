// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/proposed_layout.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

namespace views {

TEST(ProposedLayoutTest, ProposedLayoutBetween_SimpleInterpolation) {
  View view1;
  View view2;
  const ProposedLayout kLayout1{
      {10, 20}, {{&view1, true, {3, 4, 5, 6}}, {&view2, true, {1, 12, 7, 8}}}};
  const ProposedLayout kLayout2{
      {20, 10}, {{&view1, true, {3, 4, 5, 6}}, {&view2, true, {11, 2, 3, 2}}}};

  const ProposedLayout kExpected{
      {15, 15}, {{&view1, true, {3, 4, 5, 6}}, {&view2, true, {6, 7, 5, 5}}}};
  EXPECT_EQ(kExpected, ProposedLayoutBetween(0.5, kLayout1, kLayout2));
}

TEST(ProposedLayoutTest, ProposedLayoutBetween_OutOfOrder) {
  View view1;
  View view2;
  const ProposedLayout kLayout1{
      {10, 20}, {{&view2, true, {1, 12, 7, 8}}, {&view1, true, {3, 4, 5, 6}}}};
  const ProposedLayout kLayout2{
      {20, 10}, {{&view1, true, {3, 4, 5, 6}}, {&view2, true, {11, 2, 3, 2}}}};

  const ProposedLayout kExpected{
      {15, 15}, {{&view1, true, {3, 4, 5, 6}}, {&view2, true, {6, 7, 5, 5}}}};
  EXPECT_EQ(kExpected, ProposedLayoutBetween(0.5, kLayout1, kLayout2));
}

TEST(ProposedLayoutTest, ProposedLayoutBetween_ViewBecomesInvisible) {
  View view1;
  View view2;
  const ProposedLayout kLayout1{
      {10, 20}, {{&view1, true, {3, 4, 5, 6}}, {&view2, true, {1, 12, 7, 8}}}};
  const ProposedLayout kLayout2{
      {20, 10}, {{&view1, true, {3, 4, 5, 6}}, {&view2, false}}};

  const ProposedLayout kExpected{
      {15, 15}, {{&view1, true, {3, 4, 5, 6}}, {&view2, false}}};
  EXPECT_EQ(kExpected, ProposedLayoutBetween(0.5, kLayout1, kLayout2));
}

TEST(ProposedLayoutTest, ProposedLayoutBetween_ViewBecomesVisible) {
  View view1;
  View view2;
  const ProposedLayout kLayout1{
      {10, 20}, {{&view1, true, {3, 4, 5, 6}}, {&view2, false, {1, 12, 7, 8}}}};
  const ProposedLayout kLayout2{
      {20, 10}, {{&view1, true, {3, 4, 5, 6}}, {&view2, true, {11, 2, 3, 2}}}};

  const ProposedLayout kExpected{
      {15, 15}, {{&view1, true, {3, 4, 5, 6}}, {&view2, false}}};
  EXPECT_EQ(kExpected, ProposedLayoutBetween(0.5, kLayout1, kLayout2));
}

TEST(ProposedLayoutTest, ProposedLayoutBetween_ViewAdded) {
  View view1;
  View view2;
  const ProposedLayout kLayout1{{10, 20}, {{&view1, true, {3, 4, 5, 6}}}};
  const ProposedLayout kLayout2{
      {20, 10}, {{&view1, true, {3, 4, 5, 6}}, {&view2, true, {11, 2, 3, 2}}}};

  const ProposedLayout kExpected{
      {15, 15}, {{&view1, true, {3, 4, 5, 6}}, {&view2, false}}};
  EXPECT_EQ(kExpected, ProposedLayoutBetween(0.5, kLayout1, kLayout2));
}

TEST(ProposedLayoutTest, ProposedLayoutBetween_ViewRemoved) {
  View view1;
  View view2;
  const ProposedLayout kLayout1{
      {10, 20}, {{&view1, true, {3, 4, 5, 6}}, {&view2, true, {1, 12, 7, 8}}}};
  const ProposedLayout kLayout2{{20, 10}, {{&view1, true, {3, 4, 5, 6}}}};

  const ProposedLayout kExpected{{15, 15}, {{&view1, true, {3, 4, 5, 6}}}};
  EXPECT_EQ(kExpected, ProposedLayoutBetween(0.5, kLayout1, kLayout2));
}

TEST(ProposedLayoutTest, AvailableSizeBoundsInterpolate) {
  View view1;
  const ProposedLayout kLayout1{{10, 20},
                                {{&view1, true, {3, 4, 5, 6}, {4, 8}}}};
  const ProposedLayout kLayout2{{20, 10},
                                {{&view1, true, {3, 4, 5, 6}, {10, 2}}}};
  const ProposedLayout result = ProposedLayoutBetween(0.5, kLayout1, kLayout2);
  EXPECT_EQ(SizeBounds(7, 5), result.child_layouts[0].available_size);
}

}  // namespace views
