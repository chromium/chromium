// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_mode.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(AXModeTest, HasMode) {
  EXPECT_FALSE(AXMode().has_mode(AXMode::kNativeAPIs));
  EXPECT_TRUE(AXMode(AXMode::kNativeAPIs).has_mode(AXMode::kNativeAPIs));
  EXPECT_TRUE(kAXModeBasic.has_mode(AXMode::kNativeAPIs));
  EXPECT_TRUE(kAXModeBasic.has_mode(kAXModeBasic.flags()));
  EXPECT_TRUE(kAXModeComplete.has_mode(kAXModeBasic.flags()));
}

TEST(AXModeTest, SetMode) {
  AXMode mode;

  mode.set_mode(AXMode::kNativeAPIs, true);
  EXPECT_EQ(mode, AXMode(AXMode::kNativeAPIs));
  mode.set_mode(AXMode::kNativeAPIs, true);
  EXPECT_EQ(mode, AXMode(AXMode::kNativeAPIs));

  mode.set_mode(AXMode::kNativeAPIs, false);
  EXPECT_EQ(mode, AXMode());

  mode.set_mode(kAXModeBasic.flags(), true);
  EXPECT_EQ(mode, kAXModeBasic);
  mode.set_mode(kAXModeWebContentsOnly.flags(), true);
  EXPECT_EQ(mode, kAXModeBasic | kAXModeWebContentsOnly);
  mode.set_mode(kAXModeWebContentsOnly.flags(), false);
  EXPECT_EQ(mode, kAXModeBasic & ~kAXModeWebContentsOnly);
}

TEST(AXModeTest, OrEquals) {
  AXMode mode;

  mode |= kAXModeBasic;
  EXPECT_EQ(mode, kAXModeBasic);
  ASSERT_FALSE(mode.has_mode(kAXModeWebContentsOnly.flags()));
  mode |= kAXModeWebContentsOnly;
  EXPECT_TRUE(mode.has_mode(kAXModeWebContentsOnly.flags()));
  mode |= kAXModeFormControls;
  EXPECT_TRUE(mode.experimental_flags() & AXMode::kExperimentalFormControls);
}

TEST(AXModeTest, AndEquals) {
  AXMode mode;

  mode &= kAXModeBasic;
  EXPECT_EQ(mode, AXMode());

  mode = kAXModeBasic;
  mode &= kAXModeBasic;
  EXPECT_EQ(mode, kAXModeBasic);

  mode = AXMode(AXMode::kWebContents | AXMode::kInlineTextBoxes);
  mode &= AXMode(AXMode::kNativeAPIs | AXMode::kWebContents);
  EXPECT_EQ(mode, AXMode(AXMode::kWebContents));
}

TEST(AXModeTest, Invert) {
  AXMode mode = ~AXMode();

  EXPECT_EQ(mode.flags(), ~uint32_t(0));
  EXPECT_EQ(mode.experimental_flags(), ~uint32_t(0));

  mode = ~kAXModeBasic;
  EXPECT_FALSE(mode.has_mode(AXMode::kNativeAPIs));
  EXPECT_FALSE(mode.has_mode(AXMode::kWebContents));
  EXPECT_TRUE(mode.has_mode(AXMode::kInlineTextBoxes));
}

TEST(AXModeTest, Equality) {
  EXPECT_TRUE(kAXModeBasic == kAXModeBasic);
  EXPECT_FALSE(kAXModeBasic == kAXModeComplete);
  EXPECT_TRUE(AXMode(AXMode::kNone, AXMode::kExperimentalFormControls) ==
              AXMode(AXMode::kNone, AXMode::kExperimentalFormControls));
  EXPECT_FALSE(AXMode(AXMode::kNativeAPIs, AXMode::kExperimentalFormControls) ==
               AXMode(AXMode::kNone, AXMode::kExperimentalFormControls));
}

TEST(AXModeTest, Inequality) {
  EXPECT_FALSE(kAXModeBasic != kAXModeBasic);
  EXPECT_TRUE(kAXModeBasic != kAXModeComplete);
  EXPECT_FALSE(AXMode(AXMode::kNone, AXMode::kExperimentalFormControls) !=
               AXMode(AXMode::kNone, AXMode::kExperimentalFormControls));
  EXPECT_TRUE(AXMode(AXMode::kNativeAPIs, AXMode::kExperimentalFormControls) !=
              AXMode(AXMode::kNone, AXMode::kExperimentalFormControls));
}

TEST(AXModeTest, Or) {
  EXPECT_EQ(kAXModeBasic,
            AXMode(AXMode::kNativeAPIs) | AXMode(AXMode::kWebContents));
  EXPECT_EQ(kAXModeBasic, AXMode() | kAXModeBasic);
  EXPECT_EQ(kAXModeFormControls, AXMode() | kAXModeFormControls);
}

TEST(AXModeTest, And) {
  EXPECT_EQ(kAXModeBasic,
            AXMode(AXMode::kNativeAPIs | AXMode::kWebContents) &
                AXMode(AXMode::kNativeAPIs | AXMode::kWebContents));
  EXPECT_EQ(AXMode(), kAXModeBasic & AXMode());
  EXPECT_EQ(kAXModeFormControls, AXMode(std::numeric_limits<uint32_t>::max(),
                                        std::numeric_limits<uint32_t>::max()) &
                                     kAXModeFormControls);
}

}  // namespace ui
