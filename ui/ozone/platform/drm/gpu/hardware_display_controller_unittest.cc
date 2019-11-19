// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drm_fourcc.h>
#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/common/linux/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_gbm_device.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

constexpr uint32_t kCrtcIdBase = 1;
constexpr uint32_t kPrimaryCrtc = kCrtcIdBase;
constexpr uint32_t kSecondaryCrtc = kCrtcIdBase + 1;
constexpr uint32_t kPrimaryConnector = 10;
constexpr uint32_t kSecondaryConnector = 11;
constexpr uint32_t kPlaneOffset = 1000;

const gfx::Size kDefaultModeSize(kDefaultMode.hdisplay, kDefaultMode.vdisplay);
const gfx::Size kOverlaySize(kDefaultMode.hdisplay / 2,
                             kDefaultMode.vdisplay / 2);
const gfx::SizeF kDefaultModeSizeF(1.0, 1.0);

}  // namespace

class HardwareDisplayControllerTest : public testing::Test {
 public:
  HardwareDisplayControllerTest() : page_flips_(0) {}
  ~HardwareDisplayControllerTest() override = default;

  void SetUp() override;
  void TearDown() override;

  void InitializeDrmDevice(bool use_atomic);
  void SchedulePageFlip(ui::DrmOverlayPlaneList planes);
  void OnSubmission(gfx::SwapResult swap_result,
                    std::unique_ptr<gfx::GpuFence> out_fence);
  void OnPresentation(const gfx::PresentationFeedback& feedback);
  uint64_t GetPlanePropertyValue(uint32_t plane,
                                 const std::string& property_name);

  scoped_refptr<ui::DrmFramebuffer> CreateBuffer() {
    std::unique_ptr<ui::GbmBuffer> buffer = drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, kDefaultModeSize, GBM_BO_USE_SCANOUT);
    return ui::DrmFramebuffer::AddFramebuffer(drm_, buffer.get());
  }

  scoped_refptr<ui::DrmFramebuffer> CreateOverlayBuffer() {
    std::unique_ptr<ui::GbmBuffer> buffer = drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, kOverlaySize, GBM_BO_USE_SCANOUT);
    return ui::DrmFramebuffer::AddFramebuffer(drm_, buffer.get());
  }

 protected:
  std::unique_ptr<ui::HardwareDisplayController> controller_;
  scoped_refptr<ui::MockDrmDevice> drm_;

  int page_flips_;
  gfx::SwapResult last_swap_result_;
  gfx::PresentationFeedback last_presentation_feedback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerTest);
};

void HardwareDisplayControllerTest::SetUp() {
  page_flips_ = 0;
  last_swap_result_ = gfx::SwapResult::SWAP_FAILED;

  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  drm_ = new ui::MockDrmDevice(std::move(gbm_device));
  InitializeDrmDevice(/* use_atomic= */ true);

  controller_ = std::make_unique<ui::HardwareDisplayController>(
      std::make_unique<ui::CrtcController>(drm_.get(), kPrimaryCrtc,
                                           kPrimaryConnector),
      gfx::Point());
}

void HardwareDisplayControllerTest::TearDown() {
  controller_.reset();
  drm_ = nullptr;
}

