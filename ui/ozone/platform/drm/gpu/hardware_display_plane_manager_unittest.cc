// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drm_fourcc.h>
#include <stdint.h>
#include <unistd.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/common/linux/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/mock_gbm_device.h"

namespace {

constexpr uint32_t kPlaneOffset = 100;
constexpr uint32_t kTypePropId = 300;
constexpr uint32_t kInFormatsPropId = 301;
constexpr uint32_t kPlaneCtmId = 302;
constexpr uint32_t kCtmPropId = 303;
constexpr uint32_t kGammaLutPropId = 304;
constexpr uint32_t kGammaLutSizePropId = 305;
constexpr uint32_t kDegammaLutPropId = 306;
constexpr uint32_t kDegammaLutSizePropId = 307;
constexpr uint32_t kOutFencePtrPropId = 308;
constexpr uint32_t kInFormatsBlobPropId = 400;
constexpr uint32_t kBackgroundColorPropId = 401;

const gfx::Size kDefaultBufferSize(2, 2);

class HardwareDisplayPlaneManagerTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  HardwareDisplayPlaneManagerTest() {}

  void InitializeDrmState(size_t crtc_count, size_t planes_per_crtc);

  uint64_t GetObjectPropertyValue(uint32_t object_id,
                                  uint32_t object_type,
                                  const std::string& property_name);
  uint64_t GetCrtcPropertyValue(uint32_t crtc,
                                const std::string& property_name);
  uint64_t GetPlanePropertyValue(uint32_t plane,
                                 const std::string& property_name);

  void PerformPageFlip(size_t crtc_idx, ui::HardwareDisplayPlaneList* state);

  void SetUp() override;

  scoped_refptr<ui::DrmFramebuffer> CreateBuffer(const gfx::Size& size) {
    return CreateBufferWithFormat(size, DRM_FORMAT_XRGB8888);
  }

  scoped_refptr<ui::DrmFramebuffer> CreateBufferWithFormat(
      const gfx::Size& size,
      uint32_t format) {
    std::unique_ptr<ui::GbmBuffer> buffer =
        fake_drm_->gbm_device()->CreateBuffer(format, size, GBM_BO_USE_SCANOUT);
    return ui::DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get());
  }

 protected:
  ui::HardwareDisplayPlaneList state_;
  scoped_refptr<ui::DrmFramebuffer> fake_buffer_;
  scoped_refptr<ui::MockDrmDevice> fake_drm_;

  std::vector<ui::MockDrmDevice::CrtcProperties> crtc_properties_;
  std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties_;
  std::map<uint32_t, std::string> property_names_;

  bool use_atomic_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneManagerTest);
};

void HardwareDisplayPlaneManagerTest::SetUp() {
  use_atomic_ = GetParam();

  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  fake_drm_ = new ui::MockDrmDevice(std::move(gbm_device));
  fake_drm_->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
      kInFormatsBlobPropId, {DRM_FORMAT_XRGB8888}, {}));

  fake_buffer_ = CreateBuffer(kDefaultBufferSize);
}

