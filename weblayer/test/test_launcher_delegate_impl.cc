// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/test_launcher_delegate_impl.h"

#include "base/command_line.h"
#include "base/test/test_suite.h"
#include "weblayer/app/content_main_delegate_impl.h"
#include "weblayer/shell/app/shell_main_params.h"
#include "weblayer/shell/common/shell_switches.h"

namespace weblayer {

int TestLauncherDelegateImpl::RunTestSuite(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  // Browser tests are expected not to tear-down various globals and may
  // complete with the thread priority being above NORMAL.
  test_suite.DisableCheckForLeakedGlobals();
  test_suite.DisableCheckForThreadPriorityAtTestEnd();
  return test_suite.Run();
}

bool TestLauncherDelegateImpl::AdjustChildProcessCommandLine(
    base::CommandLine* command_line,
    const base::FilePath& temp_data_dir) {
  command_line->AppendSwitchPath(switches::kWebLayerShellDataPath,
                                 temp_data_dir);
  return true;
}

#if !defined(OS_ANDROID)
content::ContentMainDelegate*
TestLauncherDelegateImpl::CreateContentMainDelegate() {
  return new ContentMainDelegateImpl(CreateMainParams());
}
#endif

}  // namespace weblayer
