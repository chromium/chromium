// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_pixel_diff.h"

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

void FillInSystemEnvironment(base::Value::Dict& ds) {
  std::string processor = "unknown";
#if defined(ARCH_CPU_X86)
  processor = "x86";
#elif defined(ARCH_CPU_X86_64)
  processor = "x86_64";
#else
  LOG(WARNING) << "Unknown Processor.";
#endif

  ds.Set("system", SkiaGoldPixelDiff::GetPlatform());
  ds.Set("processor", processor);
}

// Fill in test environment to the keys_file. The format is json.
// We need the system information to determine whether a new screenshot
// is good or not. All the information that can affect the output of pixels
// should be filled in. Eg: operating system, graphics card, processor
// architecture, screen resolution, etc.
bool FillInTestEnvironment(const base::FilePath& keys_file) {
  base::Value::Dict ds;
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

  InitSkiaGold();
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