void HardwareDisplayPlaneManagerTest::InitializeDrmState(
    size_t crtc_count,
    size_t planes_per_crtc) {
  property_names_ = {
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
      // Defines some optional properties we use for convenience.
      {kTypePropId, "type"},
      {kInFormatsPropId, "IN_FORMATS"},
  };
  // Always add an additional cursor plane.
  ++planes_per_crtc;
  for (size_t i = 0; i < crtc_count; ++i) {
    ui::MockDrmDevice::CrtcProperties crtc_prop;
    // Start ID at 1 cause 0 is an invalid ID.
    crtc_prop.id = i + 1;
    crtc_properties_.emplace_back(std::move(crtc_prop));

    for (size_t j = 0; j < planes_per_crtc; ++j) {
      ui::MockDrmDevice::PlaneProperties plane_prop;
      plane_prop.id = kPlaneOffset + i * planes_per_crtc + j;
      plane_prop.crtc_mask = 1 << i;
      for (const auto& pair : property_names_) {
        uint32_t value = 0;
        if (pair.first == kTypePropId) {
          if (j == 0)
            value = DRM_PLANE_TYPE_PRIMARY;
          else if (j == planes_per_crtc - 1)
            value = DRM_PLANE_TYPE_CURSOR;
          else
            value = DRM_PLANE_TYPE_OVERLAY;
        } else if (pair.first == kInFormatsPropId) {
          value = kInFormatsBlobPropId;
        }
        plane_prop.properties.push_back(
            {/* .id = */ pair.first, /* .value = */ value});
      };

      plane_properties_.emplace_back(std::move(plane_prop));
    }
  }

  // Separately add optional properties that will be used in some tests, but the
  // tests will append the property to the planes on a case-by-case basis.
  //
  // Plane properties:
  property_names_.insert({kPlaneCtmId, "PLANE_CTM"});
  // CRTC properties:
  property_names_.insert({kCtmPropId, "CTM"});
  property_names_.insert({kGammaLutPropId, "GAMMA_LUT"});
  property_names_.insert({kGammaLutSizePropId, "GAMMA_LUT_SIZE"});
  property_names_.insert({kDegammaLutPropId, "DEGAMMA_LUT"});
  property_names_.insert({kDegammaLutSizePropId, "DEGAMMA_LUT_SIZE"});
  property_names_.insert({kOutFencePtrPropId, "OUT_FENCE_PTR"});
  property_names_.insert({kBackgroundColorPropId, "BACKGROUND_COLOR"});
}

void HardwareDisplayPlaneManagerTest::PerformPageFlip(
    size_t crtc_idx,
    ui::HardwareDisplayPlaneList* state) {
  ui::DrmOverlayPlaneList assigns;
  ui::CrtcController crtc(fake_drm_, crtc_properties_[crtc_idx].id, 0);
  scoped_refptr<ui::DrmFramebuffer> xrgb_buffer =
      CreateBuffer(kDefaultBufferSize);
  assigns.push_back(ui::DrmOverlayPlane(xrgb_buffer, nullptr));
  fake_drm_->plane_manager()->BeginFrame(state);
  ASSERT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      state, assigns, crtc_properties_[crtc_idx].id, &crtc));
  scoped_refptr<ui::PageFlipRequest> page_flip_request =
      base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
  ASSERT_TRUE(
      fake_drm_->plane_manager()->Commit(state, page_flip_request, nullptr));
}

uint64_t HardwareDisplayPlaneManagerTest::GetObjectPropertyValue(
    uint32_t object_id,
    uint32_t object_type,
    const std::string& property_name) {
  ui::DrmDevice::Property p{};
  ui::ScopedDrmObjectPropertyPtr properties(
      fake_drm_->GetObjectProperties(object_id, object_type));
  EXPECT_TRUE(ui::GetDrmPropertyForName(fake_drm_.get(), properties.get(),
                                        property_name, &p));
  return p.value;
}

uint64_t HardwareDisplayPlaneManagerTest::GetCrtcPropertyValue(
    uint32_t crtc,
    const std::string& property_name) {
  return GetObjectPropertyValue(crtc, DRM_MODE_OBJECT_CRTC, property_name);
}

uint64_t HardwareDisplayPlaneManagerTest::GetPlanePropertyValue(
    uint32_t plane,
    const std::string& property_name) {
  return GetObjectPropertyValue(plane, DRM_MODE_OBJECT_PLANE, property_name);
}

using HardwareDisplayPlaneManagerLegacyTest = HardwareDisplayPlaneManagerTest;
using HardwareDisplayPlaneManagerAtomicTest = HardwareDisplayPlaneManagerTest;

