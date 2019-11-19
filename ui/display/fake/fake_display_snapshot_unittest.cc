// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/fake/fake_display_snapshot.h"

#include <memory>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/test/display_matchers.h"
#include "ui/display/types/display_snapshot.h"

using testing::SizeIs;

namespace display {

using DisplayModeList = DisplaySnapshot::DisplayModeList;

namespace {

std::unique_ptr<DisplaySnapshot> CreateSnapshot(const std::string& str) {
  return FakeDisplaySnapshot::CreateFromSpec(1, str);
}

}  // namespace

TEST(FakeDisplaySnapshotTest, SizeOnly) {
  auto display = CreateSnapshot("1024x768");

  ASSERT_THAT(display->modes(), SizeIs(1));
  EXPECT_THAT(*display->native_mode(), IsDisplayMode(1024, 768));
}

TEST(FakeDisplaySnapshotTest, DefaultTypeIsUnknown) {
  auto display = CreateSnapshot("1024x768");

  ASSERT_THAT(display->modes(), SizeIs(1));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_UNKNOWN, display->type());
}

TEST(FakeDisplaySnapshotTest, FullNativeMode) {
  auto display = CreateSnapshot("1024x768%120");

  ASSERT_THAT(display->modes(), SizeIs(1));
  EXPECT_THAT(*display->native_mode(), IsDisplayMode(1024, 768, 120.0f));
}

TEST(FakeDisplaySnapshotTest, FullNativeModeWithDPI) {
  auto display = CreateSnapshot("1000x1000%120^300");

  ASSERT_THAT(display->modes(), SizeIs(1));
  EXPECT_THAT(*display->native_mode(), IsDisplayMode(1000, 1000, 120.0f));
  EXPECT_EQ(85, display->physical_size().width());
  EXPECT_EQ(85, display->physical_size().height());
}

TEST(FakeDisplaySnapshotTest, InternalDisplayWithSize) {
  auto display = CreateSnapshot("1600x900/i");

  ASSERT_THAT(display->modes(), SizeIs(1));
  EXPECT_THAT(*display->native_mode(), IsDisplayMode(1600, 900));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL, display->type());
}

TEST(FakeDisplaySnapshotTest, MultipleOptions) {
  auto display = CreateSnapshot("1600x900/aci");

  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL, display->type());
  EXPECT_TRUE(display->has_color_correction_matrix());
  EXPECT_TRUE(display->is_aspect_preserving_scaling());
}

TEST(FakeDisplaySnapshotTest, AlternateDisplayModes) {
  auto display = CreateSnapshot("1920x1080#1600x900:1280x720/i");
  const DisplayModeList& modes = display->modes();

  ASSERT_THAT(display->modes(), SizeIs(3));
  EXPECT_THAT(*modes[0], IsDisplayMode(1920, 1080));
  EXPECT_THAT(*modes[1], IsDisplayMode(1600, 900));
  EXPECT_THAT(*modes[2], IsDisplayMode(1280, 720));
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL, display->type());
}

TEST(FakeDisplaySnapshotTest, ComplicatedSpecString) {
  auto display =
      CreateSnapshot("1920x1080%59.99#1600x900%90:1280x720%120^300/i");
  const DisplayModeList& modes = display->modes();

  ASSERT_THAT(display->modes(), SizeIs(3));
  EXPECT_THAT(*modes[0], IsDisplayMode(1920, 1080, 59.99f));
  EXPECT_THAT(*modes[1], IsDisplayMode(1600, 900, 90.0f));
  EXPECT_THAT(*modes[2], IsDisplayMode(1280, 720, 120.0f));
  EXPECT_EQ(163, display->physical_size().width());
  EXPECT_EQ(91, display->physical_size().height());
  EXPECT_EQ(DISPLAY_CONNECTION_TYPE_INTERNAL, display->type());
}

TEST(FakeDisplaySnapshotTest, BadDisplayMode) {
  // Need width and height.
  EXPECT_EQ(nullptr, CreateSnapshot("1024"));
  // Display height and width should be separated by 'x' not ','.
  EXPECT_EQ(nullptr, CreateSnapshot("1024,768"));
  // Random 'a' before spec starts.
  EXPECT_EQ(nullptr, CreateSnapshot("a1024,768"));
  // Need to provide a refresh rate after '%'.
  EXPECT_EQ(nullptr, CreateSnapshot("1024,768%"));
  EXPECT_EQ(nullptr, CreateSnapshot("1024,768%a"));
  // Display resolution can't be zero for width or height.
  EXPECT_EQ(nullptr, CreateSnapshot("000x800"));
  EXPECT_EQ(nullptr, CreateSnapshot("800x000"));
  // Refresh rate should come before DPI.
  EXPECT_EQ(nullptr, CreateSnapshot("1000x1000^300%120"));
}

TEST(FakeDisplaySnapshotTest, BadDPI) {
  // DPI should be an integer value.
  EXPECT_EQ(nullptr, CreateSnapshot("1024x768^a"));
  EXPECT_EQ(nullptr, CreateSnapshot("1024x768^300d"));
}

TEST(FakeDisplaySnapshotTest, BadOptions) {
  // Need a '/' before options.
  EXPECT_EQ(nullptr, CreateSnapshot("1024x768i"));
  // Character 'z' is not a valid option.
  EXPECT_EQ(nullptr, CreateSnapshot("1600x900/z"));
  EXPECT_EQ(nullptr, CreateSnapshot("1600x900/iz"));
  // DPI should come before options.
  EXPECT_EQ(nullptr, CreateSnapshot("1600x900/i^300"));
}

TEST(FakeDisplaySnapshotTest, BadOrderOptionsAndModes) {
  // Options should come after alternate display modes.
  auto display = CreateSnapshot("1920x1080/i#1600x900:1280x720");
  EXPECT_EQ(nullptr, display);
}

TEST(FakeDisplaySnapshotTest, BadOrderModeSeparator) {
  // Reverse the '#' and ':' delimiters for alternate display modes.
  auto display = CreateSnapshot("1920x1080:1600x900#1280x720");
  EXPECT_EQ(nullptr, display);
}

}  // namespace display
