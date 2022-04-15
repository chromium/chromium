// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/examples/examples_exit_code.h"
#include "ui/views/examples/examples_main_proc.h"

namespace views {
namespace examples {

// TODO(crbug.com/1221012): Disabled the test on windows due to its flakiness.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TestViewsExamplesLaunches DISABLED_TestViewsExamplesLaunches
#else
#define MAYBE_TestViewsExamplesLaunches TestViewsExamplesLaunches
#endif

TEST(ExamplesTest, MAYBE_TestViewsExamplesLaunches) {
  const ExamplesExitCode exit_code = ExamplesMainProc(true);
  // Check the status of the Skia Gold comparison.
  EXPECT_EQ(ExamplesExitCode::kSucceeded, exit_code);
}

}  // namespace examples
}  // namespace views
