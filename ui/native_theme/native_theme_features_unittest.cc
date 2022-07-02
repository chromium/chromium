// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_features.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

// The unit test verifies that the Fluent scrollbar feature is enabled
// on Windows when both overlay and fluent scrollbar base features are enabled.
TEST(FluentScrollbarFeatureStateTest, Enabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kOverlayScrollbar, features::kFluentScrollbar}, {});

  EXPECT_TRUE(IsOverlayScrollbarEnabled());
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(IsFluentScrollbarEnabled());
#else
  EXPECT_FALSE(IsFluentScrollbarEnabled());
#endif
}

// The unit test verifies that the Fluent scrollbar feature is disabled when
// kOverlayScrollbar base feature is disabled no matter the state of
// kFluentScrollbar.
TEST(FluentScrollbarFeatureStateTest, Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kFluentScrollbar},
                                       {features::kOverlayScrollbar});

  EXPECT_FALSE(IsOverlayScrollbarEnabled());
  EXPECT_FALSE(IsFluentScrollbarEnabled());
}

// The unit test verifies that the Fluent scrollbar feature is disabled
// by default on all platforms.
TEST(FluentScrollbarFeatureStateTest, DisabledByDefault) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(features::kFluentScrollbar));
  EXPECT_FALSE(IsFluentScrollbarEnabled());
}

}  // namespace ui
