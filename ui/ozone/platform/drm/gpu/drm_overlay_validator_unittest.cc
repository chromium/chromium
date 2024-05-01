// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_validator.h"

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

namespace {

// Mode of size 12x8.
const drmModeModeInfo kDefaultMode = {.hdisplay = 12, .vdisplay = 8};

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;

}  // namespace

class DrmOverlayValidatorTest : public testing::Test {
 public:
  DrmOverlayValidatorTest() = default;

  DrmOverlayValidatorTest(const DrmOverlayValidatorTest&) = delete;
  DrmOverlayValidatorTest& operator=(const DrmOverlayValidatorTest&) = delete;

  void SetUp() override;
  void TearDown() override;

  void OnSwapBuffers(gfx::SwapResult result) {
    on_swap_buffers_count_++;
    last_swap_buffers_result_ = result;
  }

  scoped_refptr<DrmFramebuffer> ReturnNullBuffer(const gfx::Size& size,
                                                 uint32_t format) {
    return nullptr;
  }

  void AddPlane(const OverlaySurfaceCandidate& params);

  scoped_refptr<DrmFramebuffer> CreateBuffer() {
    auto gbm_buffer = drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, primary_rect_.size(), GBM_BO_USE_SCANOUT);
    return DrmFramebuffer::AddFramebuffer(drm_, gbm_buffer.get(),
                                          primary_rect_.size());
  }

  scoped_refptr<DrmFramebuffer> CreateOverlayBuffer(uint32_t format,
                                                    const gfx::Size& size) {
    auto gbm_buffer =
        drm_->gbm_device()->CreateBuffer(format, size, GBM_BO_USE_SCANOUT);
    return DrmFramebuffer::AddFramebuffer(drm_, gbm_buffer.get(), size);
  }

  bool ModesetController(HardwareDisplayController* controller) {
    CommitRequest commit_request;

    DrmOverlayPlaneList modeset_planes;
    modeset_planes.push_back(DrmOverlayPlane::TestPlane(CreateBuffer()));

    controller->GetModesetProps(&commit_request, modeset_planes, kDefaultMode,
                                /*enable_vrr=*/false);
    CommitRequest request_for_update = commit_request;
    bool status = drm_->plane_manager()->Commit(std::move(commit_request),
                                                DRM_MODE_ATOMIC_ALLOW_MODESET);

    for (const CrtcCommitRequest& crtc_request : request_for_update) {
      controller->UpdateState(crtc_request);
    }

    return status;
  }

  std::vector<HardwareDisplayPlane*> GetMovablePlanes() {
    std::vector<HardwareDisplayPlane*> planes;
    for (const auto& plane : drm_->plane_manager()->planes()) {
      if (plane->GetCompatibleCrtcIds().size() > 1) {
        planes.push_back(plane.get());
      }
    }
    return planes;
  }

 protected:
  struct PlaneState {
    std::vector<uint32_t> formats;
  };

  struct CrtcState {
    std::vector<PlaneState> planes;
  };

  void InitDrmStatesAndControllers(
      const std::vector<CrtcState>& crtc_states,
      const std::vector<PlaneState>& movable_planes = {});

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  scoped_refptr<FakeDrmDevice> drm_;
  raw_ptr<MockGbmDevice> gbm_ = nullptr;
  std::unique_ptr<ScreenManager> screen_manager_;
  std::unique_ptr<DrmDeviceManager> drm_device_manager_;
  raw_ptr<DrmWindow, DanglingUntriaged> window_;
  std::unique_ptr<DrmOverlayValidator> overlay_validator_;
  std::vector<OverlaySurfaceCandidate> overlay_params_;
  DrmOverlayPlaneList plane_list_;

  int on_swap_buffers_count_;
  gfx::SwapResult last_swap_buffers_result_;
  gfx::Rect overlay_rect_;
  gfx::Rect primary_rect_;

 private:
  void SetupControllers();
};

