// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEWS_TEST_SUITE_H_
#define UI_VIEWS_VIEWS_TEST_SUITE_H_

#include "base/test/test_suite.h"

#include "build/build_config.h"

#if defined(USE_AURA)
#include <memory>

namespace aura {
class Env;
}
#endif

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views {

class ViewsTestSuite : public base::TestSuite {
 public:
  ViewsTestSuite(int argc, char** argv);
  ~ViewsTestSuite() override;

  int RunTests();
  int RunTestsSerially();

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

#if defined(USE_AURA)
  // Different test suites may wish to create Env differently.
  virtual void InitializeEnv();
  virtual void DestroyEnv();
#endif

 private:
#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env_;
#endif

  int argc_;
  char** argv_;

  DISALLOW_COPY_AND_ASSIGN(ViewsTestSuite);
};

#if defined(USE_OZONE)
// Skips the X11-specific test on Ozone if the current platform is not X11.
#define SKIP_TEST_IF_NOT_OZONE_X11()                          \
  if (features::IsUsingOzonePlatform() &&                     \
      ui::OzonePlatform::GetPlatformNameForTest() != "x11") { \
    GTEST_SKIP() << "This test is X11-only";                  \
  }
#else
#define SKIP_TEST_IF_NOT_OZONE_X11()
#endif

}  // namespace views

#endif  // UI_VIEWS_VIEWS_TEST_SUITE_H_
