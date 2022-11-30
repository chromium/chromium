// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/test_launcher_delegate_impl.h"

#include "base/command_line.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "weblayer/app/content_main_delegate_impl.h"
#include "weblayer/public/common/switches.h"
#include "weblayer/shell/app/shell_main_params.h"

namespace weblayer {

int TestLauncherDelegateImpl::RunTestSuite(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);
  // Browser tests are expected not to tear-down various globals.
  test_suite.DisableCheckForLeakedGlobals();
  return test_suite.Run();
}

std::string TestLauncherDelegateImpl::GetUserDataDirectoryCommandLineSwitch() {
  return switches::kWebLayerUserDataDir;
}

#if !BUILDFLAG(IS_ANDROID)
content::ContentMainDelegate*
TestLauncherDelegateImpl::CreateContentMainDelegate() {
  return new ContentMainDelegateImpl(CreateMainParams());
}
#endif

}  // namespace weblayer
