// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/test/mock_chrome_application_mac.h"
#endif

#if defined(USE_OZONE)
#include "base/command_line.h"
#include "mojo/core/embedder/embedder.h"                  // nogncheck
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace {
class GlTestSuite : public base::TestSuite {
 public:
  GlTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {
  }

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

#if defined(OS_MACOSX) && !defined(OS_IOS)
    // This registers a custom NSApplication. It must be done before
    // TaskEnvironment registers a regular NSApplication.
    mock_cr_app::RegisterMockCrApp();
#endif

    task_environment_ = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::UI);

#if defined(USE_OZONE)
    // Make Ozone run in single-process mode, where it doesn't expect a GPU
    // process and it spawns and starts its own DRM thread. Note that this mode
    // still requires a mojo pipe for in-process communication between the host
    // and GPU components.
    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    params.using_mojo = true;

    // This initialization must be done after TaskEnvironment has
    // initialized the UI thread.
    ui::OzonePlatform::InitializeForUI(params);
#endif
  }

  void Shutdown() override {
    base::TestSuite::Shutdown();
  }

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

  DISALLOW_COPY_AND_ASSIGN(GlTestSuite);
};

}  // namespace

int main(int argc, char** argv) {
#if defined(USE_OZONE)
  mojo::core::Init();
#endif

  GlTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&GlTestSuite::Run, base::Unretained(&test_suite)));
}