void DrmOverlayValidatorTest::SetUp() {
  on_swap_buffers_count_ = 0;
  last_swap_buffers_result_ = gfx::SwapResult::SWAP_FAILED;

  auto gbm = std::make_unique<MockGbmDevice>();
  gbm_ = gbm.get();
  drm_ = new FakeDrmDevice(std::move(gbm));
}

void DrmOverlayValidatorTest::InitDrmStatesAndControllers(
    const std::vector<CrtcState>& crtc_states,
    const std::vector<PlaneState>& movable_planes) {
  size_t plane_count = crtc_states[0].planes.size();
  for (const auto& crtc_state : crtc_states) {
    ASSERT_EQ(plane_count, crtc_state.planes.size())
        << "FakeDrmDevice::CreateStateWithDefaultObjects currently expects the "
           "same number of planes per CRTC";
  }

  drm_->ResetStateWithAllProperties();

  std::vector<uint32_t> crtc_ids;
  for (const auto& crtc_state : crtc_states) {
    uint32_t crtc_id = drm_->AddCrtcAndConnector().first.id;
    crtc_ids.push_back(crtc_id);

    for (size_t i = 0; i < crtc_state.planes.size(); ++i) {
      auto in_formats_blob =
          drm_->CreateInFormatsBlob(crtc_state.planes[i].formats, {});

      auto& plane = drm_->AddPlane(
          crtc_id, i == 0 ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY);
      drm_->AddProperty(
          plane.id, {.id = kInFormatsPropId, .value = in_formats_blob->id()});
    }
  }

  for (const auto& movable_plane : movable_planes) {
    auto in_formats_blob = drm_->CreateInFormatsBlob(movable_plane.formats, {});
    auto& plane = drm_->AddPlane(crtc_ids, DRM_PLANE_TYPE_OVERLAY);
    drm_->AddProperty(plane.id,
                      {.id = kInFormatsPropId, .value = in_formats_blob->id()});
  }

  drm_->InitializeState(/*use_atomic=*/true);

  SetupControllers();
}

void DrmOverlayValidatorTest::SetupControllers() {
  uint32_t primary_crtc_id = drm_->crtc_property(0).id;
  uint32_t primary_connector_id = drm_->connector_property(0).id;

  screen_manager_ = std::make_unique<ScreenManager>();
  screen_manager_->AddDisplayController(drm_, primary_crtc_id,
                                        primary_connector_id);
  std::vector<ControllerConfigParams> controllers_to_enable;
  controllers_to_enable.emplace_back(
      1 /*display_id*/, drm_, primary_crtc_id, primary_connector_id,
      gfx::Point(), std::make_unique<drmModeModeInfo>(kDefaultMode));
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
  window_ = screen_manager_->GetWindow(kDefaultWidgetHandle);
  overlay_validator_ = std::make_unique<DrmOverlayValidator>(window_);

  overlay_rect_ =
      gfx::Rect(0, 0, kDefaultMode.hdisplay / 2, kDefaultMode.vdisplay / 2);

  primary_rect_ = gfx::Rect(0, 0, kDefaultMode.hdisplay, kDefaultMode.vdisplay);

  OverlaySurfaceCandidate primary_candidate;
  primary_candidate.buffer_size = primary_rect_.size();
  primary_candidate.display_rect = gfx::RectF(primary_rect_);
  primary_candidate.is_opaque = true;
  primary_candidate.format = gfx::BufferFormat::BGRX_8888;
  primary_candidate.overlay_handled = true;
  overlay_params_.push_back(primary_candidate);
  AddPlane(primary_candidate);

  OverlaySurfaceCandidate overlay_candidate;
  overlay_candidate.buffer_size = overlay_rect_.size();
  overlay_candidate.display_rect = gfx::RectF(overlay_rect_);
  overlay_candidate.plane_z_order = 1;
  primary_candidate.is_opaque = true;
  overlay_candidate.format = gfx::BufferFormat::BGRX_8888;
  overlay_candidate.overlay_handled = true;
  overlay_params_.push_back(overlay_candidate);
  AddPlane(overlay_candidate);
}