TEST_P(HardwareDisplayPlaneManagerLegacyTest, SinglePlaneAssignment) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(1u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, AddCursor) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  bool cursor_found = false;
  for (const auto& plane : fake_drm_->plane_manager()->planes()) {
    if (plane->type() == ui::HardwareDisplayPlane::kCursor) {
      cursor_found = true;
      break;
    }
  }
  EXPECT_TRUE(cursor_found);
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, BadCrtc) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(&state_, assigns,
                                                               0, nullptr));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, NotEnoughPlanes) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, MultipleCrtcs) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id, nullptr));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, MultiplePlanesAndCrtcs) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id, nullptr));
  EXPECT_EQ(0u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, CheckFramebufferFormatMatch) {
  ui::DrmOverlayPlaneList assigns;
  scoped_refptr<ui::DrmFramebuffer> buffer =
      CreateBufferWithFormat(kDefaultBufferSize, DRM_FORMAT_NV12);
  assigns.push_back(ui::DrmOverlayPlane(buffer, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  fake_drm_->plane_manager()->BeginFrame(&state_);
  // This should return false as plane manager creates planes which support
  // DRM_FORMAT_XRGB8888 while buffer returns kDummyFormat as its pixelFormat.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  assigns.clear();
  scoped_refptr<ui::DrmFramebuffer> xrgb_buffer =
      CreateBuffer(kDefaultBufferSize);
  assigns.push_back(ui::DrmOverlayPlane(xrgb_buffer, nullptr));
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultiplePlaneAssignment) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultiplePlanesAndCrtcs) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id, nullptr));
  EXPECT_EQ(4u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, SharedPlanes) {
  ui::DrmOverlayPlaneList assigns;
  scoped_refptr<ui::DrmFramebuffer> buffer = CreateBuffer(gfx::Size(1, 1));

  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(buffer, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  ui::MockDrmDevice::PlaneProperties plane_prop;
  plane_prop.id = 102;
  plane_prop.crtc_mask = (1 << 0) | (1 << 1);
  plane_prop.properties = {
      {/* .id = */ kTypePropId, /* .value = */ DRM_PLANE_TYPE_OVERLAY},
      {/* .id = */ kInFormatsPropId, /* .value = */ kInFormatsBlobPropId},
  };
  plane_properties_.emplace_back(std::move(plane_prop));
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id, nullptr));
  EXPECT_EQ(2u, state_.plane_list.size());
  // The shared plane is now unavailable for use by the other CRTC.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, UnusedPlanesAreReleased) {
  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  ui::DrmOverlayPlaneList assigns;
  scoped_refptr<ui::DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<ui::DrmFramebuffer> overlay_buffer =
      CreateBuffer(gfx::Size(1, 1));
  assigns.push_back(ui::DrmOverlayPlane(primary_buffer, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(overlay_buffer, nullptr));
  ui::HardwareDisplayPlaneList hdpl;
  ui::CrtcController crtc(fake_drm_, crtc_properties_[0].id, 0);

  scoped_refptr<ui::PageFlipRequest> page_flip_request =
      base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id, &crtc));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  assigns.clear();
  assigns.push_back(ui::DrmOverlayPlane(primary_buffer, nullptr));
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id, &crtc));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));

  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_EQ(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultipleFrames) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(1u, state_.plane_list.size());
  // Pretend we committed the frame.
  state_.plane_list.swap(state_.old_plane_list);
  fake_drm_->plane_manager()->BeginFrame(&state_);
  ui::HardwareDisplayPlane* old_plane = state_.old_plane_list[0];
  // The same plane should be used.
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(1u, state_.plane_list.size());
  EXPECT_EQ(state_.plane_list[0], old_plane);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultipleFramesDifferentPlanes) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(1u, state_.plane_list.size());
  // The other plane should be used.
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id, nullptr));
  EXPECT_EQ(2u, state_.plane_list.size());
  EXPECT_NE(state_.plane_list[0], state_.plane_list[1]);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       SetColorCorrectionOnAllCrtcPlanes_Success) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  plane_properties_[0].properties.push_back(
      {/* .id = */ kPlaneCtmId, /* .value = */ 0});
  plane_properties_[1].properties.push_back(
      {/* .id = */ kPlaneCtmId, /* .value = */ 0});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  ui::ScopedDrmColorCtmPtr ctm_blob(ui::CreateCTMBlob(std::vector<float>(9)));
  EXPECT_TRUE(fake_drm_->plane_manager()->SetColorCorrectionOnAllCrtcPlanes(
      crtc_properties_[0].id, std::move(ctm_blob)));
  EXPECT_EQ(1, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       SetColorCorrectionOnAllCrtcPlanes_NoPlaneCtmProperty) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  ui::ScopedDrmColorCtmPtr ctm_blob(ui::CreateCTMBlob(std::vector<float>(9)));
  EXPECT_FALSE(fake_drm_->plane_manager()->SetColorCorrectionOnAllCrtcPlanes(
      crtc_properties_[0].id, std::move(ctm_blob)));
  EXPECT_EQ(0, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       SetColorCorrectionOnAllCrtcPlanes_OnePlaneMissingCtmProperty) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/2);
  plane_properties_[0].properties.push_back(
      {/* .id = */ kPlaneCtmId, /* .value = */ 0});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  ui::ScopedDrmColorCtmPtr ctm_blob(ui::CreateCTMBlob(std::vector<float>(9)));
  EXPECT_FALSE(fake_drm_->plane_manager()->SetColorCorrectionOnAllCrtcPlanes(
      crtc_properties_[0].id, std::move(ctm_blob)));
  EXPECT_EQ(0, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerTest, SetColorMatrix_Success) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kCtmPropId, /* .value = */ 0});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->SetColorMatrix(
      crtc_properties_[0].id, std::vector<float>(9)));
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
#if defined(COMMIT_PROPERTIES_ON_PAGE_FLIP)
    EXPECT_EQ(1, fake_drm_->get_commit_count());
