// Copyright 2014 The Chromium Authors
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
#include "base/memory/scoped_refptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace {

constexpr uint32_t kPlaneOffset = 100;
constexpr uint32_t kCrtcIdBase = 500;
constexpr uint32_t kConnectorIdBase = 700;

constexpr uint32_t kActivePropId = 1000;
constexpr uint32_t kModePropId = 1001;
constexpr uint32_t kBackgroundColorPropId = 1002;
constexpr uint32_t kCtmPropId = 1003;
constexpr uint32_t kGammaLutPropId = 1004;
constexpr uint32_t kGammaLutSizePropId = 1005;
constexpr uint32_t kDegammaLutPropId = 1006;
constexpr uint32_t kDegammaLutSizePropId = 1007;
constexpr uint32_t kOutFencePtrPropId = 1008;
constexpr uint32_t kVrrEnabledPropId = 1009;

constexpr uint32_t kCrtcIdPropId = 2000;
constexpr uint32_t kTypePropId = 3010;
constexpr uint32_t kInFormatsPropId = 3011;
constexpr uint32_t kPlaneCtmId = 3012;
constexpr uint32_t kRotationPropId = 3013;

constexpr uint32_t kInFormatsBlobPropId = 400;

const gfx::Size kDefaultBufferSize(2, 2);
// Create a basic mode for a 6x4 screen.
drmModeModeInfo kDefaultMode = {.hdisplay = 6, .vdisplay = 4};

class HardwareDisplayPlaneManagerTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  HardwareDisplayPlaneManagerTest() = default;

  HardwareDisplayPlaneManagerTest(const HardwareDisplayPlaneManagerTest&) =
      delete;
  HardwareDisplayPlaneManagerTest& operator=(
      const HardwareDisplayPlaneManagerTest&) = delete;

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
    return ui::DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(), size);
  }

 protected:
  ui::HardwareDisplayPlaneList state_;
  scoped_refptr<ui::DrmFramebuffer> fake_buffer_;
  scoped_refptr<ui::MockDrmDevice> fake_drm_;

  std::vector<ui::MockDrmDevice::CrtcProperties> crtc_properties_;
  std::vector<ui::MockDrmDevice::ConnectorProperties> connector_properties_;
  std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties_;
  std::map<uint32_t, std::string> property_names_;

  bool use_atomic_ = false;
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
  std::map<uint32_t, std::string> crtc_property_names = {
      {kActivePropId, "ACTIVE"},
      {kModePropId, "MODE_ID"},
  };

  std::vector<ui::MockDrmDevice::ConnectorProperties> connector_properties(1);
  std::map<uint32_t, std::string> connector_property_names = {
      {kCrtcIdPropId, "CRTC_ID"},
  };
  for (size_t i = 0; i < connector_properties.size(); ++i) {
    connector_properties[i].id = kConnectorIdBase + i;
    for (const auto& pair : connector_property_names) {
      connector_properties[i].properties.push_back(
          {.id = pair.first, .value = 0});
    }
  }
  connector_properties_ = connector_properties;

  std::vector<ui::MockDrmDevice::PlaneProperties> plane_properties(
      planes_per_crtc * crtc_count);
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
      {kTypePropId, "type"},
      {kInFormatsPropId, "IN_FORMATS"},
      {kRotationPropId, "rotation"},
  };

  // Always add an additional cursor plane.
  ++planes_per_crtc;
  for (size_t i = 0; i < crtc_count; ++i) {
    ui::MockDrmDevice::CrtcProperties crtc_prop;
    // Start ID at 1 cause 0 is an invalid ID.
    crtc_prop.id = kCrtcIdBase + i;
    for (const auto& pair : crtc_property_names) {
      crtc_prop.properties.push_back({.id = pair.first, .value = 0});
    }
    crtc_properties_.emplace_back(std::move(crtc_prop));

    for (size_t j = 0; j < planes_per_crtc; ++j) {
      ui::MockDrmDevice::PlaneProperties plane_prop;
      plane_prop.id = kPlaneOffset + i * planes_per_crtc + j;
      plane_prop.crtc_mask = 1 << i;
      for (const auto& pair : plane_property_names) {
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
        plane_prop.properties.push_back({.id = pair.first, .value = value});
      }

      plane_properties_.emplace_back(std::move(plane_prop));
    }
  }

  property_names_.insert(crtc_property_names.begin(),
                         crtc_property_names.end());
  property_names_.insert(connector_property_names.begin(),
                         connector_property_names.end());
  property_names_.insert(plane_property_names.begin(),
                         plane_property_names.end());

  // Separately add optional properties that will be used in some tests, but the
  // tests will append the property to the planes on a case-by-case basis.
  //
  // Plane properties:
  property_names_.insert({kPlaneCtmId, "PLANE_CTM"});
  // CRTC properties:
  property_names_.insert({kBackgroundColorPropId, "BACKGROUND_COLOR"});
  property_names_.insert({kCtmPropId, "CTM"});
  property_names_.insert({kGammaLutPropId, "GAMMA_LUT"});
  property_names_.insert({kGammaLutSizePropId, "GAMMA_LUT_SIZE"});
  property_names_.insert({kDegammaLutPropId, "DEGAMMA_LUT"});
  property_names_.insert({kDegammaLutSizePropId, "DEGAMMA_LUT_SIZE"});
  property_names_.insert({kOutFencePtrPropId, "OUT_FENCE_PTR"});
  property_names_.insert({kVrrEnabledPropId, "VRR_ENABLED"});
}

