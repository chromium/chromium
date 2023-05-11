// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_pixel_diff.h"

#include "base/notreached.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include "third_party/skia/include/core/SkBitmap.h"

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/test/test_switches.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace ui {
namespace test {

const char* kSkiaGoldInstance = "chrome";

#if BUILDFLAG(IS_WIN)
const wchar_t* kSkiaGoldCtl = L"tools/skia_goldctl/win/goldctl.exe";
#elif BUILDFLAG(IS_APPLE)
#if defined(ARCH_CPU_ARM64)
const char* kSkiaGoldCtl = "tools/skia_goldctl/mac_arm64/goldctl";
#else
const char* kSkiaGoldCtl = "tools/skia_goldctl/mac_amd64/goldctl";
#endif  // defined(ARCH_CPU_ARM64)
#else
const char* kSkiaGoldCtl = "tools/skia_goldctl/linux/goldctl";
#endif

const char* kBuildRevisionKey = "git-revision";

// A dummy build revision used only under a dry run.
constexpr char kDummyBuildRevision[] = "12345";

// The switch keys for tryjob.
const char* kIssueKey = "gerrit-issue";
const char* kPatchSetKey = "gerrit-patchset";
const char* kJobIdKey = "buildbucket-id";
const char* kCodeReviewSystemKey = "code-review-system";

const char* kNoLuciAuth = "no-luci-auth";
const char* kBypassSkiaGoldFunctionality = "bypass-skia-gold-functionality";
const char* kDryRun = "dryrun";

// The switch key for saving png file locally for debugging. This will allow
// the framework to save the screenshot png file to this path.
const char* kPngFilePathDebugging = "skia-gold-local-png-write-directory";

const char* kGoldOutputTriageFormat =
    "Untriaged or negative image: https://chrome-gold.skia.org";
const char* kPublicTriageLink = "https://chrome-public-gold.skia.org";

// The separator used in the names of the screenshots taken on Ash platform.
constexpr char kAshSeparator[] = ".";

// The separator used by non-Ash platforms.
constexpr char kNonAshSeparator[] = "_";

namespace {

base::FilePath GetAbsoluteSrcRelativePath(base::FilePath::StringType path) {
  base::FilePath root_path;
  base::PathService::Get(base::BasePathKey::DIR_SOURCE_ROOT, &root_path);
  return base::MakeAbsoluteFilePath(root_path.Append(path));
}

// Append args after program.
// The base::Commandline.AppendArg append the arg at
// the end which doesn't work for us.
void AppendArgsJustAfterProgram(base::CommandLine& cmd,
                                base::CommandLine::StringVector args) {
  base::CommandLine::StringVector& argv =
      const_cast<base::CommandLine::StringVector&>(cmd.argv());
  argv.insert(argv.begin() + 1, args.begin(), args.end());
}

const char* GetPlatformName() {
#if BUILDFLAG(IS_WIN)
  return "windows";
#elif BUILDFLAG(IS_APPLE)
  return "macOS";
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#elif BUILDFLAG(IS_LINUX)
  return "linux";
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return "lacros";
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return "ash";
#endif
}

const char* GetArchName() {
#if defined(ARCH_CPU_X86)
  return "x86";
#elif defined(ARCH_CPU_X86_64)
  return "x86_64";
#elif defined(ARCH_CPU_ARM64)
  return "Arm64";
#else
  LOG(WARNING) << "Unknown Processor.";
  return "unknown";
#endif
}

void FillInSystemEnvironment(TestEnvironmentMap& test_environment) {
  // Fill in a default key and assert it was not already filled in.
  auto CheckInsertDefaultKey = [&test_environment](TestEnvironmentKey key,
                                                   std::string value) {
    bool did_insert = false;
    std::tie(std::ignore, did_insert) = test_environment.insert({key, value});
    CHECK(did_insert);
  };

  CheckInsertDefaultKey(TestEnvironmentKey::kSystem, GetPlatformName());
  CheckInsertDefaultKey(TestEnvironmentKey::kProcessor, GetArchName());
}

const char* TestEnvironmentKeyToString(TestEnvironmentKey key) {
  switch (key) {
    case TestEnvironmentKey::kSystem:
      return "system";
    case TestEnvironmentKey::kProcessor:
      return "processor";
    case TestEnvironmentKey::kSystemVersion:
      return "system_version";
    case TestEnvironmentKey::kGpuDriverVendor:
      return "driver_vendor";
    case TestEnvironmentKey::kGpuDriverVersion:
      return "driver_version";
    case TestEnvironmentKey::kGlRenderer:
      return "gl_renderer";
  }

  NOTREACHED_NORETURN();
}

bool WriteTestEnvironmentToFile(TestEnvironmentMap test_environment,
                                const base::FilePath& keys_file) {
  base::Value::Dict ds;
  for (auto& [key, value] : test_environment) {
    ds.Set(TestEnvironmentKeyToString(key), value);
  }

  base::Value root(std::move(ds));
  std::string content;
  base::JSONWriter::Write(root, &content);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::File file(keys_file, base::File::Flags::FLAG_CREATE_ALWAYS |
                                 base::File::Flags::FLAG_WRITE);
  int ret_code = file.Write(0, content.c_str(), content.size());
  file.Close();
  if (ret_code <= 0) {
    LOG(ERROR) << "Writing the keys file to temporary file failed."
               << "File path:" << keys_file.AsUTF8Unsafe()
               << ". Return code: " << ret_code;
    return false;
  }
  return true;
}

bool BotModeEnabled(const base::CommandLine* command_line) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return command_line->HasSwitch(switches::kTestLauncherBotMode) ||
         env->HasVar("CHROMIUM_TEST_LAUNCHER_BOT_MODE");
}

}  // namespace

SkiaGoldPixelDiff::SkiaGoldPixelDiff() = default;

SkiaGoldPixelDiff::~SkiaGoldPixelDiff() = default;

// static
std::string SkiaGoldPixelDiff::GetPlatform() {
  return GetPlatformName();
}

int SkiaGoldPixelDiff::LaunchProcess(const base::CommandLine& cmdline) const {
  std::string output;
  int exit_code = 0;
  CHECK(base::GetAppOutputWithExitCode(cmdline, &output, &exit_code));
  LOG(INFO) << output;
  // Gold binary only provides internal triage link which doesn't work
  // for non-Googlers. So we construct another link that works for
  // non google account committers.
  size_t triage_location_start = output.find(kGoldOutputTriageFormat);
  if (triage_location_start != std::string::npos) {
    size_t triage_location_end = output.find("\n", triage_location_start);
    LOG(WARNING) << "For committers not using @google.com account, triage "
                 << "using the following link: " << kPublicTriageLink
                 << output.substr(
                        triage_location_start + strlen(kGoldOutputTriageFormat),
                        triage_location_end - triage_location_start -
                            strlen(kGoldOutputTriageFormat));
  }
  return exit_code;
}

void SkiaGoldPixelDiff::InitSkiaGold(TestEnvironmentMap test_environment) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kBypassSkiaGoldFunctionality)) {
    LOG(WARNING) << "Bypassing Skia Gold initialization due to "
                 << "--bypass-skia-gold-functionality being present.";
    return;
  }
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::CommandLine cmd(GetAbsoluteSrcRelativePath(kSkiaGoldCtl));
  cmd.AppendSwitchPath("work-dir", working_dir_);
  if (luci_auth_) {
    cmd.AppendArg("--luci");
  }
  AppendArgsJustAfterProgram(cmd, {FILE_PATH_LITERAL("auth")});
  base::CommandLine::StringType cmd_str = cmd.GetCommandLineString();
  LOG(INFO) << "Skia Gold Auth Commandline: " << cmd_str;
  int exit_code = LaunchProcess(cmd);
  ASSERT_EQ(exit_code, 0);

  FillInSystemEnvironment(test_environment);

  base::FilePath json_temp_file =
      working_dir_.Append(FILE_PATH_LITERAL("keys_file.txt"));
  ASSERT_TRUE(
      WriteTestEnvironmentToFile(std::move(test_environment), json_temp_file));
  base::FilePath failure_temp_file =
      working_dir_.Append(FILE_PATH_LITERAL("failure.log"));
  cmd = base::CommandLine(GetAbsoluteSrcRelativePath(kSkiaGoldCtl));
  cmd.AppendSwitchASCII("instance", kSkiaGoldInstance);
  cmd.AppendSwitchPath("work-dir", working_dir_);
  cmd.AppendSwitchPath("keys-file", json_temp_file);
  cmd.AppendSwitchPath("failure-file", failure_temp_file);
  cmd.AppendSwitch("passfail");
  cmd.AppendSwitchASCII("commit", build_revision_);
  // This handles the logic for tryjob.
  if (issue_.length()) {
    cmd.AppendSwitchASCII("issue", issue_);
    cmd.AppendSwitchASCII("patchset", patchset_);
    cmd.AppendSwitchASCII("jobid", job_id_);
    cmd.AppendSwitchASCII("crs", code_review_system_);
    cmd.AppendSwitchASCII("cis", "buildbucket");
  }

  AppendArgsJustAfterProgram(
      cmd, {FILE_PATH_LITERAL("imgtest"), FILE_PATH_LITERAL("init")});
  cmd_str = cmd.GetCommandLineString();
  LOG(INFO) << "Skia Gold imgtest init Commandline: " << cmd_str;
  exit_code = LaunchProcess(cmd);
  ASSERT_EQ(exit_code, 0);
}