void HardwareDisplayControllerTest::InitializeDrmDevice(bool use_atomic) {
  constexpr uint32_t kTypePropId = 300;
  constexpr uint32_t kInFormatsPropId = 301;
  constexpr uint32_t kInFormatsBlobPropId = 400;

  std::vector<ui::MockDrmDevice::CrtcProperties> crtc_properties(2);
  std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties;
  std::map<uint32_t, std::string> property_names = {
      // Add all required properties.
      {200, "CRTC_ID"},
      {201, "CRTC_X"},
      {202, "CRTC_Y"},
      {203, "CRTC_W"},
      {204, "CRTC_H"},
      {205, "FB_ID"},
      {206, "SRC_X"},
      {207, "SRC_Y"},
      {208, "SRC_W"},
      {209, "SRC_H"},
      // Add some optional properties we use for convenience.
      {kTypePropId, "type"},
      {kInFormatsPropId, "IN_FORMATS"},
  };

  for (size_t i = 0; i < crtc_properties.size(); ++i) {
    crtc_properties[i].id = kCrtcIdBase + i;

    for (size_t j = 0; j < 2; ++j) {
      const uint32_t offset = plane_properties.size();

      ui::MockDrmDevice::PlaneProperties plane;
      plane.id = kPlaneOffset + offset;
      plane.crtc_mask = 1 << i;
      for (const auto& pair : property_names) {
        uint32_t value = 0;
        if (pair.first == kTypePropId)
          value = j == 0 ? DRM_PLANE_TYPE_PRIMARY : DRM_PLANE_TYPE_OVERLAY;
        else if (pair.first == kInFormatsPropId)
          value = kInFormatsBlobPropId;

        plane.properties.push_back(
            {/* .id = */ pair.first, /*.value = */ value});
      };

      drm_format_modifier y_css = {.formats = 1UL,
                                   .modifier = I915_FORMAT_MOD_Y_TILED_CCS};
      drm_format_modifier yf_css = {.formats = 1UL,
                                    .modifier = I915_FORMAT_MOD_Yf_TILED_CCS};
      drm_format_modifier x = {.formats = 1UL,
                               .modifier = I915_FORMAT_MOD_X_TILED};
      drm_format_modifier linear = {.formats = 1UL,
                                    .modifier = DRM_FORMAT_MOD_LINEAR};
      drm_->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
          kInFormatsBlobPropId, {DRM_FORMAT_XRGB8888},
          {y_css, yf_css, x, linear}));

      plane_properties.emplace_back(std::move(plane));
    }
  }

  drm_->InitializeState(crtc_properties, plane_properties, property_names,
                        use_atomic);
}

void HardwareDisplayControllerTest::SchedulePageFlip(
    ui::DrmOverlayPlaneList planes) {
  controller_->SchedulePageFlip(
      std::move(planes),
      base::BindOnce(&HardwareDisplayControllerTest::OnSubmission,
                     base::Unretained(this)),
      base::BindOnce(&HardwareDisplayControllerTest::OnPresentation,
                     base::Unretained(this)));
}

void HardwareDisplayControllerTest::OnSubmission(
    gfx::SwapResult result,
    std::unique_ptr<gfx::GpuFence> out_fence) {
  last_swap_result_ = result;
}

void HardwareDisplayControllerTest::OnPresentation(
    const gfx::PresentationFeedback& feedback) {
  page_flips_++;
  last_presentation_feedback_ = feedback;
}

uint64_t HardwareDisplayControllerTest::GetPlanePropertyValue(
    uint32_t plane,
    const std::string& property_name) {
  ui::DrmDevice::Property p{};
  ui::ScopedDrmObjectPropertyPtr properties(
      drm_->GetObjectProperties(plane, DRM_MODE_OBJECT_PLANE));
  EXPECT_TRUE(ui::GetDrmPropertyForName(drm_.get(), properties.get(),
                                        property_name, &p));
  return p.value;
}

TEST_F(HardwareDisplayControllerTest, CheckModesettingResult) {
  ui::DrmOverlayPlane plane(CreateBuffer(), nullptr);

  EXPECT_TRUE(controller_->Modeset(plane, kDefaultMode));
  EXPECT_FALSE(plane.buffer->HasOneRef());
}

