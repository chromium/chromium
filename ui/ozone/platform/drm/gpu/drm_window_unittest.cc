// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_window.h"

#include <drm_fourcc.h>
#include <stdint.h>
#include <xf86drm.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/page_flip_watchdog.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {

namespace {

// Mode of size 6x4.
const drmModeModeInfo kDefaultMode = {.hdisplay = 6, .vdisplay = 4};

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;
const int kDefaultCursorSize = 64;

std::vector<sk_sp<SkSurface>> GetCursorBuffers(
    const scoped_refptr<FakeDrmDevice> drm) {
  std::vector<sk_sp<SkSurface>> cursor_buffers;
  for (const auto& pair : drm->buffers()) {
    const auto& cursor_buffer = pair.second;
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
  DrmWindowTest() = default;

  DrmWindowTest(const DrmWindowTest&) = delete;
  DrmWindowTest& operator=(const DrmWindowTest&) = delete;

  void SetUp() override;
  void TearDown() override;

  void OnSubmission(gfx::SwapResult result, gfx::GpuFenceHandle release_fence) {
    last_swap_buffers_result_ = result;
  }

  void OnPresentation(const gfx::PresentationFeedback& feedback) {
    if (!feedback.failed())
      on_successful_swap_buffers_count_++;
    last_presentation_feedback_ = feedback;
  }

  scoped_refptr<DrmFramebuffer> CreateBuffer() {
    const gfx::Size window_size = ModeSize(kDefaultMode);
    std::unique_ptr<GbmBuffer> buffer = drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, window_size, GBM_BO_USE_SCANOUT);
    return DrmFramebuffer::AddFramebuffer(drm_, buffer.get(), window_size);
  }

 protected:
  void InitializeDrmState(FakeDrmDevice* drm, bool is_atomic = true);

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  scoped_refptr<FakeDrmDevice> drm_;
  scoped_refptr<FakeDrmDevice> drm2_;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<DrmDeviceManager> drm_device_manager_;

  int on_successful_swap_buffers_count_;
  gfx::SwapResult last_swap_buffers_result_;
  gfx::PresentationFeedback last_presentation_feedback_;

  uint32_t crtc_id_ = 0;
  uint32_t connector_id_ = 0;
};

void DrmWindowTest::SetUp() {
  on_successful_swap_buffers_count_ = 0;
  last_swap_buffers_result_ = gfx::SwapResult::SWAP_FAILED;

  auto gbm_device = std::make_unique<MockGbmDevice>();
  drm_ = new FakeDrmDevice(std::move(gbm_device));
  screen_manager_ = std::make_unique<ScreenManager>();

  InitializeDrmState(drm_.get());
  crtc_id_ = drm_->crtc_property(0).id;
  connector_id_ = drm_->connector_property(0).id;

  screen_manager_->AddDisplayController(drm_, crtc_id_, connector_id_);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      1 /*display_id*/, drm_, crtc_id_, connector_id_, gfx::Point(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  drm_device_manager_ = std::make_unique<DrmDeviceManager>(nullptr);

  std::unique_ptr<DrmWindow> window(new DrmWindow(
      kDefaultWidgetHandle, drm_device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  window->SetBounds(
      gfx::Rect(gfx::Size(kDefaultMode.hdisplay, kDefaultMode.vdisplay)));
  screen_manager_->AddWindow(kDefaultWidgetHandle, std::move(window));

  // Secondary DrmDevice for test cases that need it.
  drm2_ = new FakeDrmDevice(std::make_unique<MockGbmDevice>());
}

void DrmWindowTest::TearDown() {
  std::unique_ptr<DrmWindow> window =
      screen_manager_->RemoveWindow(kDefaultWidgetHandle);
  window->Shutdown();
  // Ensure DrmWindow is destroyed before ScreenManager.
  window = nullptr;

  screen_manager_ = nullptr;
  drm_->ResetPlaneManagerForTesting();
  drm2_->ResetPlaneManagerForTesting();
}

void DrmWindowTest::InitializeDrmState(FakeDrmDevice* drm, bool is_atomic) {
  drm->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  drm->InitializeState(/*use_atomic=*/false);
}

TEST_F(DrmWindowTest, SetCursorImage) {
  const gfx::Size cursor_size(6, 4);
  screen_manager_->GetWindow(kDefaultWidgetHandle)
      ->SetCursor(std::vector<SkBitmap>(1, AllocateBitmap(cursor_size)),
                  gfx::Point(4, 2), base::TimeDelta());

  SkBitmap cursor;
  std::vector<sk_sp<SkSurface>> cursor_buffers = GetCursorBuffers(drm_);
  EXPECT_EQ(2u, cursor_buffers.size());

  // Buffers 0 is the cursor backbuffer we just drew in.
  cursor.allocPixels(cursor_buffers[0]->getCanvas()->imageInfo());
  EXPECT_TRUE(cursor_buffers[0]->getCanvas()->readPixels(cursor, 0, 0));

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
                  gfx::Point(4, 2), base::TimeDelta());

  // Add another device.
  InitializeDrmState(drm2_.get());

  screen_manager_->AddDisplayController(drm2_, crtc_id_, connector_id_);

  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      /*display_id=*/2, drm2_, crtc_id_, connector_id_,
      gfx::Point(0, kDefaultMode.vdisplay),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, {display::ModesetFlag::kTestModeset,
                              display::ModesetFlag::kCommitModeset});

  // Move window to the display on the new device.
  screen_manager_->GetWindow(kDefaultWidgetHandle)
      ->SetBounds(gfx::Rect(0, kDefaultMode.vdisplay, kDefaultMode.hdisplay,
                            kDefaultMode.vdisplay));

  EXPECT_EQ(2u, GetCursorBuffers(drm2_).size());
  // Make sure the cursor is showing on the new display.
  EXPECT_NE(0u, drm2_->get_cursor_handle_for_crtc(crtc_id_));
}

TEST_F(DrmWindowTest, CheckPageflipSuccessOnSuccessfulSwap) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  // Window was re-sized, so the expectation is to re-create the buffers first.
  DrmWindow* window = screen_manager_->GetWindow(kDefaultWidgetHandle);
  drm_->set_page_flip_expectation(false);
  window->SchedulePageFlip(
      DrmOverlayPlane::Clone(planes),
      base::BindOnce(&DrmWindowTest::OnSubmission, base::Unretained(this)),
      base::BindOnce(&DrmWindowTest::OnPresentation, base::Unretained(this)));
  drm_->RunCallbacks();
  EXPECT_EQ(0, on_successful_swap_buffers_count_);
  EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS,
            last_swap_buffers_result_);
  EXPECT_EQ(static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kFailure),
            last_presentation_feedback_.flags);

  // Page flip succeeds, so GPU self-destruct should not engage.
  drm_->set_page_flip_expectation(true);
  window->SchedulePageFlip(
      DrmOverlayPlane::Clone(planes),
      base::BindOnce(&DrmWindowTest::OnSubmission, base::Unretained(this)),
      base::BindOnce(&DrmWindowTest::OnPresentation, base::Unretained(this)));
  drm_->RunCallbacks();
  EXPECT_EQ(1, on_successful_swap_buffers_count_);
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_buffers_result_);

  // Ensure self-destruct time runs out without process death.
  task_environment_.FastForwardBy(kWaitForModesetTimeout);
}

