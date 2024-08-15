// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_pixel_diff.h"

#include <memory>
#include <string_view>

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include "base/auto_reset.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/test_switches.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/test/skia_gold_matching_algorithm.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace ui {
namespace test {

const char* kSkiaGoldInstance = "chrome";
const char* kSkiaGoldPublicInstance = "chrome-public";

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
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);
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
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
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

  NOTREACHED();
}

bool WriteTestEnvironmentToFile(const TestEnvironmentMap& test_environment,
                                const base::FilePath& keys_file) {
  base::Value::Dict ds;
  for (const auto& [key, value] : test_environment) {
    ds.Set(TestEnvironmentKeyToString(key), value);
  }

  base::Value root(std::move(ds));
  std::string content;
  base::JSONWriter::Write(root, &content);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::File file(keys_file, base::File::Flags::FLAG_CREATE_ALWAYS |
                                 base::File::Flags::FLAG_WRITE);
  bool ok = file.WriteAndCheck(0, base::as_byte_span(content));
  file.Close();
  if (!ok) {
    LOG(ERROR) << "Writing the keys file to temporary file failed."
               << "File path:" << keys_file.AsUTF8Unsafe();
    return false;
  }
  return true;
}

bool BotModeEnabled(const base::CommandLine* command_line) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  return command_line->HasSwitch(switches::kTestLauncherBotMode) ||
         env->HasVar("CHROMIUM_TEST_LAUNCHER_BOT_MODE");
}

const char* GetDiffGoldInstance() {
  // TODO(skbug.com/10610): Decide whether to use the public or non-public
  // instance once authentication is fixed for the non-public instance.
  return kSkiaGoldPublicInstance;
}

// Non-empty test corpus and environment map.
using SessionCacheKey = std::pair<std::string, TestEnvironmentMap>;
using SessionCache =
    base::LRUCache<SessionCacheKey, std::unique_ptr<SkiaGoldPixelDiff>>;
SessionCache g_sessions(SessionCache::NO_AUTO_EVICT);

// If present, overrides |LaunchProcess|.
SkiaGoldPixelDiff::LaunchProcessCallback g_custom_launch_process;

int LaunchProcess(const base::CommandLine& cmdline) {
  if (g_custom_launch_process) {
    return g_custom_launch_process.Run(cmdline);
  }

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

}  // namespace

SkiaGoldPixelDiff::SkiaGoldPixelDiff() = default;

SkiaGoldPixelDiff::~SkiaGoldPixelDiff() = default;

SkiaGoldPixelDiff::ScopedSessionCacheForTesting::
    ScopedSessionCacheForTesting() {
  g_sessions.Clear();
}

SkiaGoldPixelDiff::ScopedSessionCacheForTesting::
    ~ScopedSessionCacheForTesting() {
  g_sessions.Clear();
}

// static
SkiaGoldPixelDiff* SkiaGoldPixelDiff::GetSession(
    const std::optional<std::string>& corpus,
    TestEnvironmentMap test_environment) {
  FillInSystemEnvironment(test_environment);
  const std::string corpus_name = corpus.value_or("gtest-pixeltests");
  CHECK(!corpus_name.empty());

  SessionCacheKey key(corpus_name, std::move(test_environment));
  auto it = g_sessions.Get(key);
  if (it == g_sessions.end()) {
    std::unique_ptr<SkiaGoldPixelDiff> pixel_diff =
        std::unique_ptr<SkiaGoldPixelDiff>(new SkiaGoldPixelDiff());
    pixel_diff->Init(corpus_name, key.second);
    it = g_sessions.Put(std::move(key), std::move(pixel_diff));
  }

  CHECK(it != g_sessions.end());
  return it->second.get();
}

// static
base::AutoReset<SkiaGoldPixelDiff::LaunchProcessCallback>
SkiaGoldPixelDiff::OverrideLaunchProcessForTesting(
    SkiaGoldPixelDiff::LaunchProcessCallback custom_launch_process) {
  base::AutoReset auto_reset(&g_custom_launch_process,
                             std::move(custom_launch_process));
  return auto_reset;
}

// static
std::string SkiaGoldPixelDiff::GetPlatform() {
  return GetPlatformName();
}

