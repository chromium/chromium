// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_pixel_diff.h"

#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

class MockSkiaGoldPixelDiff : public SkiaGoldPixelDiff {
 public:
  MockSkiaGoldPixelDiff() {}
  int LaunchProcess(const base::CommandLine& cmdline) const override {
    return 0;
  }
  bool UploadToSkiaGoldServer(
      const base::FilePath& local_file_path,
      const std::string& remote_golden_image_name) const override {
    return true;
  }
};

class SkiaGoldPixelDiffTest : public ::testing::Test {
 public:
  SkiaGoldPixelDiffTest() {
    auto* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII("build-revision", "test");
  }

  ~SkiaGoldPixelDiffTest() override {}

 protected:
  DISALLOW_COPY_AND_ASSIGN(SkiaGoldPixelDiffTest);
};

TEST_F(SkiaGoldPixelDiffTest, CompareScreenshotBySkBitmap) {
  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);
  MockSkiaGoldPixelDiff mock_pixel;
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}
