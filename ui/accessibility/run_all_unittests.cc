// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/accessibility/platform/provide_ax_platform_for_tests.h"

namespace {

class AccessibilityTestSuite : public base::TestSuite {
 public:
  AccessibilityTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  // base::TestSuite
  void Initialize() override {
    base::TestSuite::Initialize();
    testing::UnitTest::GetInstance()->listeners().Append(
        new ui::ProvideAXPlatformForTests());
  }
};

}  // namespace

int main(int argc, char** argv) {
  mojo::core::Init();

  AccessibilityTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&AccessibilityTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