#else
    EXPECT_EQ(2, fake_drm_->get_commit_count());
#endif
    EXPECT_NE(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "CTM"));
  } else {
    EXPECT_EQ(1, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetColorMatrix_ErrorEmptyCtm) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kCtmPropId, /* .value = */ 0});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_FALSE(
      fake_drm_->plane_manager()->SetColorMatrix(crtc_properties_[0].id, {}));
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(1, fake_drm_->get_commit_count());
    EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "CTM"));
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetGammaCorrection_MissingDegamma) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kCtmPropId, /* .value = */ 0});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {{0, 0, 0}}, {}));
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    // Page flip should succeed even if the properties failed to be updated.
    EXPECT_EQ(1, fake_drm_->get_commit_count());
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }

  crtc_properties_[0].properties.push_back(
      {/* .id = */ kDegammaLutSizePropId, /* .value = */ 1});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /*use_atomic=*/true);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {{0, 0, 0}}, {}));
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    // Page flip should succeed even if the properties failed to be updated.
    EXPECT_EQ(2, fake_drm_->get_commit_count());
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetGammaCorrection_MissingGamma) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kCtmPropId, /* .value = */ 0});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {}, {{0, 0, 0}}));
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    // Page flip should succeed even if the properties failed to be updated.
    EXPECT_EQ(1, fake_drm_->get_commit_count());
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }

  crtc_properties_[0].properties.push_back(
      {/* .id = */ kGammaLutSizePropId, /* .value = */ 1});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, /*use_atomic=*/true);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {}, {{0, 0, 0}}));
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    // Page flip should succeed even if the properties failed to be updated.
    EXPECT_EQ(2, fake_drm_->get_commit_count());
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetGammaCorrection_LegacyGamma) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  fake_drm_->set_legacy_gamma_ramp_expectation(true);
  EXPECT_TRUE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {}, {{0, 0, 0}}));
  EXPECT_EQ(1, fake_drm_->get_set_gamma_ramp_count());
  EXPECT_EQ(0, fake_drm_->get_commit_count());
  EXPECT_EQ(0, fake_drm_->get_set_object_property_count());

  // Ensure disabling gamma also works on legacy.
  EXPECT_TRUE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {}, {}));
  EXPECT_EQ(2, fake_drm_->get_set_gamma_ramp_count());
  EXPECT_EQ(0, fake_drm_->get_commit_count());
  EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
}

