// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/shell.h"

#include <shlobj.h>

#include "base/base_paths.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui::win {

class ShellTest : public testing::TestWithParam<bool> {
 public:
  ShellTest() {
    feature_list_.InitWithFeatureState(kManuallyParsePathForShellExecute,
                                       GetParam());
  }

  void SetUp() override {
#if defined(ADDRESS_SANITIZER) || defined(COMPONENT_BUILD)
    GTEST_SKIP() << "Test not supported in component or asan builds because "
                    "binary must be standalone";
#endif  // defined(ADDRESS_SANITIZER) || defined(COMPONENT_BUILD)
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ShellTest, OpenFileWithSpaceInExtension) {
  base::win::ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.Succeeded());

  const auto FeatureEnabled = GetParam;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a file with a name that triggers legacy resolution (space in
  // extension).
  const base::FilePath file_path = temp_dir.GetPath().Append(L"test. txt");
  ASSERT_TRUE(base::WriteFile(file_path, "test content"));

  // Find the helper executable.
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
  exe_path = exe_path.Append(L"shell_test_helper.exe");
  ASSERT_TRUE(base::PathExists(exe_path));

  // Create the "legacy resolution" target that ShellExecute might mistakenly
  // pick.
  base::FilePath legacy_exe_path = temp_dir.GetPath().Append(L"test. txt.exe");
  ASSERT_TRUE(base::CopyFile(exe_path, legacy_exe_path));

  // The helper will create "executed.txt" in its current directory.
  // ui::win::OpenFileViaShell sets the working directory to the file's DirName.
  base::FilePath executed_sentinel = temp_dir.GetPath().Append(L"executed.txt");
  ASSERT_FALSE(base::PathExists(executed_sentinel));

  if (FeatureEnabled()) {
    // When feature is enabled, ui::win uses SHParseDisplayName.
    // Verify that SHParseDisplayName resolves to the correct file.
    base::win::ScopedCoMem<ITEMIDLIST_ABSOLUTE> path_id_list;
    ASSERT_TRUE(SUCCEEDED(::SHParseDisplayName(file_path.value().c_str(),
                                               nullptr, &path_id_list,
                                               SFGAO_FILESYSTEM, nullptr)));

    wchar_t parsed_path[MAX_PATH];
    ASSERT_TRUE(::SHGetPathFromIDList(path_id_list.get(), parsed_path));
    base::FilePath parsed(parsed_path);
    EXPECT_EQ(file_path, parsed);
  }

  base::RunLoop sentinel_run_loop;
  base::FilePathWatcher watcher;
  // Watch the directory for the creation of the sentinel file.
  ASSERT_TRUE(watcher.Watch(
      temp_dir.GetPath(), base::FilePathWatcher::Type::kNonRecursive,
      base::BindLambdaForTesting([&](const base::FilePath& path, bool error) {
        if (!error && base::PathExists(path.Append(L"executed.txt"))) {
          sentinel_run_loop.Quit();
        }
      })));

  ASSERT_TRUE(OpenFileViaShell(file_path));

  if (FeatureEnabled()) {
    // Feature enabled: it will not execute the exe, the openwith dialog will
    // appear instead, which the test cannot easily check for. The runloop still
    // needs to exit, so wait the time it might take to launch a process, then
    // terminate it.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, sentinel_run_loop.QuitClosure(), base::Seconds(3));
  }

  sentinel_run_loop.Run();

  EXPECT_EQ(FeatureEnabled(), !base::PathExists(executed_sentinel));
}

INSTANTIATE_TEST_SUITE_P(, ShellTest, testing::Bool(), [](const auto& info) {
  return info.param ? "ManuallyParsed" : "AutomaticallyParsed";
});

}  // namespace ui::win