TEST_F(HardwareDisplayControllerTest, ModifiersWithConnectorType) {
  ui::DrmOverlayPlane plane(CreateBuffer(), nullptr);

  // With internal displays, all modifiers including compressed (css) should be
  // there.
  drm_->set_connector_type(DRM_MODE_CONNECTOR_eDP);

  std::vector<uint64_t> internal_modifiers =
      controller_->GetFormatModifiers(DRM_FORMAT_XRGB8888);
  ASSERT_FALSE(internal_modifiers.empty());

  EXPECT_NE(std::find(internal_modifiers.begin(), internal_modifiers.end(),
                      I915_FORMAT_MOD_Y_TILED_CCS),
            internal_modifiers.end());
  EXPECT_NE(std::find(internal_modifiers.begin(), internal_modifiers.end(),
                      I915_FORMAT_MOD_Yf_TILED_CCS),
            internal_modifiers.end());
  EXPECT_NE(std::find(internal_modifiers.begin(), internal_modifiers.end(),
                      I915_FORMAT_MOD_X_TILED),
            internal_modifiers.end());
  EXPECT_NE(std::find(internal_modifiers.begin(), internal_modifiers.end(),
                      DRM_FORMAT_MOD_LINEAR),
            internal_modifiers.end());

  // With external displays, *_CSS modifiers (2 of them) should not exist.
  drm_->set_connector_type(DRM_MODE_CONNECTOR_DisplayPort);

  std::vector<uint64_t> external_modifiers =
      controller_->GetFormatModifiers(DRM_FORMAT_XRGB8888);
  ASSERT_FALSE(external_modifiers.empty());
  EXPECT_EQ(external_modifiers.size(), internal_modifiers.size() - 2);

  EXPECT_EQ(std::find(external_modifiers.begin(), external_modifiers.end(),
                      I915_FORMAT_MOD_Y_TILED_CCS),
            external_modifiers.end());
  EXPECT_EQ(std::find(external_modifiers.begin(), external_modifiers.end(),
                      I915_FORMAT_MOD_Yf_TILED_CCS),
            external_modifiers.end());
  EXPECT_NE(std::find(internal_modifiers.begin(), internal_modifiers.end(),
                      I915_FORMAT_MOD_X_TILED),
            internal_modifiers.end());
  EXPECT_NE(std::find(internal_modifiers.begin(), internal_modifiers.end(),
                      DRM_FORMAT_MOD_LINEAR),
            internal_modifiers.end());
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterPageFlip) {
  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  ui::DrmOverlayPlane plane2(CreateBuffer(), nullptr);
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane2.Clone());

  SchedulePageFlip(std::move(planes));

  drm_->RunCallbacks();
  EXPECT_TRUE(plane1.buffer->HasOneRef());
  EXPECT_FALSE(plane2.buffer->HasOneRef());

  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
  EXPECT_EQ(1, drm_->get_commit_count());
  // Verify only the primary display have a valid framebuffer.
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_EQ(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfModesetFails) {
  drm_->set_set_crtc_expectation(false);

  ui::DrmOverlayPlane plane(CreateBuffer(), nullptr);

  EXPECT_FALSE(controller_->Modeset(plane, kDefaultMode));
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfPageFlipFails) {
  drm_->set_commit_expectation(false);

  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  ui::DrmOverlayPlane plane2(CreateBuffer(), nullptr);
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane2.Clone());
  EXPECT_DEATH_IF_SUPPORTED(SchedulePageFlip(std::move(planes)),
                            "SchedulePageFlip failed");
}

