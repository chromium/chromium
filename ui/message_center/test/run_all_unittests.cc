// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/provide_ax_platform_for_tests.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace {

class MessageCenterTestSuite : public base::TestSuite {
 public:
  MessageCenterTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

  MessageCenterTestSuite(const MessageCenterTestSuite&) = delete;
  MessageCenterTestSuite& operator=(const MessageCenterTestSuite&) = delete;

 protected:
  void Initialize() override {
    gl::GLSurfaceTestSupport::InitializeOneOff();
    base::TestSuite::Initialize();
    ui::RegisterPathProvider();

    base::FilePath ui_test_pak_path;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak_path));
    ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
    testing::UnitTest::GetInstance()->listeners().Append(
        new ui::ProvideAXPlatformForTests());
  }

  void Shutdown() override {
    ui::ResourceBundle::CleanupSharedInstance();
    base::TestSuite::Shutdown();
  }

 private:
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

}  // namespace

int main(int argc, char** argv) {
  mojo::core::Init();

  MessageCenterTestSuite test_suite(argc, argv);

  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&MessageCenterTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
