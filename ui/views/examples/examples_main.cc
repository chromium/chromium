// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/test/test_timeouts.h"
#include "ui/views/examples/examples_main_proc.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // The use of base::test::TaskEnvironment in the following function relies on
  // the timeout values from TestTimeouts.
  TestTimeouts::Initialize();

  base::AtExitManager at_exit;

  return static_cast<int>(views::examples::ExamplesMainProc());
}
