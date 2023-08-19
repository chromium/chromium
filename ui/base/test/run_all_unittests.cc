// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/test/mock_chrome_application_mac.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/dpi.h"
#endif

namespace {

class UIBaseTestSuite : public base::TestSuite {
 public:
  UIBaseTestSuite(int argc, char** argv);

  UIBaseTestSuite(const UIBaseTestSuite&) = delete;
  UIBaseTestSuite& operator=(const UIBaseTestSuite&) = delete;

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;
};

UIBaseTestSuite::UIBaseTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

void UIBaseTestSuite::Initialize() {
  base::TestSuite::Initialize();

#if BUILDFLAG(IS_WIN)
  display::win::SetDefaultDeviceScaleFactor(1.0);
#endif

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

#elif BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  // On iOS, the ui_base_unittests binary is itself a mini bundle, with
  // resources built in. On Android, ui_base_unittests_apk provides the
  // necessary framework.
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", NULL, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

#else
  // On other platforms, the (hardcoded) paths for chrome_100_percent.pak and
  // locale.pak get populated by later build steps. To avoid clobbering them,
  // load the test .pak files directly.
  base::FilePath assets_path;
  base::PathService::Get(base::DIR_ASSETS, &assets_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      assets_path.AppendASCII("ui_test.pak"));

  // ui_base_unittests can't depend on the locales folder which Chrome will make
  // later, so use the path created by ui_test_pak.
  base::PathService::Override(ui::DIR_LOCALES, assets_path.AppendASCII("ui"));
#endif

  base::FilePath dir_resources;
  bool result;
#if BUILDFLAG(IS_ANDROID)
  result =
      base::PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &dir_resources);
#elif BUILDFLAG(IS_MAC)
  result = base::PathService::Get(base::DIR_MODULE, &dir_resources);
#elif BUILDFLAG(IS_IOS)
  result = base::PathService::Get(base::DIR_ASSETS, &dir_resources);
#else
  dir_resources = assets_path;
  result = true;
#endif
  DCHECK(result);
  base::FilePath ui_base_test_resources_pak =
      dir_resources.Append(FILE_PATH_LITERAL("ui_base_test_resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      ui_base_test_resources_pak, ui::kScaleFactorNone);
}

void UIBaseTestSuite::Shutdown() {
  ui::ResourceBundle::CleanupSharedInstance();

#if BUILDFLAG(IS_MAC)
  base::apple::SetOverrideFrameworkBundlePath({});
#endif
  base::TestSuite::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  UIBaseTestSuite test_suite(argc, argv);

  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&UIBaseTestSuite::Run, base::Unretained(&test_suite)));
}