TEST_P(HardwareDisplayPlaneManagerTest, SetGammaCorrection_Success) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kCtmPropId, /* .value = */ 0});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {{0, 0, 0}}, {}));
  EXPECT_EQ(0, fake_drm_->get_commit_count());

  crtc_properties_[0].properties.push_back(
      {/* .id = */ kDegammaLutSizePropId, /* .value = */ 1});
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kDegammaLutPropId, /* .value = */ 0});
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kGammaLutSizePropId, /* .value = */ 1});
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kGammaLutPropId, /* .value = */ 0});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  ui::HardwareDisplayPlaneList state;
  // Check that we reset the properties correctly.
  EXPECT_TRUE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {}, {}));
  if (use_atomic_) {
    PerformPageFlip(/*crtc_idx=*/0, &state);
#if defined(COMMIT_PROPERTIES_ON_PAGE_FLIP)
    EXPECT_EQ(1, fake_drm_->get_commit_count());
#else
    EXPECT_EQ(2, fake_drm_->get_commit_count());
#endif
    EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "GAMMA_LUT"));
    EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "DEGAMMA_LUT"));
  } else {
    EXPECT_EQ(2, fake_drm_->get_set_object_property_count());
  }

  EXPECT_TRUE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {{0, 0, 0}}, {{0, 0, 0}}));
  if (use_atomic_) {
    PerformPageFlip(/*crtc_idx=*/0, &state);
#if defined(COMMIT_PROPERTIES_ON_PAGE_FLIP)
    EXPECT_EQ(2, fake_drm_->get_commit_count());
#else
    EXPECT_EQ(4, fake_drm_->get_commit_count());
#endif
    EXPECT_NE(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "GAMMA_LUT"));
    EXPECT_NE(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "DEGAMMA_LUT"));
  } else {
    EXPECT_EQ(4, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetBackgroundColor_Success) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kBackgroundColorPropId, /* .value = */ 0});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);
  fake_drm_->plane_manager()->SetBackgroundColor(crtc_properties_[0].id, 0);
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(1, fake_drm_->get_commit_count());
    EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_properties_[0].id,
                                       "BACKGROUND_COLOR"));
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }

  crtc_properties_[0].properties.push_back(
      {/* .id = */ kBackgroundColorPropId, /* .value = */ 1});
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);
  fake_drm_->plane_manager()->SetBackgroundColor(crtc_properties_[0].id, 1);
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(2, fake_drm_->get_commit_count());
    EXPECT_EQ(1u, GetCrtcPropertyValue(crtc_properties_[0].id,
                                       "BACKGROUND_COLOR"));
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       CommitReturnsNullOutFenceIfOutFencePtrNotSupported) {
  scoped_refptr<ui::DrmFramebuffer> fake_buffer2 =
      CreateBuffer(kDefaultBufferSize);

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, plane_properties_,
                             property_names_, use_atomic_);

  ui::CrtcController crtc1(fake_drm_, crtc_properties_[0].id, 0);
  ui::CrtcController crtc2(fake_drm_, crtc_properties_[1].id, 0);

  ui::DrmOverlayPlaneList assigns1;
  assigns1.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  ui::DrmOverlayPlaneList assigns2;
  assigns2.push_back(ui::DrmOverlayPlane(fake_buffer2, nullptr));

  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns1, crtc_properties_[0].id, &crtc1));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns2, crtc_properties_[1].id, &crtc2));

  scoped_refptr<ui::PageFlipRequest> page_flip_request =
      base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());

  std::unique_ptr<gfx::GpuFence> out_fence;
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                 &out_fence));
  EXPECT_EQ(nullptr, out_fence);
}

TEST_P(HardwareDisplayPlaneManagerTest,
       InitializationFailsIfSupportForOutFencePropertiesIsPartial) {
  InitializeDrmState(/*crtc_count=*/3, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kOutFencePtrPropId, /* .value = */ 1});
  crtc_properties_[2].properties.push_back(
      {/* .id = */ kOutFencePtrPropId, /* .value = */ 2});

  EXPECT_FALSE(fake_drm_->InitializeStateWithResult(
      crtc_properties_, plane_properties_, property_names_, use_atomic_));
}