void HardwareDisplayPlaneManagerTest::PerformPageFlip(
    size_t crtc_idx,
    ui::HardwareDisplayPlaneList* state) {
  ui::DrmOverlayPlaneList assigns;
  scoped_refptr<ui::DrmFramebuffer> xrgb_buffer =
      CreateBuffer(kDefaultBufferSize);
  assigns.push_back(ui::DrmOverlayPlane(xrgb_buffer, nullptr));
  fake_drm_->plane_manager()->BeginFrame(state);
  ASSERT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      state, assigns, crtc_properties_[crtc_idx].id));
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

TEST_P(HardwareDisplayPlaneManagerTest, ResettingConnectorCache) {
  const int connector_and_crtc_count = 3;
  InitializeDrmState(/*crtc_count=*/connector_and_crtc_count,
                     /*planes_per_crtc=*/1);

  // Create 3 connectors, kConnectorIdBase + 0/1/2
  std::vector<ui::MockDrmDevice::ConnectorProperties> connector_properties(
      connector_and_crtc_count);
  for (size_t i = 0; i < connector_and_crtc_count; ++i) {
    connector_properties[i].id = kConnectorIdBase + i;
    connector_properties[i].properties.push_back(
        {.id = kCrtcIdPropId, .value = 0});
  }

  fake_drm_->InitializeState(crtc_properties_, connector_properties,
                             plane_properties_, property_names_,
                             /*use_atomic=*/true);

  ui::HardwareDisplayPlaneList state;

  {
    ui::CommitRequest commit_request;
    fake_drm_->plane_manager()->BeginFrame(&state);
    // Check all 3 connectors exist
    for (size_t i = 0; i < connector_and_crtc_count; ++i) {
      ui::DrmOverlayPlaneList overlays;
      overlays.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

      ui::CrtcCommitRequest request = ui::CrtcCommitRequest::EnableCrtcRequest(
          crtc_properties_[i].id, connector_properties[i].id, kDefaultMode,
          gfx::Point(), &state, std::move(overlays));
      commit_request.push_back(std::move(request));
    }

    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
  }

  // Replace last connector and update state.
  connector_properties[connector_and_crtc_count - 1].id = kConnectorIdBase + 3;
  fake_drm_->UpdateState(crtc_properties_, connector_properties,
                         plane_properties_, property_names_);
  fake_drm_->plane_manager()->ResetConnectorsCache(fake_drm_->GetResources());

  {
    ui::CommitRequest commit_request;
    fake_drm_->plane_manager()->BeginFrame(&state);
    {
      ui::DrmOverlayPlaneList overlays;
      overlays.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
      commit_request.push_back(ui::CrtcCommitRequest::EnableCrtcRequest(
          crtc_properties_[0].id, kConnectorIdBase, kDefaultMode, gfx::Point(),
          &state, std::move(overlays)));
    }
    {
      ui::DrmOverlayPlaneList overlays;
      overlays.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
      commit_request.push_back(ui::CrtcCommitRequest::EnableCrtcRequest(
          crtc_properties_[1].id, kConnectorIdBase + 1, kDefaultMode,
          gfx::Point(), &state, std::move(overlays)));
    }
    {
      ui::DrmOverlayPlaneList overlays;
      overlays.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
      commit_request.push_back(ui::CrtcCommitRequest::EnableCrtcRequest(
          crtc_properties_[2].id, kConnectorIdBase + 3, kDefaultMode,
          gfx::Point(), &state, std::move(overlays)));
    }

    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SequenceIncrementOnModesetOnly) {
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_,
                             /*use_atomic=*/true);

  // Modeset Test
  {
    int pre_test_sequence_id = fake_drm_->modeset_sequence_id();
    ASSERT_TRUE(fake_drm_->plane_manager()->Commit(
        ui::CommitRequest(),
        DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET));
    EXPECT_EQ(pre_test_sequence_id, fake_drm_->modeset_sequence_id());
  }

  // Successful Modeset
  {
    int pre_modeset_sequence_id = fake_drm_->modeset_sequence_id();
    ASSERT_TRUE(fake_drm_->plane_manager()->Commit(
        ui::CommitRequest(), DRM_MODE_ATOMIC_ALLOW_MODESET));
    EXPECT_EQ(pre_modeset_sequence_id + 1, fake_drm_->modeset_sequence_id());
  }

  // Failed Modeset
  {
    int pre_modeset_sequence_id = fake_drm_->modeset_sequence_id();
    fake_drm_->set_set_crtc_expectation(false);
    ASSERT_FALSE(fake_drm_->plane_manager()->Commit(
        ui::CommitRequest(), DRM_MODE_ATOMIC_ALLOW_MODESET));
    fake_drm_->set_set_crtc_expectation(true);
    EXPECT_EQ(pre_modeset_sequence_id, fake_drm_->modeset_sequence_id());
  }

  // Page Flip
  {
    int pre_flip_sequence_id = fake_drm_->modeset_sequence_id();
    ASSERT_TRUE(fake_drm_->plane_manager()->Commit(ui::CommitRequest(),
                                                   DRM_MODE_ATOMIC_NONBLOCK));
    EXPECT_EQ(pre_flip_sequence_id, fake_drm_->modeset_sequence_id());
  }
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, Modeset) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_,
                             /*use_atomic=*/false);
  fake_drm_->set_set_crtc_expectation(false);

  ui::HardwareDisplayPlaneList state;
  ui::DrmOverlayPlane plane(fake_buffer_, nullptr);
  ui::CommitRequest commit_request;

  ui::DrmOverlayPlaneList overlays;
  overlays.push_back(plane.Clone());
  commit_request.push_back(ui::CrtcCommitRequest::EnableCrtcRequest(
      crtc_properties_[0].id, connector_properties_[0].id, kDefaultMode,
      gfx::Point(), &state, std::move(overlays)));
  EXPECT_FALSE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  EXPECT_EQ(plane.buffer->framebuffer_id(), fake_drm_->current_framebuffer());
  EXPECT_EQ(1, fake_drm_->get_set_crtc_call_count());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, DisableModeset) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_,
                             /*use_atomic*/ false);

  ui::HardwareDisplayPlaneList state;
  ui::CommitRequest commit_request;
  commit_request.push_back(ui::CrtcCommitRequest::DisableCrtcRequest(
      crtc_properties_[0].id, connector_properties_[0].id, &state));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, SinglePlaneAssignment) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  EXPECT_EQ(1u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, AddCursor) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  bool cursor_found = false;
  for (const auto& plane : fake_drm_->plane_manager()->planes()) {
    if (plane->type() == DRM_PLANE_TYPE_CURSOR) {
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
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_FALSE(
      fake_drm_->plane_manager()->AssignOverlayPlanes(&state_, assigns, 0));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, NotEnoughPlanes) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, MultipleCrtcs) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, MultiplePlanesAndCrtcs) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id));
  EXPECT_EQ(0u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, CheckFramebufferFormatMatch) {
  ui::DrmOverlayPlaneList assigns;
  scoped_refptr<ui::DrmFramebuffer> buffer =
      CreateBufferWithFormat(kDefaultBufferSize, DRM_FORMAT_NV12);
  assigns.push_back(ui::DrmOverlayPlane(buffer, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  fake_drm_->plane_manager()->BeginFrame(&state_);
  // This should return false as plane manager creates planes which support
  // DRM_FORMAT_XRGB8888 while buffer returns kDummyFormat as its pixelFormat.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  assigns.clear();
  scoped_refptr<ui::DrmFramebuffer> xrgb_buffer =
      CreateBuffer(kDefaultBufferSize);
  assigns.push_back(ui::DrmOverlayPlane(xrgb_buffer, nullptr));
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, Modeset) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_,
                             /*use_atomic=*/true);

  ui::HardwareDisplayPlaneList state;
  ui::CommitRequest commit_request;
  ui::DrmOverlayPlaneList overlays;
  overlays.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  commit_request.push_back(ui::CrtcCommitRequest::EnableCrtcRequest(
      crtc_properties_[0].id, connector_properties_[0].id, kDefaultMode,
      gfx::Point(), &state, std::move(overlays)));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  EXPECT_EQ(1, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, DisableModeset) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_,
                             /*use_atomic*/ true);

  ui::HardwareDisplayPlaneList state;
  ui::CommitRequest commit_request;
  commit_request.push_back(ui::CrtcCommitRequest::DisableCrtcRequest(
      crtc_properties_[0].id, connector_properties_[0].id, &state));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  EXPECT_EQ(1, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, CheckPropsAfterModeset) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_,
                             /*use_atomic=*/true);

  ui::HardwareDisplayPlaneList state;
  ui::CommitRequest commit_request;
  ui::DrmOverlayPlaneList overlays;
  overlays.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  commit_request.push_back(ui::CrtcCommitRequest::EnableCrtcRequest(
      crtc_properties_[0].id, connector_properties_[0].id, kDefaultMode,
      gfx::Point(), &state, std::move(overlays)));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  // Test props values after modesetting.
  ui::DrmDevice::Property connector_prop_crtc_id;
  ui::ScopedDrmObjectPropertyPtr connector_props =
      fake_drm_->GetObjectProperties(kConnectorIdBase,
                                     DRM_MODE_OBJECT_CONNECTOR);
  ui::GetDrmPropertyForName(fake_drm_.get(), connector_props.get(), "CRTC_ID",
                            &connector_prop_crtc_id);
  EXPECT_EQ(kCrtcIdPropId, connector_prop_crtc_id.id);

  ui::DrmDevice::Property crtc_prop_for_name;
  ui::ScopedDrmObjectPropertyPtr crtc_props =
      fake_drm_->GetObjectProperties(kCrtcIdBase, DRM_MODE_OBJECT_CRTC);
  ui::GetDrmPropertyForName(fake_drm_.get(), crtc_props.get(), "ACTIVE",
                            &crtc_prop_for_name);
  EXPECT_EQ(kActivePropId, crtc_prop_for_name.id);
  EXPECT_EQ(1U, crtc_prop_for_name.value);

  ui::GetDrmPropertyForName(fake_drm_.get(), crtc_props.get(), "MODE_ID",
                            &crtc_prop_for_name);
  EXPECT_EQ(kModePropId, crtc_prop_for_name.id);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, CheckPropsAfterDisable) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_,
                             /*use_atomic=*/true);

  ui::HardwareDisplayPlaneList state;
  {
    ui::CommitRequest commit_request;
    ui::DrmOverlayPlaneList overlays;
    overlays.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
    commit_request.push_back(ui::CrtcCommitRequest::EnableCrtcRequest(
        crtc_properties_[0].id, connector_properties_[0].id, kDefaultMode,
        gfx::Point(), &state, std::move(overlays)));
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
  }

  // Test props values after disabling.
  {
    ui::CommitRequest commit_request;
    commit_request.push_back(ui::CrtcCommitRequest::DisableCrtcRequest(
        crtc_properties_[0].id, connector_properties_[0].id, &state));
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
  }

  ui::DrmDevice::Property crtc_prop_for_name;
  ui::ScopedDrmObjectPropertyPtr crtc_props =
      fake_drm_->GetObjectProperties(kCrtcIdBase, DRM_MODE_OBJECT_CRTC);
  ui::GetDrmPropertyForName(fake_drm_.get(), crtc_props.get(), "ACTIVE",
                            &crtc_prop_for_name);
  EXPECT_EQ(kActivePropId, crtc_prop_for_name.id);
  EXPECT_EQ(0U, crtc_prop_for_name.value);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultiplePlaneAssignment) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultiplePlanesAndCrtcs) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id));
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
      {.id = kTypePropId, .value = DRM_PLANE_TYPE_OVERLAY},
      {.id = kInFormatsPropId, .value = kInFormatsBlobPropId},
  };
  plane_properties_.emplace_back(std::move(plane_prop));
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[1].id));
  EXPECT_EQ(2u, state_.plane_list.size());
  // The shared plane is now unavailable for use by the other CRTC.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, UnusedPlanesAreReleased) {
  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  ui::DrmOverlayPlaneList assigns;
  scoped_refptr<ui::DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<ui::DrmFramebuffer> overlay_buffer =
      CreateBuffer(gfx::Size(1, 1));
  assigns.push_back(ui::DrmOverlayPlane(primary_buffer, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(overlay_buffer, nullptr));
  ui::HardwareDisplayPlaneList hdpl;

  scoped_refptr<ui::PageFlipRequest> page_flip_request =
      base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  assigns.clear();
  assigns.push_back(ui::DrmOverlayPlane(primary_buffer, nullptr));
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));

  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_EQ(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, AssignPlanesRestoresInUse) {
  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  ui::DrmOverlayPlaneList assigns;
  scoped_refptr<ui::DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<ui::DrmFramebuffer> overlay_buffer =
      CreateBuffer(gfx::Size(1, 1));
  assigns.push_back(ui::DrmOverlayPlane(primary_buffer, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(overlay_buffer, nullptr));
  ui::HardwareDisplayPlaneList hdpl;

  scoped_refptr<ui::PageFlipRequest> page_flip_request =
      base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  EXPECT_TRUE(fake_drm_->plane_manager()->planes().front()->in_use());
  assigns.push_back(ui::DrmOverlayPlane(overlay_buffer, nullptr));

  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  // Assign overlay planes will fail since there aren't enough planes.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id));

  // The primary plane should still be in use since we failed to assign
  // planes and did not commit a new configuration.
  EXPECT_TRUE(fake_drm_->plane_manager()->planes().front()->in_use());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, PageflipTestRestoresInUse) {
  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  ui::DrmOverlayPlaneList assigns;
  scoped_refptr<ui::DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<ui::DrmFramebuffer> overlay_buffer =
      CreateBuffer(gfx::Size(1, 1));
  assigns.push_back(ui::DrmOverlayPlane(primary_buffer, nullptr));
  assigns.push_back(ui::DrmOverlayPlane(overlay_buffer, nullptr));
  ui::HardwareDisplayPlaneList hdpl;

  scoped_refptr<ui::PageFlipRequest> page_flip_request =
      base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  assigns.clear();
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, crtc_properties_[0].id));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&hdpl, nullptr, nullptr));
  // The primary plane should still be in use since the commit was
  // a pageflip test and did not change any KMS state.
  EXPECT_TRUE(fake_drm_->plane_manager()->planes().front()->in_use());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       PageFlipOnlySwapsPlaneListsOnSuccess) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  ui::DrmOverlayPlaneList single_assign;
  single_assign.emplace_back(CreateBuffer(kDefaultBufferSize), nullptr);

  ui::DrmOverlayPlaneList overlay_assigns;
  overlay_assigns.emplace_back(CreateBuffer(kDefaultBufferSize), nullptr);
  overlay_assigns.emplace_back(CreateBuffer(kDefaultBufferSize), nullptr);

  ui::HardwareDisplayPlaneList hdpl;

  auto flip_with_assigns = [&](bool commit_status,
                               const auto& assigns) -> bool {
    auto page_flip_request =
        base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
    fake_drm_->plane_manager()->BeginFrame(&hdpl);
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &hdpl, assigns, crtc_properties_[0].id));
    fake_drm_->set_commit_expectation(commit_status);
    return fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request,
                                              nullptr);
  };

  // Flipping with an overlay should mark both as old planes:
  EXPECT_TRUE(flip_with_assigns(/*commit_status=*/true, overlay_assigns));
  EXPECT_EQ(2u, hdpl.old_plane_list.size());
  EXPECT_EQ(0u, hdpl.plane_list.size());

  // We shouldn't see a change to the old plane list on a force-failed commit,
  // even though we only are trying to flip a single plane.
  EXPECT_FALSE(flip_with_assigns(/*commit_status=*/false, single_assign));
  EXPECT_EQ(2u, hdpl.old_plane_list.size());
  EXPECT_EQ(0u, hdpl.plane_list.size());

  // Once we do successfully flip a single plane, the old plane list should
  // reflect it.
  EXPECT_TRUE(flip_with_assigns(/*commit_status=*/true, single_assign));
  EXPECT_EQ(1u, hdpl.old_plane_list.size());
  EXPECT_EQ(0u, hdpl.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultipleFrames) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  EXPECT_EQ(1u, state_.plane_list.size());
  // Pretend we committed the frame.
  state_.plane_list.swap(state_.old_plane_list);
  fake_drm_->plane_manager()->BeginFrame(&state_);
  ui::HardwareDisplayPlane* old_plane = state_.old_plane_list[0];
  // The same plane should be used.
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  EXPECT_EQ(1u, state_.plane_list.size());
  EXPECT_EQ(state_.plane_list[0], old_plane);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultipleFramesDifferentPlanes) {
  ui::DrmOverlayPlaneList assigns;
  assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  EXPECT_EQ(1u, state_.plane_list.size());
  // The other plane should be used.
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, crtc_properties_[0].id));
  EXPECT_EQ(2u, state_.plane_list.size());
  EXPECT_NE(state_.plane_list[0], state_.plane_list[1]);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       SetColorCorrectionOnAllCrtcPlanes_Success) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  plane_properties_[0].properties.push_back({.id = kPlaneCtmId, .value = 0});
  plane_properties_[1].properties.push_back({.id = kPlaneCtmId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  ui::ScopedDrmColorCtmPtr ctm_blob(ui::CreateCTMBlob(std::vector<float>(9)));
  EXPECT_TRUE(fake_drm_->plane_manager()->SetColorCorrectionOnAllCrtcPlanes(
      crtc_properties_[0].id, std::move(ctm_blob)));
  EXPECT_EQ(1, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       SetColorCorrectionOnAllCrtcPlanes_NoPlaneCtmProperty) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  ui::ScopedDrmColorCtmPtr ctm_blob(ui::CreateCTMBlob(std::vector<float>(9)));
  EXPECT_FALSE(fake_drm_->plane_manager()->SetColorCorrectionOnAllCrtcPlanes(
      crtc_properties_[0].id, std::move(ctm_blob)));
  EXPECT_EQ(0, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       SetColorCorrectionOnAllCrtcPlanes_OnePlaneMissingCtmProperty) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/2);
  plane_properties_[0].properties.push_back({.id = kPlaneCtmId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  ui::ScopedDrmColorCtmPtr ctm_blob(ui::CreateCTMBlob(std::vector<float>(9)));
  EXPECT_FALSE(fake_drm_->plane_manager()->SetColorCorrectionOnAllCrtcPlanes(
      crtc_properties_[0].id, std::move(ctm_blob)));
  EXPECT_EQ(0, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerTest, SetColorMatrix_Success) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back({.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

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
  crtc_properties_[0].properties.push_back({.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

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
  crtc_properties_[0].properties.push_back({.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

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
      {.id = kDegammaLutSizePropId, .value = 1});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_,
                             /*use_atomic=*/true);

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
  crtc_properties_[0].properties.push_back({.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

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
      {.id = kGammaLutSizePropId, .value = 1});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_,
                             /*use_atomic=*/true);

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
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

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
  crtc_properties_[0].properties.push_back({.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      crtc_properties_[0].id, {{0, 0, 0}}, {}));
  EXPECT_EQ(0, fake_drm_->get_commit_count());

  crtc_properties_[0].properties.push_back(
      {.id = kDegammaLutSizePropId, .value = 1});
  crtc_properties_[0].properties.push_back(
      {.id = kDegammaLutPropId, .value = 0});
  crtc_properties_[0].properties.push_back(
      {.id = kGammaLutSizePropId, .value = 1});
  crtc_properties_[0].properties.push_back({.id = kGammaLutPropId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

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
      {.id = kBackgroundColorPropId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);
  fake_drm_->plane_manager()->SetBackgroundColor(crtc_properties_[0].id, 0);
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(1, fake_drm_->get_commit_count());
    EXPECT_EQ(0u,
              GetCrtcPropertyValue(crtc_properties_[0].id, "BACKGROUND_COLOR"));
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }

  crtc_properties_[0].properties.push_back(
      {.id = kBackgroundColorPropId, .value = 1});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);
  fake_drm_->plane_manager()->SetBackgroundColor(crtc_properties_[0].id, 1);
  if (use_atomic_) {
    ui::HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(2, fake_drm_->get_commit_count());
    EXPECT_EQ(1u,
              GetCrtcPropertyValue(crtc_properties_[0].id, "BACKGROUND_COLOR"));
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, SetVrrEnabled_Success) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {.id = kVrrEnabledPropId, .value = 0});
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);
  ui::HardwareDisplayPlaneList state;
  fake_drm_->plane_manager()->BeginFrame(&state);

  // Check the property is set correctly, but isn't committed until modeset.
  EXPECT_TRUE(
      fake_drm_->plane_manager()->SetVrrEnabled(crtc_properties_[0].id, true));
  {
    EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "VRR_ENABLED"));

    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "VRR_ENABLED"));

    ui::CommitRequest commit_request;
    ui::DrmOverlayPlaneList overlays;
    overlays.emplace_back(fake_buffer_, nullptr);
    commit_request.push_back(ui::CrtcCommitRequest::EnableCrtcRequest(
        crtc_properties_[0].id, connector_properties_[0].id, kDefaultMode,
        gfx::Point(), &state, std::move(overlays)));
    fake_drm_->plane_manager()->Commit(
        commit_request,
        DRM_MODE_ATOMIC_ALLOW_MODESET & DRM_MODE_ATOMIC_TEST_ONLY);
    EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "VRR_ENABLED"));

    fake_drm_->plane_manager()->Commit(commit_request,
                                       DRM_MODE_ATOMIC_ALLOW_MODESET);
    EXPECT_EQ(1u, GetCrtcPropertyValue(crtc_properties_[0].id, "VRR_ENABLED"));
  }

  // Check the property is reset correctly, but isn't committed until modeset.
  EXPECT_TRUE(
      fake_drm_->plane_manager()->SetVrrEnabled(crtc_properties_[0].id, false));
  {
    EXPECT_EQ(1u, GetCrtcPropertyValue(crtc_properties_[0].id, "VRR_ENABLED"));

    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(1u, GetCrtcPropertyValue(crtc_properties_[0].id, "VRR_ENABLED"));

    ui::CommitRequest commit_request;
    ui::DrmOverlayPlaneList overlays;
    overlays.emplace_back(fake_buffer_, nullptr);
    commit_request.push_back(ui::CrtcCommitRequest::EnableCrtcRequest(
        crtc_properties_[0].id, connector_properties_[0].id, kDefaultMode,
        gfx::Point(), &state, std::move(overlays)));
    fake_drm_->plane_manager()->Commit(
        commit_request,
        DRM_MODE_ATOMIC_ALLOW_MODESET & DRM_MODE_ATOMIC_TEST_ONLY);
    EXPECT_EQ(1u, GetCrtcPropertyValue(crtc_properties_[0].id, "VRR_ENABLED"));

    fake_drm_->plane_manager()->Commit(commit_request,
                                       DRM_MODE_ATOMIC_ALLOW_MODESET);
    EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_properties_[0].id, "VRR_ENABLED"));
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       CommitReturnsNullOutFenceIfOutFencePtrNotSupported) {
  scoped_refptr<ui::DrmFramebuffer> fake_buffer2 =
      CreateBuffer(kDefaultBufferSize);

  InitializeDrmState(/*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  ui::DrmOverlayPlaneList assigns1;
  assigns1.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));
  ui::DrmOverlayPlaneList assigns2;
  assigns2.push_back(ui::DrmOverlayPlane(fake_buffer2, nullptr));

  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns1, crtc_properties_[0].id));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns2, crtc_properties_[1].id));

  scoped_refptr<ui::PageFlipRequest> page_flip_request =
      base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());

  gfx::GpuFenceHandle release_fence;
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                 &release_fence));
  EXPECT_TRUE(release_fence.is_null());
}

TEST_P(HardwareDisplayPlaneManagerTest,
       InitializationFailsIfSupportForOutFencePropertiesIsPartial) {
  InitializeDrmState(/*crtc_count=*/3, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 1});
  crtc_properties_[2].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 2});

  EXPECT_FALSE(fake_drm_->InitializeStateWithResult(
      crtc_properties_, connector_properties_, plane_properties_,
      property_names_, use_atomic_));
}

