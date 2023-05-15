// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/examples/examples_exit_code.h"
#include "ui/views/examples/examples_main_proc.h"

#if BUILDFLAG(IS_WIN)
#include "ui/native_theme/native_theme_win.h"
#endif

namespace views::examples {

TEST(ExamplesTest, TestViewsExamplesLaunches) {
#if BUILDFLAG(IS_WIN)
  if (ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()) {
    GTEST_SKIP() << "Host is in dark mode; skipping test";
  }
#endif
  const ExamplesExitCode exit_code = ExamplesMainProc(true);
  // Check the status of the Skia Gold comparison.
  EXPECT_EQ(ExamplesExitCode::kSucceeded, exit_code);
}

}  // namespace views::examples
