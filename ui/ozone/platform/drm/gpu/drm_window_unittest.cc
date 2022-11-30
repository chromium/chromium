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

#include "base/bind.h"
#include "base/files/platform_file.h"
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
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/page_flip_watchdog.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace {

// Mode of size 6x4.
const drmModeModeInfo kDefaultMode = {.hdisplay = 6, .vdisplay = 4};

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

  scoped_refptr<ui::DrmFramebuffer> CreateBuffer() {
    const gfx::Size window_size = ui::ModeSize(kDefaultMode);
    std::unique_ptr<ui::GbmBuffer> buffer = drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, window_size, GBM_BO_USE_SCANOUT);
    return ui::DrmFramebuffer::AddFramebuffer(drm_, buffer.get(), window_size);
  }

 protected:
  void InitializeDrmState(ui::MockDrmDevice* drm, bool is_atomic = true);

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  scoped_refptr<ui::MockDrmDevice> drm_;
  std::unique_ptr<ui::ScreenManager> screen_manager_;
  std::unique_ptr<ui::DrmDeviceManager> drm_device_manager_;

  int on_successful_swap_buffers_count_;
  gfx::SwapResult last_swap_buffers_result_;
  gfx::PresentationFeedback last_presentation_feedback_;

 private:
  struct PlaneState {
    std::vector<uint32_t> formats;
  };

  struct CrtcState {
    std::vector<PlaneState> planes;
  };
};

void DrmWindowTest::SetUp() {
  on_successful_swap_buffers_count_ = 0;
  last_swap_buffers_result_ = gfx::SwapResult::SWAP_FAILED;

  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  drm_ = new ui::MockDrmDevice(std::move(gbm_device));
  screen_manager_ = std::make_unique<ui::ScreenManager>();

  InitializeDrmState(drm_.get());

  screen_manager_->AddDisplayController(drm_, kDefaultCrtc, kDefaultConnector);
  std::vector<ui::ScreenManager::ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      1 /*display_id*/, drm_, kDefaultCrtc, kDefaultConnector, gfx::Point(),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

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

void DrmWindowTest::InitializeDrmState(ui::MockDrmDevice* drm, bool is_atomic) {
  // A Sample of CRTC states.
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}}};

  constexpr uint32_t kPlaneIdBase = 300;
  constexpr uint32_t kInFormatsBlobPropIdBase = 400;

  std::vector<ui::MockDrmDevice::CrtcProperties> crtc_properties(
      crtc_states.size());
  std::map<uint32_t, std::string> crtc_property_names = {
      {1000, "ACTIVE"},
      {1001, "MODE_ID"},
  };

  std::vector<ui::MockDrmDevice::ConnectorProperties> connector_properties(3);
  std::map<uint32_t, std::string> connector_property_names = {
      {2000, "CRTC_ID"},
  };
  for (size_t i = 0; i < connector_properties.size(); ++i) {
    connector_properties[i].id = kDefaultConnector + i;
    for (const auto& pair : connector_property_names) {
      connector_properties[i].properties.push_back(
          {.id = pair.first, .value = 0});
    }
  }

  std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties;
  std::map<uint32_t, std::string> plane_property_names = {
      // Add all required properties.
      {3000, "CRTC_ID"},
      {3001, "CRTC_X"},
      {3002, "CRTC_Y"},
      {3003, "CRTC_W"},
      {3004, "CRTC_H"},
      {3005, "FB_ID"},
      {3006, "SRC_X"},
      {3007, "SRC_Y"},
      {3008, "SRC_W"},
      {3009, "SRC_H"},
      // Defines some optional properties we use for convenience.
      {3010, "type"},
      {3011, "IN_FORMATS"},
  };

  uint32_t plane_id = kPlaneIdBase;
  uint32_t property_id = kInFormatsBlobPropIdBase;

  for (size_t crtc_idx = 0; crtc_idx < crtc_states.size(); ++crtc_idx) {
    crtc_properties[crtc_idx].id = kDefaultCrtc + crtc_idx;
    for (const auto& pair : crtc_property_names) {
      crtc_properties[crtc_idx].properties.push_back(
          {.id = pair.first, .value = 0});
    }

    std::vector<ui::MockDrmDevice::PlaneProperties> crtc_plane_properties(
        crtc_states[crtc_idx].planes.size());
    for (size_t plane_idx = 0; plane_idx < crtc_states[crtc_idx].planes.size();
         ++plane_idx) {
      crtc_plane_properties[plane_idx].id = plane_id++;
      crtc_plane_properties[plane_idx].crtc_mask = 1 << crtc_idx;

      for (const auto& pair : plane_property_names) {
        uint64_t value = 0;
        if (pair.first == 3010) {
          value =
              plane_idx == 0 ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
        } else if (pair.first == 3011) {
          value = property_id++;
          drm->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
              value, crtc_states[crtc_idx].planes[plane_idx].formats,
              std::vector<drm_format_modifier>()));
        } else if (pair.first >= 3001 && pair.first <= 3009) {
          value = 27;
        }

        crtc_plane_properties[plane_idx].properties.push_back(
            {.id = pair.first, .value = value});
      }
    }

    plane_properties.insert(plane_properties.end(),
                            crtc_plane_properties.begin(),
                            crtc_plane_properties.end());
  }

  std::map<uint32_t, std::string> property_names;
  property_names.insert(crtc_property_names.begin(), crtc_property_names.end());
  property_names.insert(connector_property_names.begin(),
                        connector_property_names.end());
  property_names.insert(plane_property_names.begin(),
                        plane_property_names.end());
  drm->InitializeState(crtc_properties, connector_properties, plane_properties,
                       property_names, /*is_atomic=*/false);
}