TEST_F(DrmWindowTest, CheckPageflipFailureOnFailedSwap) {
  DrmOverlayPlaneList planes;
  planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

  // Window was re-sized, so the expectation is to re-create the buffers first.
  DrmWindow* window = screen_manager_->GetWindow(kDefaultWidgetHandle);
  drm_->set_page_flip_expectation(false);
  window->SchedulePageFlip(
      DrmOverlayPlane::Clone(planes),
      base::BindOnce(&DrmWindowTest::OnSubmission, base::Unretained(this)),
      base::BindOnce(&DrmWindowTest::OnPresentation, base::Unretained(this)));
  drm_->RunCallbacks();
  EXPECT_EQ(0, on_successful_swap_buffers_count_);
  EXPECT_EQ(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS,
            last_swap_buffers_result_);
  EXPECT_EQ(static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kFailure),
            last_presentation_feedback_.flags);

  // Page flip still fails, so we expect GPU self-destruct timer to kick in.
  window->SchedulePageFlip(
      DrmOverlayPlane::Clone(planes),
      base::BindOnce(&DrmWindowTest::OnSubmission, base::Unretained(this)),
      base::BindOnce(&DrmWindowTest::OnPresentation, base::Unretained(this)));
  drm_->RunCallbacks();
  EXPECT_EQ(0, on_successful_swap_buffers_count_);
  EXPECT_EQ(gfx::SwapResult::SWAP_FAILED, last_swap_buffers_result_);
  EXPECT_EQ(static_cast<uint32_t>(gfx::PresentationFeedback::Flags::kFailure),
            last_presentation_feedback_.flags);

  // Since no modeset event was detected, death occurs after
  // |kWaitForModesetTimeout| seconds.
  const std::string gpu_crash_log =
      "Failed to modeset within " +
      base::NumberToString(kWaitForModesetTimeout.InSeconds()) +
      " s of the first page flip failure. Crashing GPU process.";
  EXPECT_DEATH_IF_SUPPORTED(
      task_environment_.FastForwardBy(kWaitForModesetTimeout), gpu_crash_log);
}

}  // namespace ui