TEST_P(HardwareDisplayPlaneManagerTest,
       InitializationSucceedsIfSupportForOutFencePropertiesIsComplete) {
  InitializeDrmState(/*crtc_count=*/3, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {/* .id = */ kOutFencePtrPropId, /* .value = */ 1});
  crtc_properties_[1].properties.push_back(
      {/* .id = */ kOutFencePtrPropId, /* .value = */ 2});
  crtc_properties_[2].properties.push_back(
      {/* .id = */ kOutFencePtrPropId, /* .value = */ 3});

  EXPECT_TRUE(fake_drm_->InitializeStateWithResult(
      crtc_properties_, plane_properties_, property_names_, use_atomic_));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HardwareDisplayPlaneManagerTest,
                         testing::Values(false, true));

// TODO(dnicoara): Migrate as many tests as possible to the general list above.
INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HardwareDisplayPlaneManagerLegacyTest,
                         testing::Values(false));

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HardwareDisplayPlaneManagerAtomicTest,
                         testing::Values(true));

class FakeFenceFD {
 public:
  FakeFenceFD();

  std::unique_ptr<gfx::GpuFence> GetGpuFence() const;
  void Signal() const;

 private:
  base::ScopedFD read_fd;
  base::ScopedFD write_fd;
};

FakeFenceFD::FakeFenceFD() {
  int fds[2];
  base::CreateLocalNonBlockingPipe(fds);
  read_fd = base::ScopedFD(fds[0]);
  write_fd = base::ScopedFD(fds[1]);
}

std::unique_ptr<gfx::GpuFence> FakeFenceFD::GetGpuFence() const {
  gfx::GpuFenceHandle handle;
  handle.type = gfx::GpuFenceHandleType::kAndroidNativeFenceSync;
  handle.native_fd =
      base::FileDescriptor(HANDLE_EINTR(dup(read_fd.get())), true);
  return std::make_unique<gfx::GpuFence>(handle);
}

void FakeFenceFD::Signal() const {
  base::WriteFileDescriptor(write_fd.get(), "a", 1);
}

class HardwareDisplayPlaneManagerPlanesReadyTest : public testing::Test {
 protected:
  HardwareDisplayPlaneManagerPlanesReadyTest() {}

  void SetUp() override {
    auto gbm_device = std::make_unique<ui::MockGbmDevice>();
    fake_drm_ = new ui::MockDrmDevice(std::move(gbm_device));
    drm_framebuffer_ = CreateBuffer(kDefaultBufferSize);
    planes_without_fences_ = CreatePlanesWithoutFences();
    planes_with_fences_ = CreatePlanesWithFences();
  }

  void UseLegacyManager();
  void UseAtomicManager();
  void RequestPlanesReady(ui::DrmOverlayPlaneList planes);

  scoped_refptr<ui::DrmFramebuffer> CreateBuffer(const gfx::Size& size) {
    std::unique_ptr<ui::GbmBuffer> buffer =
        fake_drm_->gbm_device()->CreateBuffer(DRM_FORMAT_XRGB8888, size,
                                              GBM_BO_USE_SCANOUT);
    return ui::DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get());
  }

  ui::DrmOverlayPlaneList CreatePlanesWithoutFences() {
    ui::DrmOverlayPlaneList planes;
    planes.push_back(
        ui::DrmOverlayPlane(CreateBuffer(kDefaultBufferSize), nullptr));
    planes.push_back(
        ui::DrmOverlayPlane(CreateBuffer(kDefaultBufferSize), nullptr));
    return planes;
  }

  ui::DrmOverlayPlaneList CreatePlanesWithFences() {
    ui::DrmOverlayPlaneList planes;
    planes.push_back(ui::DrmOverlayPlane(CreateBuffer(kDefaultBufferSize),
                                         fake_fence_fd1_.GetGpuFence()));
    planes.push_back(ui::DrmOverlayPlane(CreateBuffer(kDefaultBufferSize),
                                         fake_fence_fd2_.GetGpuFence()));
    return planes;
  }

  scoped_refptr<ui::MockDrmDevice> fake_drm_;
  std::unique_ptr<ui::HardwareDisplayPlaneManager> plane_manager_;
  bool callback_called = false;
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  scoped_refptr<ui::DrmFramebuffer> drm_framebuffer_;
  const FakeFenceFD fake_fence_fd1_;
  const FakeFenceFD fake_fence_fd2_;

  ui::DrmOverlayPlaneList planes_without_fences_;
  ui::DrmOverlayPlaneList planes_with_fences_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayPlaneManagerPlanesReadyTest);
};

