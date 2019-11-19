// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_window.h"

#include <drm_fourcc.h>
#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/platform_file.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/common/linux/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_gbm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace {

// Mode of size 6x4.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;
const uint32_t kDefaultCrtc = 1;
const uint32_t kDefaultConnector = 2;
const int kDefaultCursorSize = 64;

std::vector<sk_sp<SkSurface>> GetCursorBuffers(
    const scoped_refptr<ui::MockDrmDevice> drm) {
  std::vector<sk_sp<SkSurface>> cursor_buffers;
  for (const auto& cursor_buffer : drm->buffers()) {
    if (cursor_buffer && cursor_buffer->width() == kDefaultCursorSize &&
        cursor_buffer->height() == kDefaultCursorSize) {
      cursor_buffers.push_back(cursor_buffer);
    }
  }

  return cursor_buffers;
}

SkBitmap AllocateBitmap(const gfx::Size& size) {
  SkBitmap image;
  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(),
                                       kN32_SkColorType, kPremul_SkAlphaType);
  image.allocPixels(info);
  image.eraseColor(SK_ColorWHITE);
  return image;
}

}  // namespace

class DrmWindowTest : public testing::Test {
 public:
  DrmWindowTest() {}

  void SetUp() override;
  void TearDown() override;

  void OnSubmission(gfx::SwapResult result,
                    std::unique_ptr<gfx::GpuFence> out_fence) {
    last_swap_buffers_result_ = result;
  }

  void OnPresentation(const gfx::PresentationFeedback& feedback) {
    on_swap_buffers_count_++;
    last_presentation_feedback_ = feedback;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  scoped_refptr<ui::MockDrmDevice> drm_;
  std::unique_ptr<ui::ScreenManager> screen_manager_;
  std::unique_ptr<ui::DrmDeviceManager> drm_device_manager_;

  int on_swap_buffers_count_;
  gfx::SwapResult last_swap_buffers_result_;
  gfx::PresentationFeedback last_presentation_feedback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DrmWindowTest);
};

void DrmWindowTest::SetUp() {
  on_swap_buffers_count_ = 0;
  last_swap_buffers_result_ = gfx::SwapResult::SWAP_FAILED;

  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  drm_ = new ui::MockDrmDevice(std::move(gbm_device));
  screen_manager_ = std::make_unique<ui::ScreenManager>();
  screen_manager_->AddDisplayController(drm_, kDefaultCrtc, kDefaultConnector);
  screen_manager_->ConfigureDisplayController(
      drm_, kDefaultCrtc, kDefaultConnector, gfx::Point(), kDefaultMode);

  drm_device_manager_ = std::make_unique<ui::DrmDeviceManager>(nullptr);

  std::unique_ptr<ui::DrmWindow> window(new ui::DrmWindow(
      kDefaultWidgetHandle, drm_device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(
      gfx::Rect(gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay)));
  screen_manager_->AddWindow(kDefaultWidgetHandle, std::move(window));
}

void DrmWindowTest::TearDown() {
  std::unique_ptr<ui::DrmWindow> window =
      screen_manager_->RemoveWindow(kDefaultWidgetHandle);
  window->Shutdown();
}

TEST_F(DrmWindowTest, SetCursorImage) {
  const gfx::Size cursor_size(6, 4);
  screen_manager_->GetWindow(kDefaultWidgetHandle)
      ->SetCursor(std::vector<SkBitmap>(1, AllocateBitmap(cursor_size)),
                  gfx::Point(4, 2), 0);

  SkBitmap cursor;
  std::vector<sk_sp<SkSurface>> cursor_buffers = GetCursorBuffers(drm_);
  EXPECT_EQ(2u, cursor_buffers.size());

  // Buffers 1 is the cursor backbuffer we just drew in.
  cursor.allocPixels(cursor_buffers[1]->getCanvas()->imageInfo());
  EXPECT_TRUE(cursor_buffers[1]->getCanvas()->readPixels(cursor, 0, 0));

  // Check that the frontbuffer is displaying the right image as set above.
  for (int i = 0; i < cursor.height(); ++i) {
    for (int j = 0; j < cursor.width(); ++j) {
      if (j < cursor_size.width() && i < cursor_size.height())
        EXPECT_EQ(SK_ColorWHITE, cursor.getColor(j, i));
      else
        EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT),
                  cursor.getColor(j, i));
    }
  }
}

TEST_F(DrmWindowTest, CheckCursorSurfaceAfterChangingDevice) {
  const gfx::Size cursor_size(6, 4);
  screen_manager_->GetWindow(kDefaultWidgetHandle)
      ->SetCursor(std::vector<SkBitmap>(1, AllocateBitmap(cursor_size)),
                  gfx::Point(4, 2), 0);

  // Add another device.
  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  scoped_refptr<ui::MockDrmDevice> drm =
      new ui::MockDrmDevice(std::move(gbm_device));
  screen_manager_->AddDisplayController(drm, kDefaultCrtc, kDefaultConnector);
  screen_manager_->ConfigureDisplayController(
      drm, kDefaultCrtc, kDefaultConnector,
      gfx::Point(0, kDefaultMode.vdisplay), kDefaultMode);

  // Move window to the display on the new device.
  screen_manager_->GetWindow(kDefaultWidgetHandle)
      ->SetBounds(gfx::Rect(0, kDefaultMode.vdisplay, kDefaultMode.hdisplay,
                            kDefaultMode.vdisplay));

  EXPECT_EQ(2u, GetCursorBuffers(drm).size());
  // Make sure the cursor is showing on the new display.
  EXPECT_NE(0u, drm->get_cursor_handle_for_crtc(kDefaultCrtc));
}

TEST_F(DrmWindowTest, CheckDeathOnFailedSwap) {
  const gfx::Size window_size(6, 4);
  ui::DrmWindow* window = screen_manager_->GetWindow(kDefaultWidgetHandle);

  std::unique_ptr<ui::GbmBuffer> buffer = drm_->gbm_device()->CreateBuffer(
      DRM_FORMAT_XRGB8888, window_size, GBM_BO_USE_SCANOUT);
  scoped_refptr<ui::DrmFramebuffer> framebuffer =
      ui::DrmFramebuffer::AddFramebuffer(drm_, buffer.get());
  ui::DrmOverlayPlane plane(framebuffer, nullptr);

  drm_->set_page_flip_expectation(false);

  ui::DrmOverlayPlaneList planes;
  planes.push_back(plane.Clone());

  // Window was re-sized, so the expectation is to re-create the buffers first.
  window->SchedulePageFlip(
      ui::DrmOverlayPlane::Clone(planes),
      base::BindOnce(&DrmWindowTest::OnSubmission, base::Unretained(this)),
      base::BindOnce(&DrmWindowTest::OnPresentation, base::Unretained(this)));
  EXPECT_EQ(1, on_swap_buffers_count_);
  EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS,
            last_swap_buffers_result_);
  EXPECT_EQ(static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kFailure),
            last_presentation_feedback_.flags);

  EXPECT_DEATH_IF_SUPPORTED(
      window->SchedulePageFlip(
          ui::DrmOverlayPlane::Clone(planes),
          base::BindOnce(&DrmWindowTest::OnSubmission, base::Unretained(this)),
          base::BindOnce(&DrmWindowTest::OnPresentation,
                         base::Unretained(this))),
      "SchedulePageFlip failed");
}
