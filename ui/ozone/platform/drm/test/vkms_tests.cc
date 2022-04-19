// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/platform/drm/test/integration_test_helpers.h"

class VKMSTest : public testing::Test {
 public:
  VKMSTest() = default;

  void SetUp() override {
    base::RunLoop run_loop;
    drm_thread_proxy_ = std::make_unique<ui::DrmThreadProxy>();
    drm_thread_proxy_->StartDrmThread(run_loop.QuitClosure());
    drm_thread_proxy_->WaitUntilDrmThreadStarted();
    drm_thread_proxy_->AddDrmDeviceReceiver(
        drm_device_.BindNewPipeAndPassReceiver());
    run_loop.Run();

    auto [path, file] = ui::test::FindDrmDriverOrDie("vkms");
    drm_device_->AddGraphicsDevice(path, std::move(file));
  }

 protected:
  ui::MovableDisplaySnapshots RefreshDisplays() {
    ui::MovableDisplaySnapshots output;

    base::RunLoop run_loop;
    auto callback =
        base::BindLambdaForTesting([&](ui::MovableDisplaySnapshots snapshots) {
          output = std::move(snapshots);
          run_loop.Quit();
        });
    drm_device_->RefreshNativeDisplays(callback);
    run_loop.Run();

    return output;
  }

  // This should be first, so that it is destructed last. The DRM thread
  // interacts with current IO thread when it is destructed.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};

  std::unique_ptr<ui::DrmThreadProxy> drm_thread_proxy_ = nullptr;
  mojo::Remote<ui::ozone::mojom::DrmDevice> drm_device_;
};

TEST_F(VKMSTest, DisplaysAreAvailable) {
  auto snapshots = RefreshDisplays();
  EXPECT_GT(snapshots.size(), 0ul);
}
