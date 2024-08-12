// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_configuration_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/drm/common/display_types.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_overlay_plane.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
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

    auto [path, fd] = ui::test::FindDrmDriverOrDie("vkms");
    drm_device_->AddGraphicsDevice(path, mojo::PlatformHandle(std::move(fd)));
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

  ui::MovableDisplaySnapshots SetupDisplaysHorizontally() {
    auto displays = RefreshDisplays();
    std::vector<display::DisplayConfigurationParams> params;
    uint32_t x = 0;
    for (auto& display : displays) {
      const auto* mode = display->modes()[0].get();
      params.emplace_back(display->display_id(), gfx::Point(x, 0), mode);
      x += mode->size().width();
    }

    base::RunLoop run_loop;
    auto callback = base::BindLambdaForTesting(
        [&run_loop](const std::vector<display::DisplayConfigurationParams>&
                        request_results,
                    bool success) {
          EXPECT_TRUE(success) << "Unable to set up displays.";
          run_loop.Quit();
        });
    drm_device_->ConfigureNativeDisplays(params,
                                         {display::ModesetFlag::kTestModeset,
                                          display::ModesetFlag::kCommitModeset},
                                         callback);
    run_loop.Run();

    return RefreshDisplays();
  }

  void FlipExpectingRecreateBuffers(
      const std::unique_ptr<ui::DrmWindowProxy>& window_proxy) {
    base::RunLoop run_loop;
    auto submission_callback =
        base::BindOnce([](gfx::SwapResult result, gfx::GpuFenceHandle handle) {
          EXPECT_EQ(result, gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS);
        });
    auto presentation_callback = base::BindLambdaForTesting(
        [&run_loop](const gfx::PresentationFeedback& feedback) {
          EXPECT_TRUE(feedback.failed());
          run_loop.Quit();
        });
    auto planes = std::vector<ui::DrmOverlayPlane>();
    window_proxy->SchedulePageFlip(std::move(planes),
                                   std::move(submission_callback),
                                   std::move(presentation_callback));
    run_loop.Run();
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

TEST_F(VKMSTest, SinglePlanePageFlip) {
  auto displays = SetupDisplaysHorizontally();

  constexpr gfx::AcceleratedWidget kWidget = 1;
  const auto kWindowRect =
      gfx::Rect(displays[0]->origin(), displays[0]->current_mode()->size());

  drm_device_->CreateWindow(kWidget, kWindowRect);
  auto window_proxy = drm_thread_proxy_->CreateDrmWindowProxy(kWidget);
  FlipExpectingRecreateBuffers(window_proxy);

  // Note that as of 2022/04, the only vkms-supported buffers are RGB. Note that
  // the BufferFormat order is flipped from DRM. See here for our conversions:
  // ui/gfx/linux/drm_util_linux.cc
  std::unique_ptr<ui::GbmBuffer> buffer;
  scoped_refptr<ui::DrmFramebuffer> framebuffer;
  drm_thread_proxy_->CreateBuffer(
      kWidget, kWindowRect.size(), kWindowRect.size(),
      gfx::BufferFormat::BGRX_8888, gfx::BufferUsage::SCANOUT_CPU_READ_WRITE,
      /*flags=*/0, &buffer, &framebuffer);

  auto planes = std::vector<ui::DrmOverlayPlane>();
  planes.push_back(ui::DrmOverlayPlane::TestPlane(
      framebuffer, gfx::ColorSpace::CreateSRGB(),
      std::make_unique<gfx::GpuFence>(gfx::GpuFenceHandle())));

  base::RunLoop run_loop;
  auto submission_callback =
      base::BindOnce([](gfx::SwapResult result, gfx::GpuFenceHandle handle) {
        EXPECT_EQ(result, gfx::SwapResult::SWAP_ACK);
      });
  auto presentation_callback = base::BindLambdaForTesting(
      [&](const gfx::PresentationFeedback& feedback) {
        EXPECT_FALSE(feedback.failed());
        run_loop.Quit();
      });
  window_proxy->SchedulePageFlip(std::move(planes),
                                 std::move(submission_callback),
                                 std::move(presentation_callback));
  run_loop.Run();

  drm_device_->DestroyWindow(kWidget);
}
