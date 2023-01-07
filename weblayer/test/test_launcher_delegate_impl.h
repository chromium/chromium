// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_TEST_LAUNCHER_DELEGATE_IMPL_H_
#define WEBLAYER_TEST_TEST_LAUNCHER_DELEGATE_IMPL_H_

#include "build/build_config.h"
#include "content/public/test/test_launcher.h"

namespace weblayer {

class TestLauncherDelegateImpl : public content::TestLauncherDelegate {
 public:
  int RunTestSuite(int argc, char** argv) override;
  std::string GetUserDataDirectoryCommandLineSwitch() override;
#if !BUILDFLAG(IS_ANDROID)
  content::ContentMainDelegate* CreateContentMainDelegate() override;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_TEST_LAUNCHER_DELEGATE_IMPL_H_