void DrmOverlayValidatorTest::AddPlane(const OverlaySurfaceCandidate& params) {
  scoped_refptr<DrmDevice> drm = window_->GetController()->GetDrmDevice();

  scoped_refptr<DrmFramebuffer> drm_framebuffer = CreateOverlayBuffer(
      GetFourCCFormatFromBufferFormat(params.format), params.buffer_size);
  plane_list_.emplace_back(
      std::move(drm_framebuffer), params.color_space, params.plane_z_order,
      absl::get<gfx::OverlayTransform>(params.transform), gfx::Rect(),
      gfx::ToNearestRect(params.display_rect), params.crop_rect, true, nullptr);
}

void DrmOverlayValidatorTest::TearDown() {
  std::unique_ptr<DrmWindow> window =
      screen_manager_->RemoveWindow(kDefaultWidgetHandle);
  window->Shutdown();
  // Destroy the DrmWindow before destroying the ScreenManager.
  window = nullptr;

  // Need to ensure ScreenManager is destructed before PlaneManager.
  screen_manager_ = nullptr;
  drm_->ResetPlaneManagerForTesting();
}

TEST_F(DrmOverlayValidatorTest, WindowWithNoController) {
  CrtcState crtc_state = {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}};
  InitDrmStatesAndControllers({crtc_state});

  // We should never promote layers to overlay when controller is not
  // present.
  HardwareDisplayController* controller = window_->GetController();
  window_->SetController(nullptr);
  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(returns.front(), OVERLAY_STATUS_NOT);
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_NOT);
  window_->SetController(controller);
}

TEST_F(DrmOverlayValidatorTest, DontPromoteMoreLayersThanAvailablePlanes) {
  CrtcState crtc_state = {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}};
  InitDrmStatesAndControllers({crtc_state});

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(returns.front(), OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_NOT);
}

TEST_F(DrmOverlayValidatorTest, DontCollapseOverlayToPrimaryInFullScreen) {
  CrtcState crtc_state = {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}};
  InitDrmStatesAndControllers({crtc_state});

  // Overlay Validator should not collapse planes during validation.
  overlay_params_.back().buffer_size = primary_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(primary_rect_);
  plane_list_.back().display_bounds = primary_rect_;

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  // Second candidate should be marked as Invalid as we have only one plane
  // per CRTC.
  EXPECT_EQ(returns.front(), OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_NOT);
}

TEST_F(DrmOverlayValidatorTest, OverlayFormat_XRGB) {
  // This test checks for optimal format in case of non full screen video case.
  // This should be XRGB when overlay doesn't support YUV.
  CrtcState state = {
      .planes = {{.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}},
                 {.formats = {DRM_FORMAT_XRGB8888}}}};
  InitDrmStatesAndControllers(std::vector<CrtcState>(1, state));

  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(overlay_rect_);
  plane_list_.back().display_bounds = overlay_rect_;

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  for (const auto& param : returns)
    EXPECT_EQ(param, OVERLAY_STATUS_ABLE);
}

TEST_F(DrmOverlayValidatorTest, OverlayFormat_YUV) {
  // This test checks for optimal format in case of non full screen video case.
  // Prefer YUV as optimal format when Overlay supports it and scaling is
  // needed.
  CrtcState state = {
      .planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                 {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}};
  InitDrmStatesAndControllers(std::vector<CrtcState>(1, state));

  gfx::RectF crop_rect = gfx::RectF(0, 0, 0.5, 0.5);
  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(overlay_rect_);
  overlay_params_.back().crop_rect = crop_rect;
  overlay_params_.back().is_opaque = false;
  overlay_params_.back().format = gfx::BufferFormat::YUV_420_BIPLANAR;
  plane_list_.pop_back();
  AddPlane(overlay_params_.back());

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  for (const auto& param : returns)
    EXPECT_EQ(param, OVERLAY_STATUS_ABLE);
}

