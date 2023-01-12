// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace {

class UIAndroidTestSuite : public base::TestSuite {
 public:
  UIAndroidTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  UIAndroidTestSuite(const UIAndroidTestSuite&) = delete;
  UIAndroidTestSuite& operator=(const UIAndroidTestSuite&) = delete;

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();

    ui::RegisterPathProvider();

    ui::ResourceBundle::InitSharedInstanceWithPakPath(base::FilePath());
  }

  void Shutdown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }
};

}  // namespace

int main(int argc, char** argv) {
  UIAndroidTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&UIAndroidTestSuite::Run, base::Unretained(&test_suite)));
}