TEST_F(HardwareDisplayControllerTest, CheckOverlayPresent) {
  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  ui::DrmOverlayPlane plane2(
      CreateOverlayBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
      gfx::Rect(kOverlaySize), gfx::RectF(kDefaultModeSizeF), true, nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  planes.push_back(plane2.Clone());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
  EXPECT_EQ(1, drm_->get_commit_count());
  // Verify both planes on the primary display have a valid framebuffer.
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_F(HardwareDisplayControllerTest, CheckOverlayTestMode) {
  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  ui::DrmOverlayPlane plane2(
      CreateOverlayBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
      gfx::Rect(kOverlaySize), gfx::RectF(kDefaultModeSizeF), true, nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  planes.push_back(plane2.Clone());

  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  EXPECT_EQ(1, drm_->get_commit_count());
  // Verify both planes on the primary display have a valid framebuffer.
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));

  // A test call shouldn't cause new flips, but should succeed.
  EXPECT_TRUE(controller_->TestPageFlip(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
  EXPECT_EQ(2, drm_->get_commit_count());

  // Regular flips should continue on normally.
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, page_flips_);
  EXPECT_EQ(3, drm_->get_commit_count());
  // Verify both planes on the primary display have a valid framebuffer.
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_F(HardwareDisplayControllerTest, AcceptUnderlays) {
  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  ui::DrmOverlayPlane plane2(CreateBuffer(), -1, gfx::OVERLAY_TRANSFORM_NONE,
                             gfx::Rect(kDefaultModeSize),
                             gfx::RectF(kDefaultModeSizeF), true, nullptr);

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  planes.push_back(plane2.Clone());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, PageflipMirroredControllers) {
  controller_->AddCrtc(std::unique_ptr<ui::CrtcController>(
      new ui::CrtcController(drm_.get(), kSecondaryCrtc, kSecondaryConnector)));

  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  EXPECT_EQ(2, drm_->get_set_crtc_call_count());

  ui::DrmOverlayPlane plane2(CreateBuffer(), nullptr);
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane2.Clone());
  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
  EXPECT_EQ(1, drm_->get_commit_count());
  // Verify only the displays have a valid framebuffer on the primary plane.
  // First display:
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_EQ(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
  // Second display:
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 2, "FB_ID"));
  EXPECT_EQ(0u, GetPlanePropertyValue(kPlaneOffset + 3, "FB_ID"));
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterRemoveCrtc) {
  controller_->AddCrtc(std::unique_ptr<ui::CrtcController>(
      new ui::CrtcController(drm_.get(), kSecondaryCrtc, kSecondaryConnector)));

  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);

  const ui::HardwareDisplayPlane* primary_crtc_plane = nullptr;
  const ui::HardwareDisplayPlane* secondary_crtc_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use() && plane->owning_crtc() == kPrimaryCrtc)
      primary_crtc_plane = plane.get();
    if (plane->in_use() && plane->owning_crtc() == kSecondaryCrtc)
      secondary_crtc_plane = plane.get();
  }

  ASSERT_NE(nullptr, primary_crtc_plane);
  ASSERT_NE(nullptr, secondary_crtc_plane);
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
  EXPECT_EQ(kSecondaryCrtc, secondary_crtc_plane->owning_crtc());

  // Removing the crtc should not free the plane or change ownership.
  std::unique_ptr<ui::CrtcController> crtc =
      controller_->RemoveCrtc(drm_, kPrimaryCrtc);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
  EXPECT_TRUE(secondary_crtc_plane->in_use());
  EXPECT_EQ(kSecondaryCrtc, secondary_crtc_plane->owning_crtc());

  // Check that controller doesn't affect the state of removed plane in
  // subsequent page flip.
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, page_flips_);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
  EXPECT_TRUE(secondary_crtc_plane->in_use());
  EXPECT_EQ(kSecondaryCrtc, secondary_crtc_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterDestroyingCrtc) {
  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);

  const ui::HardwareDisplayPlane* owned_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes())
    if (plane->in_use())
      owned_plane = plane.get();
  ASSERT_TRUE(owned_plane != nullptr);
  EXPECT_EQ(kPrimaryCrtc, owned_plane->owning_crtc());
  std::unique_ptr<ui::CrtcController> crtc =
      controller_->RemoveCrtc(drm_, kPrimaryCrtc);
  // Destroying crtc should free the plane.
  crtc.reset();
  uint32_t crtc_nullid = 0;
  EXPECT_FALSE(owned_plane->in_use());
  EXPECT_EQ(crtc_nullid, owned_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, PlaneStateAfterAddCrtc) {
  controller_->AddCrtc(std::unique_ptr<ui::CrtcController>(
      new ui::CrtcController(drm_.get(), kSecondaryCrtc, kSecondaryConnector)));

  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);

  ui::HardwareDisplayPlane* primary_crtc_plane = nullptr;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use() && kPrimaryCrtc == plane->owning_crtc())
      primary_crtc_plane = plane.get();
  }

  ASSERT_TRUE(primary_crtc_plane != nullptr);

  std::unique_ptr<ui::HardwareDisplayController> hdc_controller;
  hdc_controller = std::make_unique<ui::HardwareDisplayController>(
      controller_->RemoveCrtc(drm_, kPrimaryCrtc), controller_->origin());
  SchedulePageFlip(ui::DrmOverlayPlane::Clone(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(2, page_flips_);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());

  // We reset state of plane here to test that the plane was actually added to
  // hdc_controller. In which case, the right state should be set to plane
  // after page flip call is handled by the controller.
  primary_crtc_plane->set_in_use(false);
  primary_crtc_plane->set_owning_crtc(0);
  hdc_controller->SchedulePageFlip(
      ui::DrmOverlayPlane::Clone(planes),
      base::BindOnce(&HardwareDisplayControllerTest::OnSubmission,
                     base::Unretained(this)),
      base::BindOnce(&HardwareDisplayControllerTest::OnPresentation,
                     base::Unretained(this)));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(3, page_flips_);
  EXPECT_TRUE(primary_crtc_plane->in_use());
  EXPECT_EQ(kPrimaryCrtc, primary_crtc_plane->owning_crtc());
}