void SkiaGoldPixelDiff::InitSkiaGold() const {
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
  ASSERT_TRUE(WriteTestEnvironmentToFile(test_environment_, json_temp_file));
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

void SkiaGoldPixelDiff::Init(const std::string& corpus,
                             TestEnvironmentMap test_environment) {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!BotModeEnabled(base::CommandLine::ForCurrentProcess())) {
    is_dry_run_ = true;
  }

  ASSERT_TRUE(cmd_line->HasSwitch(kBuildRevisionKey) || is_dry_run_)
      << "Missing switch " << kBuildRevisionKey;

  // Use the dummy revision code for dry run.
  build_revision_ = is_dry_run_
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
  if (cmd_line->HasSwitch(kNoLuciAuth) || is_dry_run_) {
    luci_auth_ = false;
  }
  initialized_ = true;
  corpus_ = corpus;
  test_environment_ = std::move(test_environment);
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
  if (is_dry_run_) {
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

// static
std::string SkiaGoldPixelDiff::GetGoldenImageName(
    const std::string& test_suite_name,
    const std::string& test_name,
    const std::optional<std::string>& suffix) {
  std::vector<std::string_view> parts;

  // Test suites can have "/" in their names from a parameterization
  // instantiation, which isn't allowed in file names.
  auto test_suite_parts = base::SplitStringPiece(
      test_suite_name, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& test_suite_part : test_suite_parts) {
    parts.push_back(test_suite_part);
  }

  // Tests can have "/" in their names from a parameterization value, which
  // isn't allowed in file names.
  auto test_name_parts = base::SplitStringPiece(
      test_name, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (const auto& test_name_part : test_name_parts) {
    parts.push_back(test_name_part);
  }

  if (suffix.has_value()) {
    parts.push_back(suffix.value());
  }

  const char* separator =
      GetPlatform() == std::string("ash") ? kAshSeparator : kNonAshSeparator;
  return base::JoinString(parts, separator);
}

// static
std::string SkiaGoldPixelDiff::GetGoldenImageName(
    const ::testing::TestInfo* test_info,
    const std::optional<std::string>& suffix) {
  return GetGoldenImageName(test_info->test_suite_name(), test_info->name(),
                            suffix);
}

bool SkiaGoldPixelDiff::CompareScreenshot(
    const std::string& golden_image_name,
    const SkBitmap& bitmap,
    const SkiaGoldMatchingAlgorithm* algorithm) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(initialized_) << "Initialize the class before using this method.";
  std::vector<unsigned char> output;
  bool ret = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &output);
  if (!ret) {
    LOG(ERROR) << "Encoding SkBitmap to PNG format failed.";
    return false;
  }

  CHECK_EQ(golden_image_name.find_first_of(" /"), std::string::npos)
      << " a golden image name should not contain any space or back slash: "
      << golden_image_name;

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath temporary_path = working_dir_.Append(
      base::FilePath::FromUTF8Unsafe(golden_image_name + ".png"));
  base::File file(temporary_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                      base::File::Flags::FLAG_WRITE);
  bool ok = file.WriteAndCheck(0, output);
  file.Close();
  if (!ok) {
    LOG(ERROR) << "Writing the PNG image to temporary file failed."
               << "File path:" << temporary_path.AsUTF8Unsafe();
    return false;
  }
  bool success =
      UploadToSkiaGoldServer(temporary_path, golden_image_name, algorithm);
  if (!success && is_dry_run_) {
    GenerateLocalDiff(golden_image_name, temporary_path);
  }

  return success;
}

void SkiaGoldPixelDiff::GenerateLocalDiff(
    const std::string& remote_golden_image_name,
    const base::FilePath& test_output_path) const {
  base::CommandLine* process_command_line =
      base::CommandLine::ForCurrentProcess();
  if (!process_command_line->HasSwitch(kPngFilePathDebugging)) {
    LOG(WARNING)
        << "Please use --" << kPngFilePathDebugging
        << " to generate local diff images for this screenshot mismatch.";
    return;
  }
  base::FilePath path =
      process_command_line->GetSwitchValuePath(kPngFilePathDebugging);
  if (!base::PathExists(path)) {
    base::CreateDirectory(path);
  }

  auto output_dir = path.AppendASCII(remote_golden_image_name);
  if (!base::PathExists(output_dir)) {
    base::CreateDirectory(output_dir);
  }

  // TODO(skbug.com/10611): Remove this temporary work dir and instead just use
  // |working_dir_| once `goldctl diff` stops clobbering the auth files in the
  // provided work directory.
  auto temp_dir = base::ScopedTempDir();
  if (!temp_dir.CreateUniqueTempDir()) {
    LOG(WARNING) << "Failed to create a local diff temp work dir.";
    return;
  }
  if (!base::CopyDirectory(working_dir_, temp_dir.GetPath(), true)) {
    LOG(WARNING) << "Failed to copy working dir to local diff temp work dir.";
    return;
  }
  // |CopyDirectory| will copy the source directory itself, rather than its
  // contents, so we need to locate the copy.
  base::FilePath temp_work_dir =
      base::FileEnumerator(temp_dir.GetPath(), false,
                           base::FileEnumerator::DIRECTORIES,
                           working_dir_.BaseName().value())
          .Next();
  CHECK(!temp_work_dir.empty());

  base::CommandLine cmd(GetAbsoluteSrcRelativePath(kSkiaGoldCtl));
  cmd.AppendSwitchASCII("corpus", corpus_);
  cmd.AppendSwitchASCII("instance", GetDiffGoldInstance());
  cmd.AppendSwitchASCII("test", remote_golden_image_name);
  cmd.AppendSwitchPath("input", test_output_path);
  cmd.AppendSwitchPath("work-dir", temp_work_dir);
  cmd.AppendSwitchPath("out-dir", output_dir);
  AppendArgsJustAfterProgram(cmd, {FILE_PATH_LITERAL("diff")});

  base::CommandLine::StringType cmd_str = cmd.GetCommandLineString();
  LOG(INFO) << "Skia Gold Commandline: " << cmd_str;
  int exit_code = LaunchProcess(cmd);
  CHECK_EQ(exit_code, 0);

  struct DiffLink {
    base::FilePath png_path;
    base::Time mtime;
  };
  struct DiffLinks {
    std::optional<DiffLink> given_image;
    std::optional<DiffLink> closest_image;
    std::optional<DiffLink> diff_image;
  };

  auto AssignIfNewer = [](std::optional<DiffLink>& image,
                          const base::FilePath& png_path,
                          const base::Time& mtime) {
    if (!image.has_value() || mtime > image->mtime) {
      image = {
          .png_path = png_path,
          .mtime = mtime,
      };
    };
  };

  DiffLinks results;
  base::FileEnumerator e(output_dir, false, base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.png"));
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    base::Time mtime = e.GetInfo().GetLastModifiedTime();
    std::string png_file_name = name.BaseName().MaybeAsASCII();
    if (png_file_name.starts_with("input-")) {
      AssignIfNewer(results.given_image, name, mtime);
    } else if (png_file_name.starts_with("closest-")) {
      AssignIfNewer(results.closest_image, name, mtime);
    } else if (png_file_name == "diff.png") {
      AssignIfNewer(results.diff_image, name, mtime);
    }
  }

  auto FormatPathForTerminalOutput =
      [](std::optional<DiffLink>& path) -> std::optional<std::string> {
    if (path.has_value()) {
      base::FilePath path_absolute = path.value().png_path;
      if (!path_absolute.IsAbsolute()) {
        path_absolute = base::PathService::CheckedGet(base::DIR_CURRENT)
                            .Append(path_absolute);
      }

      base::FilePath path_normalized;
      if (!base::NormalizeFilePath(path_absolute, &path_normalized)) {
        return {path->png_path.MaybeAsASCII()};
      }

      return {std::string("file:///") +
              path_normalized.NormalizePathSeparatorsTo(FILE_PATH_LITERAL('/'))
                  .MaybeAsASCII()};
    } else {
      return std::nullopt;
    }
  };

  const char* failure_message = "Unable to retrieve link";
  LOG(INFO) << "\n  Generated image: "
            << FormatPathForTerminalOutput(results.given_image)
                   .value_or(failure_message)
            << "\n  Closest image: "
            << FormatPathForTerminalOutput(results.closest_image)
                   .value_or(failure_message)
            << "\n  Diff image: "
            << FormatPathForTerminalOutput(results.diff_image)
                   .value_or(failure_message)
            << "\n";
}

}  // namespace test
}  // namespace ui