TEST_P(HardwareDisplayPlaneManagerTest,
       InitializationSucceedsIfSupportForOutFencePropertiesIsComplete) {
  InitializeDrmState(/*crtc_count=*/3, /*planes_per_crtc=*/1);
  crtc_properties_[0].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 1});
  crtc_properties_[1].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 2});
  crtc_properties_[2].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 3});

  EXPECT_TRUE(fake_drm_->InitializeStateWithResult(
      crtc_properties_, connector_properties_, plane_properties_,
      property_names_, use_atomic_));
}

// Verifies that formats with 2 bits of alpha decay to opaques for AddFB2().
TEST_P(HardwareDisplayPlaneManagerTest, ForceOpaqueFormatsForAddFramebuffer) {
  InitializeDrmState(/*crtc_count=*/3, /*planes_per_crtc=*/1);

  struct {
    uint32_t input_fourcc;  // FourCC presented to AddFramebuffer.
    uint32_t used_fourcc;   // FourCC expected to be used in AddFramebuffer.
  } kFourCCFormats[] = {
      {DRM_FORMAT_ABGR2101010, DRM_FORMAT_XBGR2101010},
      {DRM_FORMAT_ARGB2101010, DRM_FORMAT_XRGB2101010},
  };

  for (const auto& format_pair : kFourCCFormats) {
    scoped_refptr<ui::DrmFramebuffer> drm_fb =
        CreateBufferWithFormat(kDefaultBufferSize, format_pair.input_fourcc);

    EXPECT_EQ(drm_fb->framebuffer_pixel_format(), format_pair.used_fourcc);
    EXPECT_EQ(drm_fb->opaque_framebuffer_pixel_format(),
              format_pair.used_fourcc);
  }

  // If DRM supports high-bitdepth formats with Alpha, there's no need for
  // opaque decaying. Note that we have to support all |kFourCCFormats|.
  fake_drm_->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
      kInFormatsBlobPropId, {DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010},
      {}));
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  for (const auto& format_pair : kFourCCFormats) {
    scoped_refptr<ui::DrmFramebuffer> drm_fb =
        CreateBufferWithFormat(kDefaultBufferSize, format_pair.input_fourcc);

    EXPECT_EQ(drm_fb->framebuffer_pixel_format(), format_pair.input_fourcc);
    EXPECT_EQ(drm_fb->opaque_framebuffer_pixel_format(),
              format_pair.used_fourcc);
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, GetHardwareCapabilities) {
  InitializeDrmState(/*crtc_count=*/4, /*planes_per_crtc=*/7);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  for (int i = 0; i < 4; ++i) {
    auto hc =
        fake_drm_->plane_manager()->GetHardwareCapabilities(kCrtcIdBase + i);
    EXPECT_TRUE(hc.is_valid);
    // Legacy doesn't support OVERLAY planes.
    int expected_planes = use_atomic_ ? 7 : 1;
    EXPECT_EQ(hc.num_overlay_capable_planes, expected_planes);
  }

  {
    // Change the last (CURSOR) plane into a PRIMARY plane that is available to
    // only the first two CRTCs.
    auto& last_props = plane_properties_[plane_properties_.size() - 1];
    last_props.crtc_mask = (1 << 0) | (1 << 1);
    // Find the type property and change it to PRIMARY.
    for (auto& property : last_props.properties) {
      if (property.id == kTypePropId) {
        property.value = DRM_PLANE_TYPE_PRIMARY;
        break;
      }
    }

    fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                               plane_properties_, property_names_, use_atomic_);
  }

  for (int i = 0; i < 4; ++i) {
    auto hc =
        fake_drm_->plane_manager()->GetHardwareCapabilities(kCrtcIdBase + i);

    EXPECT_TRUE(hc.is_valid);
    // Legacy doesn't support OVERLAY planes.
    int expected_planes = use_atomic_ ? 7 : 1;
    // First two CRTCs have the newly added plane available.
    if (i == 0 || i == 1) {
      expected_planes++;
    }
    EXPECT_EQ(hc.num_overlay_capable_planes, expected_planes);
  }

  {
    fake_drm_->SetDriverName(absl::nullopt);
    auto hc = fake_drm_->plane_manager()->GetHardwareCapabilities(kCrtcIdBase);
    EXPECT_FALSE(hc.is_valid);

    fake_drm_->SetDriverName("amdgpu");
    hc = fake_drm_->plane_manager()->GetHardwareCapabilities(kCrtcIdBase);
    EXPECT_TRUE(hc.is_valid);
    EXPECT_FALSE(hc.has_independent_cursor_plane);

    fake_drm_->SetDriverName("generic");
    hc = fake_drm_->plane_manager()->GetHardwareCapabilities(kCrtcIdBase);
    EXPECT_TRUE(hc.is_valid);
    EXPECT_TRUE(hc.has_independent_cursor_plane);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         HardwareDisplayPlaneManagerTest,
                         testing::Values(false, true));