void SkiaGoldPixelDiff::Init(const std::string& screenshot_prefix,
                             const std::string& corpus,
                             TestEnvironmentMap test_environment) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!BotModeEnabled(base::CommandLine::ForCurrentProcess())) {
    cmd_line->AppendSwitch(kDryRun);
  }

  ASSERT_TRUE(cmd_line->HasSwitch(kBuildRevisionKey) ||
              cmd_line->HasSwitch(kDryRun))
      << "Missing switch " << kBuildRevisionKey;

  // Use the dummy revision code for dry run.
  build_revision_ = cmd_line->HasSwitch(kDryRun)
                        ? kDummyBuildRevision
                        : cmd_line->GetSwitchValueASCII(kBuildRevisionKey);

  ASSERT_TRUE(
      cmd_line->HasSwitch(kIssueKey) && cmd_line->HasSwitch(kPatchSetKey) &&
          cmd_line->HasSwitch(kJobIdKey) ||
      !cmd_line->HasSwitch(kIssueKey) && !cmd_line->HasSwitch(kPatchSetKey) &&
          !cmd_line->HasSwitch(kJobIdKey))
      << "Missing switch. If it's running for tryjob, you should pass --"
      << kIssueKey << " --" << kPatchSetKey << " --" << kJobIdKey
      << ". Otherwise, do not pass any one of them.";
  if (cmd_line->HasSwitch(kIssueKey)) {
    issue_ = cmd_line->GetSwitchValueASCII(kIssueKey);
    patchset_ = cmd_line->GetSwitchValueASCII(kPatchSetKey);
    job_id_ = cmd_line->GetSwitchValueASCII(kJobIdKey);
    code_review_system_ = cmd_line->GetSwitchValueASCII(kCodeReviewSystemKey);
    if (code_review_system_.empty()) {
      code_review_system_ = "gerrit";
    }
  }
  if (cmd_line->HasSwitch(kNoLuciAuth) || !BotModeEnabled(cmd_line)) {
    luci_auth_ = false;
  }
  initialized_ = true;
  prefix_ = screenshot_prefix;
  corpus_ = corpus.length() ? corpus : "gtest-pixeltests";
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("SkiaGoldTemp"),
                               &working_dir_);

  InitSkiaGold(std::move(test_environment));
}

