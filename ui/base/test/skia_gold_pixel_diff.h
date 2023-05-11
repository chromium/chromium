// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SKIA_GOLD_PIXEL_DIFF_H_
#define UI_BASE_TEST_SKIA_GOLD_PIXEL_DIFF_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"

namespace base {
class CommandLine;
}

class SkBitmap;

namespace ui {
namespace test {

// Keys used to identify test environment information that can possibly affect
// pixel output. If your test depends on some new environment information, add
// it here and update |TestEnvironmentKeyToString|.
enum class TestEnvironmentKey {
  // Being default-provided keys. These should not by used by subtypes.

  // The operating system name.
  kSystem,
  // The processor architecture.
  kProcessor,

  // Begin subtype-provided keys.\

  // The version of the operating system.
  // On Windows, this is the release ID string and is used to track the DWM
  // version.
  kSystemVersion,

  // The vendor name of the GPU used for ths test.
  kGpuDriverVendor,

  // The GPU driver version.
  kGpuDriverVersion,

  // The GL_RENDERER string returned from the GLContext.
  kGlRenderer,
};

using TestEnvironmentMap = base::flat_map<TestEnvironmentKey, std::string>;

class SkiaGoldMatchingAlgorithm;
// This is the utility class for Skia Gold pixeltest.
class SkiaGoldPixelDiff {
 public:
  SkiaGoldPixelDiff();

  SkiaGoldPixelDiff(const SkiaGoldPixelDiff&) = delete;
  SkiaGoldPixelDiff& operator=(const SkiaGoldPixelDiff&) = delete;

  virtual ~SkiaGoldPixelDiff();

  // Returns the platform used to generate this image. It is appended to the
  // golden name of the images uploaded to the Skia Gold Server. This is pubic
  // to be used in tests.
  static std::string GetPlatform();

  // Call Init method before using this class.
  // Args:
  // screenshot_prefix The prefix for your screenshot name on GCS.
  //   For every screenshot you take, it should have a unique name
  //   across Chromium, because all screenshots (aka golden images) stores
  //   in one bucket on GCS. The standard convention is to use the browser
  //   test class name as the prefix. The name will be
  //   |screenshot_prefix| + "_" + |screenshot_name|.'
  //   E.g. 'ToolbarTest_BackButtonHover'.
  // corpus The corpus (i.e. result group) that will be used to store the
  //   result in Gold. If omitted, will default to the generic corpus for
  //   results from gtest-based tests.
  // test_environment A map containing any test-specific environment information
  //   to determine whether a new screenshot is good or not. All the information
  //   that can affect the output of pixels should be filled in. Eg: operating
  //   system, graphics card, processor architecture, screen resolution, etc.
  void Init(const std::string& screenshot_prefix,
            const std::string& corpus = std::string(),
            TestEnvironmentMap test_environment = TestEnvironmentMap());

  bool CompareScreenshot(
      const std::string& screenshot_name,
      const SkBitmap& bitmap,
      const SkiaGoldMatchingAlgorithm* algorithm = nullptr) const;

 protected:
  // Upload the local file to Skia Gold server. Return true if the screenshot
  // is the same as the remote golden image.
  virtual bool UploadToSkiaGoldServer(
      const base::FilePath& local_file_path,
      const std::string& remote_golden_image_name,
      const SkiaGoldMatchingAlgorithm* algorithm) const;

  virtual int LaunchProcess(const base::CommandLine& cmdline) const;
  bool Initialized() const { return initialized_; }

 private:
  void InitSkiaGold(TestEnvironmentMap test_environment);
  // Prefix for every golden images.
  std::string prefix_;
  bool initialized_ = false;
  // Use luci auth on bots. Don't use luci auth for local development.
  bool luci_auth_ = true;
  // Which corpus in the instance to associate results with.
  std::string corpus_;
  // Build revision. This is only used for CI run.
  std::string build_revision_;
  // The following 3 members are for tryjob run.
  // Chagnelist issue id.
  std::string issue_;
  // Which patchset for a changelist.
  std::string patchset_;
  // Buildbucket build id.
  std::string job_id_;
  // Which code review system is being used, typically "gerrit" for Chromium
  // and "gerrit-internal" for Chrome.
  std::string code_review_system_;
  // The working dir for goldctl. It's the dir for storing temporary files.
  base::FilePath working_dir_;
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_SKIA_GOLD_PIXEL_DIFF_H_
