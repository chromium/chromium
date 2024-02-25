// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_test_suite.h"

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/chromeos_buildflags.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/provide_ax_platform_for_tests.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if defined(USE_AURA)
#include <memory>

#include "ui/aura/env.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/command_line.h"
#include "ui/gl/gl_switches.h"
#endif

namespace views {

ViewsTestSuite::ViewsTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv), argc_(argc), argv_(argv) {}

ViewsTestSuite::~ViewsTestSuite() = default;

int ViewsTestSuite::RunTests() {
  return base::LaunchUnitTests(
      argc_, argv_,
      base::BindOnce(&ViewsTestSuite::Run, base::Unretained(this)));
}

int ViewsTestSuite::RunTestsSerially() {
  return base::LaunchUnitTestsSerially(
      argc_, argv_,
      base::BindOnce(&ViewsTestSuite::Run, base::Unretained(this)));
}

void ViewsTestSuite::Initialize() {
  base::TestSuite::Initialize();

  testing::UnitTest::GetInstance()->listeners().Append(
      new ui::ProvideAXPlatformForTests());

#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
  // Force software-gl. This is necessary for mus tests to avoid an msan warning
  // in gl init.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kOverrideUseSoftwareGLForTests);
#endif

  gl::GLSurfaceTestSupport::InitializeOneOff();

  ui::RegisterPathProvider();

  base::FilePath ui_test_pak_path;
  ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
#if defined(USE_AURA)
  InitializeEnv();
#endif
}

void ViewsTestSuite::Shutdown() {
#if defined(USE_AURA)
  DestroyEnv();
#endif
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

#if defined(USE_AURA)
void ViewsTestSuite::InitializeEnv() {
  env_ = aura::Env::CreateInstance();
}

void ViewsTestSuite::DestroyEnv() {
  env_.reset();
}
#endif

}  // namespace views