bool SkiaGoldPixelDiff::UploadToSkiaGoldServer(
    const base::FilePath& local_file_path,
    const std::string& remote_golden_image_name,
    const SkiaGoldMatchingAlgorithm* algorithm) const {
  // Copy the png file to another place for local debugging.
  base::CommandLine* process_command_line =
      base::CommandLine::ForCurrentProcess();
  if (process_command_line->HasSwitch(kPngFilePathDebugging)) {
    base::FilePath path =
        process_command_line->GetSwitchValuePath(kPngFilePathDebugging);
    if (!base::PathExists(path)) {
      base::CreateDirectory(path);
    }
    base::FilePath filepath;
    if (remote_golden_image_name.length() <= 4 ||
        (remote_golden_image_name.length() > 4 &&
         remote_golden_image_name.substr(remote_golden_image_name.length() -
                                         4) != ".png")) {
      filepath = path.AppendASCII(remote_golden_image_name + ".png");
    } else {
      filepath = path.AppendASCII(remote_golden_image_name);
    }
    base::CopyFile(local_file_path, filepath);
  }

  if (process_command_line->HasSwitch(kBypassSkiaGoldFunctionality)) {
    LOG(WARNING) << "Bypassing Skia Gold comparison due to "
                 << "--bypass-skia-gold-functionality being present.";
    return true;
  }

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::CommandLine cmd(GetAbsoluteSrcRelativePath(kSkiaGoldCtl));
  cmd.AppendSwitchASCII("test-name", remote_golden_image_name);
  cmd.AppendSwitchASCII("corpus", corpus_);
  cmd.AppendSwitchPath("png-file", local_file_path);
  cmd.AppendSwitchPath("work-dir", working_dir_);
  if (process_command_line->HasSwitch(kDryRun)) {
    cmd.AppendSwitch(kDryRun);
  }

  if (algorithm)
    algorithm->AppendAlgorithmToCmdline(cmd);

  AppendArgsJustAfterProgram(
      cmd, {FILE_PATH_LITERAL("imgtest"), FILE_PATH_LITERAL("add")});
  base::CommandLine::StringType cmd_str = cmd.GetCommandLineString();
  LOG(INFO) << "Skia Gold Commandline: " << cmd_str;
  int exit_code = LaunchProcess(cmd);
  return exit_code == 0;
}

