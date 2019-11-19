// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SKIA_GOLD_PIXEL_DIFF_H_
#define UI_BASE_TEST_SKIA_GOLD_PIXEL_DIFF_H_

#include <string>

#include "base/files/file_path.h"

namespace base {
class CommandLine;
}

class SkBitmap;

// This is the utility class for Skia Gold pixeltest.
class SkiaGoldPixelDiff {
 public:
  SkiaGoldPixelDiff();
  virtual ~SkiaGoldPixelDiff();
  // Call Init method before using this class.
  // Args:
  // screenshot_prefix The prefix for your screenshot name on GCS.
  //   For every screenshot you take, it should have a unique name
  //   across Chromium, because all screenshots (aka golden images) stores
  //   in one bucket on GCS. The standard convention is to use the browser
  //   test class name as the prefix. The name will be
  //   |screenshot_prefix| + "_" + |screenshot_name|.'
  //   E.g. 'ToolbarTest_BackButtonHover'.
  void Init(const std::string& screenshot_prefix);

  bool CompareScreenshot(const std::string& screenshot_name,
                         const SkBitmap& bitmap) const;

 protected:
  // Upload the local file to Skia Gold server. Return true if the screenshot
  // is the same as the remote golden image.
  virtual bool UploadToSkiaGoldServer(
      const base::FilePath& local_file_path,
      const std::string& remote_golden_image_name) const;

  virtual int LaunchProcess(const base::CommandLine& cmdline) const;
  bool Initialized() const { return initialized_; }

 private:
  void InitSkiaGold();
  // Prefix for every golden images.
  std::string prefix_;
  bool initialized_ = false;
  // Use luci auth on bots. Don't use luci auth for local development.
  bool luci_auth_ = true;
  // Build revision. This is only used for CI run.
  std::string build_revision_;
  // The following 3 members are for tryjob run.
  // Chagnelist issue id.
  std::string issue_;
  // Which patchset for a changelist.
  std::string patchset_;
  // Buildbucket build id.
  std::string job_id_;
  // The working dir for goldctl. It's the dir for storing temporary files.
  base::FilePath working_dir_;

  DISALLOW_COPY_AND_ASSIGN(SkiaGoldPixelDiff);
};

#endif  // UI_BASE_TEST_SKIA_GOLD_PIXEL_DIFF_H_