TEST_F(DrmOverlayValidatorTest, RejectYUVBuffersIfNotSupported) {
  // Check case where buffer storage format is already YUV 420 but planes don't
  // support it.
  CrtcState state = {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                                {.formats = {DRM_FORMAT_XRGB8888}}}};
  InitDrmStatesAndControllers(std::vector<CrtcState>(1, state));

  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(overlay_rect_);
  overlay_params_.back().format = gfx::BufferFormat::YUV_420_BIPLANAR;
  plane_list_.pop_back();
  AddPlane(overlay_params_.back());

  std::vector<OverlaySurfaceCandidate> validated_params = overlay_params_;
  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(validated_params, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_NOT);
}

TEST_F(DrmOverlayValidatorTest,
       RejectYUVBuffersIfNotSupported_MirroredControllers) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}}};
  InitDrmStatesAndControllers(crtc_states);

  HardwareDisplayController* controller = window_->GetController();
  controller->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), drm_->crtc_property(1).id, drm_->connector_property(1).id));
  EXPECT_TRUE(ModesetController(controller));

  gfx::RectF crop_rect = gfx::RectF(0, 0, 0.5, 0.5);
  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(overlay_rect_);
  overlay_params_.back().crop_rect = crop_rect;
  plane_list_.back().display_bounds = overlay_rect_;
  plane_list_.back().crop_rect = crop_rect;

  std::vector<OverlaySurfaceCandidate> validated_params = overlay_params_;
  validated_params.back().format = gfx::BufferFormat::YUV_420_BIPLANAR;
  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(validated_params, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_ABLE);
}

TEST_F(DrmOverlayValidatorTest,
       RejectYUVBuffersIfNotSupported_NoPackedFormatsInMirroredCrtc) {
  // This configuration should not be promoted to Overlay when either of the
  // controllers don't support YUV 420 format.

  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888}}}},
  };
  InitDrmStatesAndControllers(crtc_states);

  HardwareDisplayController* controller = window_->GetController();
  controller->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), drm_->crtc_property(1).id, drm_->connector_property(1).id));
  EXPECT_TRUE(ModesetController(controller));

  gfx::RectF crop_rect = gfx::RectF(0, 0, 0.5, 0.5);
  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(overlay_rect_);
  overlay_params_.back().crop_rect = crop_rect;
  plane_list_.back().display_bounds = overlay_rect_;
  plane_list_.back().crop_rect = crop_rect;

  std::vector<OverlaySurfaceCandidate> validated_params = overlay_params_;
  validated_params.back().format = gfx::BufferFormat::YUV_420_BIPLANAR;
  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(validated_params, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_NOT);
}

TEST_F(DrmOverlayValidatorTest,
       RejectYUVBuffersIfNotSupported_NoPackedFormatsInPrimaryDisplay) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}}};
  InitDrmStatesAndControllers(crtc_states);

  HardwareDisplayController* controller = window_->GetController();
  controller->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), drm_->crtc_property(1).id, drm_->connector_property(1).id));
  EXPECT_TRUE(ModesetController(controller));

  gfx::RectF crop_rect = gfx::RectF(0, 0, 0.5, 0.5);
  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(overlay_rect_);
  overlay_params_.back().crop_rect = crop_rect;
  plane_list_.back().display_bounds = overlay_rect_;
  plane_list_.back().crop_rect = crop_rect;

  std::vector<OverlaySurfaceCandidate> validated_params = overlay_params_;
  validated_params.back().format = gfx::BufferFormat::YUV_420_BIPLANAR;

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(validated_params, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_NOT);
}

TEST_F(DrmOverlayValidatorTest, OptimalFormatXRGB_MirroredControllers) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}},
  };
  InitDrmStatesAndControllers(crtc_states);

  HardwareDisplayController* controller = window_->GetController();
  controller->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), drm_->crtc_property(1).id, drm_->connector_property(1).id));
  EXPECT_TRUE(ModesetController(controller));

  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(overlay_rect_);
  plane_list_.back().display_bounds = overlay_rect_;

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_ABLE);
}