TEST_F(DrmWindowTest, SetCursorImage) {
  const gfx::Size cursor_size(6, 4);
  screen_manager_->GetWindow(kDefaultWidgetHandle)
      ->SetCursor(std::vector<SkBitmap>(1, AllocateBitmap(cursor_size)),
                  gfx::Point(4, 2), base::TimeDelta());

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
                  gfx::Point(4, 2), base::TimeDelta());

  // Add another device.
  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  scoped_refptr<ui::MockDrmDevice> drm =
      new ui::MockDrmDevice(std::move(gbm_device));
  InitializeDrmState(drm.get());

  screen_manager_->AddDisplayController(drm, kDefaultCrtc, kDefaultConnector);

  std::vector<ui::ScreenManager::ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      2 /*display_id*/, drm, kDefaultCrtc, kDefaultConnector,
      gfx::Point(0, kDefaultMode.vdisplay),
      std::make_unique<drmModeModeInfo>(kDefaultMode));
  screen_manager_->ConfigureDisplayControllers(
      controllers_to_enable, display::kTestModeset | display::kCommitModeset);

  // Move window to the display on the new device.
  screen_manager_->GetWindow(kDefaultWidgetHandle)
      ->SetBounds(gfx::Rect(0, kDefaultMode.vdisplay, kDefaultMode.hdisplay,
                            kDefaultMode.vdisplay));

  EXPECT_EQ(2u, GetCursorBuffers(drm).size());
  // Make sure the cursor is showing on the new display.
  EXPECT_NE(0u, drm->get_cursor_handle_for_crtc(kDefaultCrtc));
}

TEST_F(DrmWindowTest, CheckPageflipSuccessOnSuccessfulSwap) {
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);

  // Window was re-sized, so the expectation is to re-create the buffers first.
  ui::DrmWindow* window = screen_manager_->GetWindow(kDefaultWidgetHandle);
  drm_->set_page_flip_expectation(false);
  window->SchedulePageFlip(
      ui::DrmOverlayPlane::Clone(planes),
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
      ui::DrmOverlayPlane::Clone(planes),
      base::BindOnce(&DrmWindowTest::OnSubmission, base::Unretained(this)),
      base::BindOnce(&DrmWindowTest::OnPresentation, base::Unretained(this)));
  drm_->RunCallbacks();
  EXPECT_EQ(1, on_successful_swap_buffers_count_);
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_buffers_result_);

  // Ensure self-destruct time runs out without process death.
  task_environment_.FastForwardBy(ui::kWaitForModesetTimeout);
}

TEST_F(DrmWindowTest, CheckPageflipFailureOnFailedSwap) {
  ui::DrmOverlayPlaneList planes;
  planes.emplace_back(CreateBuffer(), nullptr);

  // Window was re-sized, so the expectation is to re-create the buffers first.
  ui::DrmWindow* window = screen_manager_->GetWindow(kDefaultWidgetHandle);
  drm_->set_page_flip_expectation(false);
  window->SchedulePageFlip(
      ui::DrmOverlayPlane::Clone(planes),
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
      ui::DrmOverlayPlane::Clone(planes),
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
      base::NumberToString(ui::kWaitForModesetTimeout.InSeconds()) +
      " s of the first page flip failure. Crashing GPU process.";
  EXPECT_DEATH_IF_SUPPORTED(
      task_environment_.FastForwardBy(ui::kWaitForModesetTimeout),
      gpu_crash_log);
}
