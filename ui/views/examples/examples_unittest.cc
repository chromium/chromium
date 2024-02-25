// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/examples/examples_exit_code.h"
#include "ui/views/examples/examples_main_proc.h"

namespace views::examples {

TEST(ExamplesTest, TestViewsExamplesLaunches) {
  const ExamplesExitCode exit_code = ExamplesMainProc(/*under_test=*/true);
  // Check the status of the Skia Gold comparison.
  EXPECT_TRUE((exit_code == ExamplesExitCode::kSucceeded) ||
              (exit_code == ExamplesExitCode::kNone));
}

}  // namespace views::examples