TEST_F(DrmOverlayValidatorTest,
       OptimalFormatXRGB_NoPackedFormatInMirroredCrtc) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888}}}},
  };
  InitDrmStatesAndControllers(crtc_states);

  HardwareDisplayController* controller = window_->GetController();
  controller->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), drm_->crtc_property(1).id, drm_->connector_property(1).id));
  EXPECT_TRUE(ModesetController(controller));

  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(overlay_rect_);
  plane_list_.back().display_bounds = overlay_rect_;

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_ABLE);
}

TEST_F(DrmOverlayValidatorTest,
       OptimalFormatXRGB_NoPackedFormatInPrimaryDisplay) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                  {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}}};
  InitDrmStatesAndControllers(crtc_states);

  HardwareDisplayController* controller = window_->GetController();
  controller->AddCrtc(std::make_unique<CrtcController>(
      drm_.get(), drm_->crtc_property(1).id, drm_->connector_property(1).id));
  EXPECT_TRUE(ModesetController(controller));

  overlay_params_.back().buffer_size = overlay_rect_.size();
  overlay_params_.back().display_rect = gfx::RectF(overlay_rect_);
  plane_list_.back().display_bounds = overlay_rect_;

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.back(), OVERLAY_STATUS_ABLE);
}

TEST_F(DrmOverlayValidatorTest, RejectBufferAllocationFail) {
  CrtcState crtc_state = {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}};
  InitDrmStatesAndControllers({crtc_state});

  // Buffer allocation for scanout might fail.
  // In that case we should reject the overlay candidate.
  gbm_->set_allocation_failure(true);

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  EXPECT_EQ(returns.front(), OVERLAY_STATUS_NOT);
}

// This test verifies that the Ozone/DRM implementation does not reject overlay
// candidates purely on the basis of having non-integer bounds. Instead, they
// should be rounded to the nearest integer.
TEST_F(DrmOverlayValidatorTest, NonIntegerDisplayRect) {
  CrtcState state = {
      .planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                 {.formats = {DRM_FORMAT_XRGB8888, DRM_FORMAT_NV12}}}};
  InitDrmStatesAndControllers(std::vector<CrtcState>(1, state));

  overlay_params_.back().display_rect.Inset(0.005f);
  plane_list_.pop_back();
  AddPlane(overlay_params_.back());

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(2u, returns.size());
  for (const auto& param : returns)
    EXPECT_EQ(param, OVERLAY_STATUS_ABLE);
}

class TestAtOnceDrmOverlayValidatorTest
    : public DrmOverlayValidatorTest,
      public testing::WithParamInterface<bool> {};

TEST_F(DrmOverlayValidatorTest, FourCandidates_OneCommit) {
  // Four planes.
  CrtcState crtc_state = {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                                     {.formats = {DRM_FORMAT_XRGB8888}},
                                     {.formats = {DRM_FORMAT_XRGB8888}},
                                     {.formats = {DRM_FORMAT_XRGB8888}}}};
  InitDrmStatesAndControllers({crtc_state});
  int setup_commits = drm_->get_commit_count();

  // Add two more overlay candidates.
  auto param3 = overlay_params_.back();
  auto param4 = overlay_params_.back();
  overlay_params_.push_back(param3);
  overlay_params_.push_back(param4);

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());

  // All planes promoted.
  ASSERT_EQ(4u, returns.size());
  EXPECT_EQ(returns[0], OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns[1], OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns[2], OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns[3], OVERLAY_STATUS_ABLE);
  // Only 1 commit was necessary.
  EXPECT_EQ(drm_->get_commit_count() - setup_commits, 1);
}

TEST_F(DrmOverlayValidatorTest, FourCandidatesTwoPlanes_OneCommit) {
  // Only two planes.
  CrtcState crtc_state = {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                                     {.formats = {DRM_FORMAT_XRGB8888}}}};
  InitDrmStatesAndControllers({crtc_state});
  int setup_commits = drm_->get_commit_count();

  // Add two more overlay candidates.
  auto param3 = overlay_params_.back();
  auto param4 = overlay_params_.back();
  overlay_params_.push_back(param3);
  overlay_params_.push_back(param4);

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());

  // Two planes promoted.
  ASSERT_EQ(4u, returns.size());
  EXPECT_EQ(returns[0], OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns[1], OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns[2], OVERLAY_STATUS_NOT);
  EXPECT_EQ(returns[3], OVERLAY_STATUS_NOT);
  // We should only see one commit because we won't talk to DRM if we can't
  // allocate planes.
  EXPECT_EQ(drm_->get_commit_count() - setup_commits, 1);
}