bool SkiaGoldPixelDiff::CompareScreenshot(
    const std::string& screenshot_name,
    const SkBitmap& bitmap,
    const SkiaGoldMatchingAlgorithm* algorithm) const {
  DCHECK(Initialized()) << "Initialize the class before using this method.";
  std::vector<unsigned char> output;
  bool ret = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &output);
  if (!ret) {
    LOG(ERROR) << "Encoding SkBitmap to PNG format failed.";
    return false;
  }
  // The golden image name should be unique on GCS per platform. And also the
  // name should be valid across all systems.
  std::string suffix = GetPlatform();
  std::string normalized_prefix;
  std::string normalized_screenshot_name;

  // Parameterized tests have "/" in their names which isn't allowed in file
  // names. Replace with `separator`.
  const std::string separator =
      suffix == std::string("ash") ? kAshSeparator : kNonAshSeparator;
  base::ReplaceChars(prefix_, "/", separator, &normalized_prefix);
  base::ReplaceChars(screenshot_name, "/", separator,
                     &normalized_screenshot_name);
  std::string name = normalized_prefix + separator +
                     normalized_screenshot_name + separator + suffix;
  CHECK_EQ(name.find_first_of(" /"), std::string::npos)
      << " a golden image name should not contain any space or back slash";

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath temporary_path =
      working_dir_.Append(base::FilePath::FromUTF8Unsafe(name + ".png"));
  base::File file(temporary_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                      base::File::Flags::FLAG_WRITE);
  int ret_code = file.Write(0, (char*)output.data(), output.size());
  file.Close();
  if (ret_code <= 0) {
    LOG(ERROR) << "Writing the PNG image to temporary file failed."
               << "File path:" << temporary_path.AsUTF8Unsafe()
               << ". Return code: " << ret_code;
    return false;
  }
  return UploadToSkiaGoldServer(temporary_path, name, algorithm);
}

}  // namespace test
}  // namespace ui
