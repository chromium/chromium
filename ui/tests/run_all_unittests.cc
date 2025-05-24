// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/test/mock_chrome_application_mac.h"
#endif

namespace {

class UITestSuite : public base::TestSuite {
 public:
  UITestSuite(int argc, char** argv);

  UITestSuite(const UITestSuite&) = delete;
  UITestSuite& operator=(const UITestSuite&) = delete;

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;
};

UITestSuite::UITestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

void UITestSuite::Initialize() {
  base::TestSuite::Initialize();

  ui::RegisterPathProvider();

#if BUILDFLAG(IS_MAC)
  base::FilePath exe_path;
  base::PathService::Get(base::DIR_EXE, &exe_path);

  mock_cr_app::RegisterMockCrApp();

  // On Mac, a test Framework bundle is created that links locale.pak and
  // chrome_100_percent.pak at the appropriate places to ui_test.pak.
  base::apple::SetOverrideFrameworkBundlePath(
      exe_path.AppendASCII("ui_unittests Framework.framework"));
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
#endif
}

void UITestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();

#if BUILDFLAG(IS_MAC)
  base::apple::SetOverrideFrameworkBundlePath({});
#endif
  base::TestSuite::Shutdown();
}

}  // namespace

int main(int argc, char* argv[]) {
  UITestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