TEST_F(DrmOverlayValidatorTest, TwoOfSixIgnored_OneCommit) {
  // Six planes.
  CrtcState crtc_state = {.planes = {{.formats = {DRM_FORMAT_XRGB8888}},
                                     {.formats = {DRM_FORMAT_XRGB8888}},
                                     {.formats = {DRM_FORMAT_XRGB8888}},
                                     {.formats = {DRM_FORMAT_XRGB8888}},
                                     {.formats = {DRM_FORMAT_XRGB8888}},
                                     {.formats = {DRM_FORMAT_XRGB8888}}}};
  InitDrmStatesAndControllers({crtc_state});
  int setup_commits = drm_->get_commit_count();

  auto param3 = overlay_params_.back();
  auto param4 = overlay_params_.back();
  auto param5 = overlay_params_.back();
  auto param6 = overlay_params_.back();
  // Candidate 3 and 5 are already disqualified.
  param3.overlay_handled = false;
  param5.overlay_handled = false;
  overlay_params_.push_back(param3);
  overlay_params_.push_back(param4);
  overlay_params_.push_back(param5);
  overlay_params_.push_back(param6);

  std::vector<OverlayStatus> returns =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());

  ASSERT_EQ(6u, returns.size());
  // Third and Fifth candidate were ignored.
  EXPECT_EQ(returns[0], OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns[1], OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns[2], OVERLAY_STATUS_NOT);
  EXPECT_EQ(returns[3], OVERLAY_STATUS_ABLE);
  EXPECT_EQ(returns[4], OVERLAY_STATUS_NOT);
  EXPECT_EQ(returns[5], OVERLAY_STATUS_ABLE);
  // Only 1 commit was needed because the two unpromoted candidates were
  // excluded before testing.
  EXPECT_EQ(drm_->get_commit_count() - setup_commits, 1);
}

TEST_F(DrmOverlayValidatorTest, PinnedPlanesCantBeReused) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}}};
  std::vector<PlaneState> movable_planes = {{.formats = {DRM_FORMAT_XRGB8888}}};
  InitDrmStatesAndControllers(crtc_states, movable_planes);

  auto* movable_plane = GetMovablePlanes()[0];
  movable_plane->set_in_use(true);
  movable_plane->set_owning_crtc(drm_->crtc_property(1).id);

  auto results =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(results[0], OVERLAY_STATUS_ABLE)
      << "The primary plane should still be usable.";
  EXPECT_EQ(results[1], OVERLAY_STATUS_NOT)
      << "The overlay plane should not be available.";
}

TEST_F(DrmOverlayValidatorTest, UnpinnedMovablePlanesCanBeUsed) {
  std::vector<CrtcState> crtc_states = {
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}},
      {.planes = {{.formats = {DRM_FORMAT_XRGB8888}}}}};
  std::vector<PlaneState> movable_planes = {{.formats = {DRM_FORMAT_XRGB8888}}};
  InitDrmStatesAndControllers(crtc_states, movable_planes);

  auto* movable_plane = GetMovablePlanes()[0];
  ASSERT_EQ(0u, movable_plane->owning_crtc());
  ASSERT_FALSE(movable_plane->in_use());

  auto results =
      overlay_validator_->TestPageFlip(overlay_params_, DrmOverlayPlaneList());
  EXPECT_EQ(results[0], OVERLAY_STATUS_ABLE)
      << "The primary plane should still be usable.";
  EXPECT_EQ(results[1], OVERLAY_STATUS_ABLE)
      << "The overlay plane should be available since it is not pinned.";
}

}  // namespace ui
