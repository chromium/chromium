// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_TEST_LAUNCHER_DELEGATE_IMPL_H_
#define WEBLAYER_TEST_TEST_LAUNCHER_DELEGATE_IMPL_H_

#include "content/public/test/test_launcher.h"

namespace weblayer {

class TestLauncherDelegateImpl : public content::TestLauncherDelegate {
 public:
  int RunTestSuite(int argc, char** argv) override;
  bool AdjustChildProcessCommandLine(
      base::CommandLine* command_line,
      const base::FilePath& temp_data_dir) override;
#if !defined(OS_ANDROID)
  content::ContentMainDelegate* CreateContentMainDelegate() override;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_TEST_LAUNCHER_DELEGATE_IMPL_H_
