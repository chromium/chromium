// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/page_transition_types.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(PageTransitionTypesTest, PageTransitionCoreTypeIs) {
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(PAGE_TRANSITION_TYPED, PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(PageTransitionCoreTypeIs(PAGE_TRANSITION_TYPED,
                                        PAGE_TRANSITION_AUTO_BOOKMARK));
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      PageTransitionFromInt(PAGE_TRANSITION_TYPED |
                            PAGE_TRANSITION_SERVER_REDIRECT),
      PAGE_TRANSITION_TYPED));
}

TEST(PageTransitionTypesTest, PageTransitionTypeIncludingQualifiersIs) {
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(PAGE_TRANSITION_TYPED,
                                                      PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(PageTransitionTypeIncludingQualifiersIs(PAGE_TRANSITION_TYPED,
                                                       PAGE_TRANSITION_LINK));
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      PageTransitionFromInt(PAGE_TRANSITION_TYPED |
                            PAGE_TRANSITION_SERVER_REDIRECT),
      PageTransitionFromInt(PAGE_TRANSITION_TYPED |
                            PAGE_TRANSITION_SERVER_REDIRECT)));
  EXPECT_FALSE(PageTransitionTypeIncludingQualifiersIs(
      PageTransitionFromInt(PAGE_TRANSITION_TYPED |
                            PAGE_TRANSITION_SERVER_REDIRECT),
      PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(PageTransitionTypeIncludingQualifiersIs(
      PAGE_TRANSITION_TYPED,
      PageTransitionFromInt(PAGE_TRANSITION_TYPED |
                            PAGE_TRANSITION_SERVER_REDIRECT)));
}

TEST(PageTransitionTypesTest, PageTransitionStripQualifier) {
  typedef int32_t underlying_type;

  EXPECT_EQ(static_cast<underlying_type>(
                PageTransitionStripQualifier(PAGE_TRANSITION_TYPED)),
            static_cast<underlying_type>(PAGE_TRANSITION_TYPED));
  EXPECT_EQ(static_cast<underlying_type>(PageTransitionStripQualifier(
      PageTransitionFromInt(PAGE_TRANSITION_TYPED |
                            PAGE_TRANSITION_SERVER_REDIRECT))),
            static_cast<underlying_type>(PAGE_TRANSITION_TYPED));
}

}  // namespace ui
