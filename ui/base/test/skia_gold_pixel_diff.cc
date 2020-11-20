// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_pixel_diff.h"

#include "build/build_config.h"
#if defined(OS_WIN)
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
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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

#if defined(OS_WIN)
const wchar_t* kSkiaGoldCtl = L"tools/skia_goldctl/win/goldctl.exe";
#elif defined(OS_APPLE)
const char* kSkiaGoldCtl = "tools/skia_goldctl/mac/goldctl";
#else
const char* kSkiaGoldCtl = "tools/skia_goldctl/linux/goldctl";
#endif

const char* kBuildRevisionKey = "git-revision";

// The switch keys for tryjob.
const char* kIssueKey = "gerrit-issue";
const char* kPatchSetKey = "gerrit-patchset";
const char* kJobIdKey = "buildbucket-id";
const char* kCodeReviewSystemKey = "code-review-system";

const char* kNoLuciAuth = "no-luci-auth";
const char* kBypassSkiaGoldFunctionality = "bypass-skia-gold-functionality";
const char* kDryRun = "dryrun";

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

void FillInSystemEnvironment(base::Value::DictStorage& ds) {
  std::string processor = "unknown";
#if defined(ARCH_CPU_X86)
  processor = "x86";
#elif defined(ARCH_CPU_X86_64)
  processor = "x86_64";
#else
  LOG(WARNING) << "Unknown Processor.";
#endif

  ds["system"] = base::Value(SkiaGoldPixelDiff::GetPlatform());
  ds["processor"] = base::Value(processor);
}

// Returns whether image comparison failure should result in Gerrit comments.
// In general, when a pixel test fails on CQ, Gold will make a gerrit
// comment indicating that the cl breaks some pixel tests. However,
// if the test is flaky and has a failure->passing pattern, we don't
// want Gold to make gerrit comments on the first failure.
// This function returns true iff:
//  * it's a tryjob and no retries left.
//  or * it's a CI job.
bool ShouldMakeGerritCommentsOnFailures() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (!cmd->HasSwitch(kIssueKey))
    return true;
  if (cmd->HasSwitch(switches::kTestLauncherRetriesLeft)) {
    int retries_left = 0;
    bool succeed = base::StringToInt(
        cmd->GetSwitchValueASCII(switches::kTestLauncherRetriesLeft),
        &retries_left);
    if (!succeed) {
      LOG(ERROR) << switches::kTestLauncherRetriesLeft << " = "
                 << cmd->GetSwitchValueASCII(switches::kTestLauncherRetriesLeft)
                 << " can not convert to integer.";
      return true;
    }
    if (retries_left > 0) {
      LOG(INFO) << "Test failure will not result in Gerrit comment because"
                   " there are more retries.";
      return false;
    }
  }
  return true;
}

// Fill in test environment to the keys_file. The format is json.
// We need the system information to determine whether a new screenshot
// is good or not. All the information that can affect the output of pixels
// should be filled in. Eg: operating system, graphics card, processor
// architecture, screen resolution, etc.
bool FillInTestEnvironment(const base::FilePath& keys_file) {
  base::Value::DictStorage ds;
  FillInSystemEnvironment(ds);
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
#if defined(OS_WIN)
  return "windows";
#elif defined(OS_APPLE)
  return "macOS";
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#elif defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return "linux";
#endif
}

int SkiaGoldPixelDiff::LaunchProcess(const base::CommandLine& cmdline) const {
  base::Process sub_process =
      base::LaunchProcess(cmdline, base::LaunchOptionsForTest());
  int exit_code = 0;
  if (!sub_process.WaitForExit(&exit_code)) {
    ADD_FAILURE() << "Failed to wait for process.";
    // Return a non zero code indicating an error.
    return 1;
  }
  return exit_code;
}

void SkiaGoldPixelDiff::InitSkiaGold() {
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

  base::FilePath json_temp_file =
      working_dir_.Append(FILE_PATH_LITERAL("keys_file.txt"));
  FillInTestEnvironment(json_temp_file);
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
                             const std::string& corpus) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  ASSERT_TRUE(cmd_line->HasSwitch(kBuildRevisionKey))
      << "Missing switch " << kBuildRevisionKey;
  ASSERT_TRUE(
      cmd_line->HasSwitch(kIssueKey) && cmd_line->HasSwitch(kPatchSetKey) &&
          cmd_line->HasSwitch(kJobIdKey) ||
      !cmd_line->HasSwitch(kIssueKey) && !cmd_line->HasSwitch(kPatchSetKey) &&
          !cmd_line->HasSwitch(kJobIdKey))
      << "Missing switch. If it's running for tryjob, you should pass --"
      << kIssueKey << " --" << kPatchSetKey << " --" << kJobIdKey
      << ". Otherwise, do not pass any one of them.";
  build_revision_ = cmd_line->GetSwitchValueASCII(kBuildRevisionKey);
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

  InitSkiaGold();
}

bool SkiaGoldPixelDiff::UploadToSkiaGoldServer(
    const base::FilePath& local_file_path,
    const std::string& remote_golden_image_name,
    const SkiaGoldMatchingAlgorithm* algorithm) const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kBypassSkiaGoldFunctionality)) {
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

  if (!BotModeEnabled(base::CommandLine::ForCurrentProcess())) {
    cmd.AppendSwitch(kDryRun);
  }

  std::map<std::string, std::string> optional_keys;
  if (!ShouldMakeGerritCommentsOnFailures()) {
    optional_keys["ignore"] = "1";
  }
  for (auto key : optional_keys) {
    cmd.AppendSwitchASCII("add-test-optional-key",
                          base::StrCat({key.first, ":", key.second}));
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
  std::string normalized_screenshot_name;
  // Parameterized tests have "/" in their names which isn't allowed in file
  // names. Replace with "_".
  base::ReplaceChars(screenshot_name, "/", "_", &normalized_screenshot_name);
  std::string name = prefix_ + "_" + normalized_screenshot_name + "_" + suffix;
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