void HardwareDisplayPlaneManagerPlanesReadyTest::RequestPlanesReady(
    ui::DrmOverlayPlaneList planes) {
  auto set_true = [](bool* b, ui::DrmOverlayPlaneList planes) { *b = true; };
  plane_manager_->RequestPlanesReadyCallback(
      std::move(planes), base::BindOnce(set_true, &callback_called));
}

void HardwareDisplayPlaneManagerPlanesReadyTest::UseLegacyManager() {
  plane_manager_ =
      std::make_unique<ui::HardwareDisplayPlaneManagerLegacy>(fake_drm_.get());
}

void HardwareDisplayPlaneManagerPlanesReadyTest::UseAtomicManager() {
  plane_manager_ =
      std::make_unique<ui::HardwareDisplayPlaneManagerAtomic>(fake_drm_.get());
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       LegacyWithoutFencesIsAsynchronousWithoutFenceWait) {
  UseLegacyManager();
  RequestPlanesReady(ui::DrmOverlayPlane::Clone(planes_without_fences_));

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       LegacyWithFencesIsAsynchronousWithFenceWait) {
  UseLegacyManager();
  RequestPlanesReady(ui::DrmOverlayPlane::Clone(planes_with_fences_));

  EXPECT_FALSE(callback_called);

  fake_fence_fd1_.Signal();
  fake_fence_fd2_.Signal();

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       AtomicWithoutFencesIsAsynchronousWithoutFenceWait) {
  UseAtomicManager();
  RequestPlanesReady(ui::DrmOverlayPlane::Clone(planes_without_fences_));

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       AtomicWithFencesIsAsynchronousWithoutFenceWait) {
  UseAtomicManager();
  RequestPlanesReady(ui::DrmOverlayPlane::Clone(planes_with_fences_));

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

class HardwareDisplayPlaneAtomicMock : public ui::HardwareDisplayPlaneAtomic {
 public:
  HardwareDisplayPlaneAtomicMock() : ui::HardwareDisplayPlaneAtomic(1) {}
  ~HardwareDisplayPlaneAtomicMock() override {}

  bool SetPlaneData(drmModeAtomicReq* property_set,
                    uint32_t crtc_id,
                    uint32_t framebuffer,
                    const gfx::Rect& crtc_rect,
                    const gfx::Rect& src_rect,
                    const gfx::OverlayTransform transform,
                    int in_fence_fd) override {
    framebuffer_ = framebuffer;
    return true;
  }
  uint32_t framebuffer() const { return framebuffer_; }

 private:
  uint32_t framebuffer_ = 0;
};

TEST(HardwareDisplayPlaneManagerAtomic, EnableBlend) {
  auto gbm_device = std::make_unique<ui::MockGbmDevice>();
  auto drm_device =
      base::MakeRefCounted<ui::MockDrmDevice>(std::move(gbm_device));
  auto plane_manager =
      std::make_unique<ui::HardwareDisplayPlaneManagerAtomic>(drm_device.get());
  ui::HardwareDisplayPlaneList plane_list;
  HardwareDisplayPlaneAtomicMock hw_plane;
  std::unique_ptr<ui::GbmBuffer> buffer =
      drm_device->gbm_device()->CreateBuffer(
          DRM_FORMAT_XRGB8888, kDefaultBufferSize, GBM_BO_USE_SCANOUT);
  scoped_refptr<ui::DrmFramebuffer> framebuffer =
      ui::DrmFramebuffer::AddFramebuffer(drm_device, buffer.get());
  ui::DrmOverlayPlane overlay(framebuffer, nullptr);
  overlay.enable_blend = true;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, gfx::Rect(),
                              nullptr);
  EXPECT_EQ(hw_plane.framebuffer(), framebuffer->framebuffer_id());

  overlay.enable_blend = false;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, gfx::Rect(),
                              nullptr);
  EXPECT_EQ(hw_plane.framebuffer(), framebuffer->opaque_framebuffer_id());
}

}  // namespace