TEST_F(HardwareDisplayControllerTest, ModesetWhilePageFlipping) {
  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));

  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, FailPageFlipping) {
  drm_->set_commit_expectation(false);

  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  EXPECT_DEATH_IF_SUPPORTED(SchedulePageFlip(std::move(planes)),
                            "SchedulePageFlip failed");
}

TEST_F(HardwareDisplayControllerTest, CheckNoPrimaryPlane) {
  ui::DrmOverlayPlane plane1(CreateBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
                             gfx::Rect(kDefaultModeSize),
                             gfx::RectF(0, 0, 1, 1), true, nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, AddCrtcMidPageFlip) {
  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));

  controller_->AddCrtc(std::unique_ptr<ui::CrtcController>(
      new ui::CrtcController(drm_.get(), kSecondaryCrtc, kSecondaryConnector)));

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, RemoveCrtcMidPageFlip) {
  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  SchedulePageFlip(std::move(planes));

  controller_->RemoveCrtc(drm_, kPrimaryCrtc);

  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);
  EXPECT_EQ(1, page_flips_);
}

TEST_F(HardwareDisplayControllerTest, Disable) {
  // Page flipping overlays is only supported on atomic configurations.
  InitializeDrmDevice(/* use_atomic= */ true);

  ui::DrmOverlayPlane plane1(CreateBuffer(), nullptr);
  EXPECT_TRUE(controller_->Modeset(plane1, kDefaultMode));

  ui::DrmOverlayPlane plane2(
      CreateOverlayBuffer(), 1, gfx::OVERLAY_TRANSFORM_NONE,
      gfx::Rect(kOverlaySize), gfx::RectF(kDefaultModeSizeF), true, nullptr);
  std::vector<ui::DrmOverlayPlane> planes;
  planes.push_back(plane1.Clone());
  planes.push_back(plane2.Clone());

  SchedulePageFlip(std::move(planes));
  drm_->RunCallbacks();
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, last_swap_result_);

  controller_->Disable();

  int planes_in_use = 0;
  for (const auto& plane : drm_->plane_manager()->planes()) {
    if (plane->in_use())
      planes_in_use++;
  }
  // Only the primary plane is in use.
  ASSERT_EQ(1, planes_in_use);
}
