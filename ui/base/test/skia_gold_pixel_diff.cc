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
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

const char* kSkiaGoldInstance = "chrome";

#if defined(OS_WIN)
const wchar_t* kSkiaGoldCtl = L"tools/skia_goldctl/win/goldctl.exe";
#elif defined(OS_MACOSX)
const char* kSkiaGoldCtl = "tools/skia_goldctl/mac/goldctl";
#else
const char* kSkiaGoldCtl = "tools/skia_goldctl/linux/goldctl";
#endif

const char* kBuildRevisionKey = "build-revision";

// The switch keys for tryjob.
const char* kIssueKey = "issue";
const char* kPatchSetKey = "patchset";
const char* kJobIdKey = "jobid";

const char* kNoLuciAuth = "no-luci-auth";

SkiaGoldPixelDiff::SkiaGoldPixelDiff() = default;

SkiaGoldPixelDiff::~SkiaGoldPixelDiff() = default;

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
  int args_size = args.size();
  argv.resize(argv.size() + args_size);
  for (int i = argv.size() - args_size; i > 1; --i) {
    argv[i + args_size - 1] = argv[i - 1];
  }
  argv.insert(argv.begin() + 1, args.begin(), args.end());
}

// Fill in test environment to the keys_file. The format is json.
// We need the system information to determine whether a new screenshot
// is good or not. All the information that can affect the output of pixels
// should be filled in. Eg: operating system, graphics card, processor
// architecture, screen resolution, etc.
bool FillInTestEnvironment(const base::FilePath& keys_file) {
  std::string system = "unknown";
  std::string processor = "unknown";
#if defined(OS_WIN)
  system = "windows";
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  switch (system_info.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_INTEL:
      processor = "x86";
      break;
    case PROCESSOR_ARCHITECTURE_AMD64:
      processor = "x86_64";
      break;
    case PROCESSOR_ARCHITECTURE_IA64:
      processor = "ia_64";
      break;
    case PROCESSOR_ARCHITECTURE_ARM:
      processor = "arm";
      break;
  }
#else
  LOG(WARNING) << "Other OS not implemented.";
#endif

  base::Value::DictStorage ds;
  ds["system"] = std::make_unique<base::Value>(system);
  ds["processor"] = std::make_unique<base::Value>(processor);
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
  if (issue_.length()) {
    cmd.AppendSwitchASCII("issue", issue_);
    cmd.AppendSwitchASCII("patchset", patchset_);
    cmd.AppendSwitchASCII("jobid", job_id_);
  }
  AppendArgsJustAfterProgram(
      cmd, {FILE_PATH_LITERAL("imgtest"), FILE_PATH_LITERAL("init")});
  cmd_str = cmd.GetCommandLineString();
  LOG(INFO) << "Skia Gold imgtest init Commandline: " << cmd_str;
  exit_code = LaunchProcess(cmd);
  ASSERT_EQ(exit_code, 0);
}

void SkiaGoldPixelDiff::Init(const std::string& screenshot_prefix) {
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
  }
  if (cmd_line->HasSwitch(kNoLuciAuth)) {
    luci_auth_ = false;
  }
  initialized_ = true;
  prefix_ = screenshot_prefix;
  base::CreateNewTempDirectory(FILE_PATH_LITERAL("SkiaGoldTemp"),
                               &working_dir_);

  InitSkiaGold();
}

bool SkiaGoldPixelDiff::UploadToSkiaGoldServer(
    const base::FilePath& local_file_path,
    const std::string& remote_golden_image_name) const {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::CommandLine cmd(GetAbsoluteSrcRelativePath(kSkiaGoldCtl));
  cmd.AppendSwitchASCII("test-name", remote_golden_image_name);
  cmd.AppendSwitchASCII("add-test-key", "source_type:gtest-pixeltests");
  cmd.AppendSwitchPath("png-file", local_file_path);
  cmd.AppendSwitchPath("work-dir", working_dir_);
  AppendArgsJustAfterProgram(
      cmd, {FILE_PATH_LITERAL("imgtest"), FILE_PATH_LITERAL("add")});
  base::CommandLine::StringType cmd_str = cmd.GetCommandLineString();
  LOG(INFO) << "Skia Gold Commandline: " << cmd_str;
  int exit_code = LaunchProcess(cmd);
  return exit_code == 0;
}

bool SkiaGoldPixelDiff::CompareScreenshot(const std::string& screenshot_name,
                                          const SkBitmap& bitmap) const {
  DCHECK(Initialized()) << "Initialize the class before using this method.";
  std::vector<unsigned char> output;
  bool ret = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &output);
  if (!ret) {
    LOG(ERROR) << "Encoding SkBitmap to PNG format failed.";
    return false;
  }
  // The golden image name should be unique on GCS. And also the name
  // should be valid across all systems.
  std::string name = prefix_ + "_" + screenshot_name;
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
  return UploadToSkiaGoldServer(temporary_path, name);
}