// TODO(dnicoara): Migrate as many tests as possible to the general list above.
INSTANTIATE_TEST_SUITE_P(All,
                         HardwareDisplayPlaneManagerLegacyTest,
                         testing::Values(false));

INSTANTIATE_TEST_SUITE_P(All,
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
  handle.owned_fd = base::ScopedFD(HANDLE_EINTR(dup(read_fd.get())));
  return std::make_unique<gfx::GpuFence>(std::move(handle));
}

void FakeFenceFD::Signal() const {
  base::WriteFileDescriptor(write_fd.get(), "a");
}

class HardwareDisplayPlaneManagerPlanesReadyTest : public testing::Test {
 protected:
  HardwareDisplayPlaneManagerPlanesReadyTest() = default;

  HardwareDisplayPlaneManagerPlanesReadyTest(
      const HardwareDisplayPlaneManagerPlanesReadyTest&) = delete;
  HardwareDisplayPlaneManagerPlanesReadyTest& operator=(
      const HardwareDisplayPlaneManagerPlanesReadyTest&) = delete;

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
    return ui::DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(), size);
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

TEST_P(HardwareDisplayPlaneManagerAtomicTest, OriginalModifiersSupportOnly) {
  fake_drm_->SetPropertyBlob(ui::MockDrmDevice::AllocateInFormatsBlob(
      kInFormatsBlobPropId, {DRM_FORMAT_NV12}, {}));

  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  {
    ui::DrmOverlayPlaneList assigns;
    // Create as NV12 since this is required for rotation support.
    std::unique_ptr<ui::GbmBuffer> buffer =
        fake_drm_->gbm_device()->CreateBuffer(
            DRM_FORMAT_NV12, kDefaultBufferSize, GBM_BO_USE_SCANOUT);
    scoped_refptr<ui::DrmFramebuffer> framebuffer_original =
        ui::DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(),
                                           kDefaultBufferSize, {}, true);
    assigns.push_back(ui::DrmOverlayPlane(framebuffer_original, nullptr));
    assigns.back().plane_transform = gfx::OVERLAY_TRANSFORM_ROTATE_270;

    fake_drm_->plane_manager()->BeginFrame(&state_);
    // Rotation should be supported for this buffer as it is the original buffer
    // with the original modifiers.
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, crtc_properties_[0].id));

    gfx::GpuFenceHandle release_fence;
    scoped_refptr<ui::PageFlipRequest> page_flip_request =
        base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                   &release_fence));
  }

  {
    ui::DrmOverlayPlaneList assigns;
    assigns.clear();
    fake_drm_->plane_manager()->BeginFrame(&state_);
    // The test buffer would not have accurate modifiers and therefore should
    // fail rotation.
    std::unique_ptr<ui::GbmBuffer> buffer =
        fake_drm_->gbm_device()->CreateBuffer(
            DRM_FORMAT_NV12, kDefaultBufferSize, GBM_BO_USE_SCANOUT);
    scoped_refptr<ui::DrmFramebuffer> framebuffer_non_original =
        ui::DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(),
                                           kDefaultBufferSize, {}, false);
    assigns.push_back(ui::DrmOverlayPlane(framebuffer_non_original, nullptr));
    assigns.back().plane_transform = gfx::OVERLAY_TRANSFORM_ROTATE_270;
    EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, crtc_properties_[0].id));
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, OverlaySourceCrop) {
  InitializeDrmState(/*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(crtc_properties_, connector_properties_,
                             plane_properties_, property_names_, use_atomic_);

  {
    ui::DrmOverlayPlaneList assigns;
    assigns.push_back(ui::DrmOverlayPlane(fake_buffer_, nullptr));

    fake_drm_->plane_manager()->BeginFrame(&state_);
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, crtc_properties_[0].id));

    gfx::GpuFenceHandle release_fence;
    scoped_refptr<ui::PageFlipRequest> page_flip_request =
        base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                   &release_fence));

    EXPECT_EQ(2u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_W"));
    EXPECT_EQ(2u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_H"));
  }

  {
    ui::DrmOverlayPlaneList assigns;
    assigns.push_back(ui::DrmOverlayPlane(
        fake_buffer_, 0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        gfx::Rect(kDefaultBufferSize), gfx::RectF(0, 0, .5, 1), false,
        nullptr));

    fake_drm_->plane_manager()->BeginFrame(&state_);
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, crtc_properties_[0].id));

    scoped_refptr<ui::PageFlipRequest> page_flip_request =
        base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
    gfx::GpuFenceHandle release_fence;
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                   &release_fence));

    EXPECT_EQ(1u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_W"));
    EXPECT_EQ(2u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_H"));
  }

  {
    ui::DrmOverlayPlaneList assigns;
    assigns.push_back(ui::DrmOverlayPlane(
        fake_buffer_, 0, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
        gfx::Rect(kDefaultBufferSize), gfx::RectF(0, 0, .999, .501), false,
        nullptr));

    fake_drm_->plane_manager()->BeginFrame(&state_);
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, crtc_properties_[0].id));

    scoped_refptr<ui::PageFlipRequest> page_flip_request =
        base::MakeRefCounted<ui::PageFlipRequest>(base::TimeDelta());
    gfx::GpuFenceHandle release_fence;
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                   &release_fence));

    EXPECT_EQ(2u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_W"));
    EXPECT_EQ(1u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_H"));
  }
}

class HardwareDisplayPlaneAtomicMock : public ui::HardwareDisplayPlaneAtomic {
 public:
  HardwareDisplayPlaneAtomicMock() : ui::HardwareDisplayPlaneAtomic(1) {}
  ~HardwareDisplayPlaneAtomicMock() override = default;

  bool AssignPlaneProps(uint32_t crtc_id,
                        uint32_t framebuffer,
                        const gfx::Rect& crtc_rect,
                        const gfx::Rect& src_rect,
                        const gfx::OverlayTransform transform,
                        int in_fence_fd,
                        uint32_t format_fourcc,
                        bool is_original_buffer) override {
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
      ui::DrmFramebuffer::AddFramebuffer(drm_device, buffer.get(),
                                         kDefaultBufferSize);
  ui::DrmOverlayPlane overlay(framebuffer, nullptr);
  overlay.enable_blend = true;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, gfx::Rect());
  EXPECT_EQ(hw_plane.framebuffer(), framebuffer->framebuffer_id());

  overlay.enable_blend = false;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, gfx::Rect());
  EXPECT_EQ(hw_plane.framebuffer(), framebuffer->opaque_framebuffer_id());
}

}  // namespace
