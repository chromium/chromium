// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "ui/views/examples/examples_main_proc.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/examples/examples_main_mac_support.h"
#endif

#if BUILDFLAG(IS_MAC)
int ViewsExamplesMain(int argc, char** argv) {
#else
int main(int argc, char** argv) {
#endif  // BUILDFLAG(IS_MAC)
  base::CommandLine::Init(argc, argv);

  // The use of base::test::TaskEnvironment in the following function relies on
  // the timeout values from TestTimeouts.
  TestTimeouts::Initialize();

  base::AtExitManager at_exit;

#if BUILDFLAG(IS_MAC)
  UpdateFrameworkBundlePath();
#endif

  return static_cast<int>(views::examples::ExamplesMainProc());
}
