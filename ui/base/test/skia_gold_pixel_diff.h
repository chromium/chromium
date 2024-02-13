// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_SKIA_GOLD_PIXEL_DIFF_H_
#define UI_BASE_TEST_SKIA_GOLD_PIXEL_DIFF_H_

#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace base {
class CommandLine;
}

class SkBitmap;

namespace testing {
class TestInfo;
}

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
class SkiaGoldPixelDiff final {
 public:
  // Returns the platform used to generate this image. It is appended to the
  // golden name of the images uploaded to the Skia Gold Server. This is pubic
  // to be used in tests.
  static std::string GetPlatform();

  // Resets the global session cache used by |GetSession| on creation and
  // deletion. Invalidates pointers returned by |GetSession| in both
  // cases. Used to limit reuse of sessions when testing |SkiaGoldPixelDiff|
  // itself.
  class ScopedSessionCacheForTesting {
   public:
    ScopedSessionCacheForTesting();
    ~ScopedSessionCacheForTesting();

    ScopedSessionCacheForTesting(const ScopedSessionCacheForTesting&) = delete;
    ScopedSessionCacheForTesting& operator=(
        const ScopedSessionCacheForTesting&) = delete;
    ScopedSessionCacheForTesting(ScopedSessionCacheForTesting&&) = delete;
    ScopedSessionCacheForTesting& operator=(ScopedSessionCacheForTesting&&) =
        delete;
  };

  // Get or create and initialize a |SkiaGoldPixelDiff| instance with the
  // provided parameters.
  // Args:
  // corpus The corpus (i.e. result group) that will be used to store the
  //   result in Gold. If omitted, will default to the generic corpus for
  //   results from gtest-based tests.
  // test_environment A map containing any test-specific environment information
  //   to determine whether a new screenshot is good or not. All the information
  //   that can affect the output of pixels should be filled in. Eg: operating
  //   system, graphics card, processor architecture, screen resolution, etc.
  static SkiaGoldPixelDiff* GetSession(
      const std::optional<std::string>& corpus = {},
      TestEnvironmentMap test_environment = TestEnvironmentMap());

  // Allows passing in a mocked |LaunchProcessCallback|, which is reset when the
  // return value is destructed.
  using LaunchProcessCallback =
      base::RepeatingCallback<int(const base::CommandLine&)>;
  static base::AutoReset<LaunchProcessCallback> OverrideLaunchProcessForTesting(
      LaunchProcessCallback custom_launch_process);

  SkiaGoldPixelDiff(const SkiaGoldPixelDiff&) = delete;
  SkiaGoldPixelDiff& operator=(const SkiaGoldPixelDiff&) = delete;

  ~SkiaGoldPixelDiff();

  // Generate a golden image name from the test suite and test name from
  // |test_info| with an optional suffix. If the test is parameterized, the
  // parameterization with be added after the test name and before the suffix.
  // The result will be something like |{TestSuiteName}_{TestName}_{Suffix}|.
  static std::string GetGoldenImageName(
      const std::string& test_suite_name,
      const std::string& test_name,
      const std::optional<std::string>& suffix = {});

  static std::string GetGoldenImageName(
      const ::testing::TestInfo* test_info,
      const std::optional<std::string>& suffix = {});

  // golden_image_name
  //   For every screenshot you take, it should have a unique name across
  //   Chromium. This is because Skia Gold primarily uses the image name to
  //   determine which golden images to compare against. The standard convention
  //   is to use the test suite name and test name, e.g.
  //   `ToolbarTest_BackButtonHover`. See |GetGoldenImageName|.
  bool CompareScreenshot(
      const std::string& golden_image_name,
      const SkBitmap& bitmap,
      const SkiaGoldMatchingAlgorithm* algorithm = nullptr) const;

 private:
  SkiaGoldPixelDiff();

  void Init(const std::string& corpus, TestEnvironmentMap test_environment);

  void InitSkiaGold() const;

  // Upload the local file to Skia Gold server. Return true if the screenshot
  // is the same as the remote golden image.
  bool UploadToSkiaGoldServer(const base::FilePath& local_file_path,
                              const std::string& remote_golden_image_name,
                              const SkiaGoldMatchingAlgorithm* algorithm) const;

  // Calculate a diff and save locally to aid in local development.
  void GenerateLocalDiff(const std::string& remote_golden_image_name,
                         const base::FilePath& test_output_path) const;

  bool initialized_ = false;
  // True if bot mode is not enabled.
  bool is_dry_run_ = false;
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
  // The key-value pairs that will be associated with screenshot baselines.
  TestEnvironmentMap test_environment_;
  // Protects calls to |CompareScreenshot|, which modifies the working dir.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_SKIA_GOLD_PIXEL_DIFF_H_
