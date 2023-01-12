// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/chromeos_buildflags.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
#error This test target only builds with linux-chromeos, not for real ChromeOS\
 devices. See comment in build/config/chromeos/args.gni.
#endif

namespace {

class UIChromeOSTestSuite : public base::TestSuite {
 public:
  UIChromeOSTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  UIChromeOSTestSuite(const UIChromeOSTestSuite&) = delete;
  UIChromeOSTestSuite& operator=(const UIChromeOSTestSuite&) = delete;

  ~UIChromeOSTestSuite() override {}

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();
    gl::GLSurfaceTestSupport::InitializeOneOff();
    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);
    env_ = aura::Env::CreateInstance();
  }

  void Shutdown() override {
    env_.reset();
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }

 private:
  std::unique_ptr<aura::Env> env_;
};

}  // namespace

int main(int argc, char** argv) {
  UIChromeOSTestSuite test_suite(argc, argv);

  mojo::core::Init();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&UIChromeOSTestSuite::Run, base::Unretained(&test_suite)));
}
