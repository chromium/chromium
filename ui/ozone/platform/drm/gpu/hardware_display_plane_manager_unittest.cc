// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <drm_fourcc.h>
#include <stdint.h>
#include <unistd.h>
#include <xf86drmMode.h>

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_color_management.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/fake_drm_device.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

namespace {

using testing::_;
using testing::Return;

// TODO(crbug.com/40945652): These tests should not use a single-point
// curve as the non-empty value (it is arguably not a valid input).
const display::GammaCurve kNonemptyGammaCurve({{0, 0, 0}});
const display::GammaCurve kEmptyGammaCurve;

const gfx::Size kDefaultBufferSize(2, 2);
// Create a basic mode for a 6x4 screen.
drmModeModeInfo kDefaultMode = {.hdisplay = 6, .vdisplay = 4};

}  // namespace

class HardwareDisplayPlaneManagerTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  HardwareDisplayPlaneManagerTest() = default;

  HardwareDisplayPlaneManagerTest(const HardwareDisplayPlaneManagerTest&) =
      delete;
  HardwareDisplayPlaneManagerTest& operator=(
      const HardwareDisplayPlaneManagerTest&) = delete;

  uint64_t GetObjectPropertyValue(uint32_t object_id,
                                  uint32_t object_type,
                                  const std::string& property_name);
  uint64_t GetCrtcPropertyValue(uint32_t crtc,
                                const std::string& property_name);
  uint64_t GetPlanePropertyValue(uint32_t plane,
                                 const std::string& property_name);

  void PerformPageFlip(size_t crtc_idx, HardwareDisplayPlaneList* state);
  void PerformPageFlip(size_t crtc_idx,
                       HardwareDisplayPlaneList* state,
                       DrmOverlayPlaneList& assigns);
  void PerformFailingPageFlip(size_t crtc_idx,
                              HardwareDisplayPlaneList* state,
                              DrmOverlayPlaneList& assigns);

  uint32_t AddConnector(uint32_t possible_crtcs) {
    FakeDrmDevice::EncoderProperties& encoder = fake_drm_->AddEncoder();
    encoder.possible_crtcs = possible_crtcs;
    const uint32_t encoder_id = encoder.id;

    FakeDrmDevice::ConnectorProperties& connector = fake_drm_->AddConnector();
    connector.connection = true;
    connector.encoders = std::vector<uint32_t>{encoder_id};
    return connector.id;
  }

  void SetUp() override;
  void TearDown() override;

  scoped_refptr<DrmFramebuffer> CreateBuffer(const gfx::Size& size) {
    return CreateBufferWithFormat(size, DRM_FORMAT_XRGB8888);
  }

  scoped_refptr<DrmFramebuffer> CreateBufferWithFormat(const gfx::Size& size,
                                                       uint32_t format) {
    std::unique_ptr<GbmBuffer> buffer =
        fake_drm_->gbm_device()->CreateBuffer(format, size, GBM_BO_USE_SCANOUT);
    return DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(), size);
  }

 protected:
  HardwareDisplayPlaneList state_;
  scoped_refptr<DrmFramebuffer> fake_buffer_;
  scoped_refptr<FakeDrmDevice> fake_drm_;

  bool use_atomic_ = false;
};

void HardwareDisplayPlaneManagerTest::SetUp() {
  use_atomic_ = GetParam();

  auto gbm_device = std::make_unique<MockGbmDevice>();
  fake_drm_ = new FakeDrmDevice(std::move(gbm_device));

  fake_buffer_ = CreateBuffer(kDefaultBufferSize);
}

void HardwareDisplayPlaneManagerTest::TearDown() {
  fake_drm_->ResetPlaneManagerForTesting();
}

void HardwareDisplayPlaneManagerTest::PerformPageFlip(
    size_t crtc_idx,
    HardwareDisplayPlaneList* state) {
  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> xrgb_buffer = CreateBuffer(kDefaultBufferSize);
  assigns.push_back(DrmOverlayPlane::TestPlane(xrgb_buffer));
  PerformPageFlip(crtc_idx, state, assigns);
}

void HardwareDisplayPlaneManagerTest::PerformPageFlip(
    size_t crtc_idx,
    HardwareDisplayPlaneList* state,
    DrmOverlayPlaneList& assigns) {
  fake_drm_->plane_manager()->BeginFrame(state);
  ASSERT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      state, assigns, fake_drm_->crtc_property(crtc_idx).id));
  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());

  fake_drm_->set_commit_expectation(true);

  ASSERT_TRUE(
      fake_drm_->plane_manager()->Commit(state, page_flip_request, nullptr));
}

void HardwareDisplayPlaneManagerTest::PerformFailingPageFlip(
    size_t crtc_idx,
    HardwareDisplayPlaneList* state,
    DrmOverlayPlaneList& assigns) {
  fake_drm_->plane_manager()->BeginFrame(state);
  ASSERT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      state, assigns, fake_drm_->crtc_property(crtc_idx).id));
  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());

  fake_drm_->set_commit_expectation(false);

  ASSERT_FALSE(
      fake_drm_->plane_manager()->Commit(state, page_flip_request, nullptr));
}

uint64_t HardwareDisplayPlaneManagerTest::GetObjectPropertyValue(
    uint32_t object_id,
    uint32_t object_type,
    const std::string& property_name) {
  DrmDevice::Property p{};
  ScopedDrmObjectPropertyPtr properties(
      fake_drm_->GetObjectProperties(object_id, object_type));
  EXPECT_TRUE(GetDrmPropertyForName(fake_drm_.get(), properties.get(),
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
  auto& drm_state =
      fake_drm_->ResetStateWithDefaultObjects(connector_and_crtc_count,
                                              /*planes_per_crtc=*/1);

  drm_state.connector_properties.clear();
  // Create 3 connectors, kConnectorIdBase + 0/1/2
  for (size_t i = 0; i < connector_and_crtc_count; ++i) {
    auto& connector_properties = drm_state.connector_properties.emplace_back();
    connector_properties.id = kConnectorIdBase + i;
    connector_properties.connection = true;
    connector_properties.properties.push_back(
        {.id = kCrtcIdPropId, .value = 0});
  }

  fake_drm_->InitializeState(/*use_atomic=*/true);

  HardwareDisplayPlaneList state;

  {
    CommitRequest commit_request;
    fake_drm_->plane_manager()->BeginFrame(&state);
    // Check all 3 connectors exist
    for (size_t i = 0; i < connector_and_crtc_count; ++i) {
      DrmOverlayPlaneList overlays;
      overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

      CrtcCommitRequest request = CrtcCommitRequest::EnableCrtcRequest(
          fake_drm_->crtc_property(i).id, fake_drm_->connector_property(i).id,
          kDefaultMode, gfx::Point(), &state, std::move(overlays),
          /*enable_vrr=*/false);
      commit_request.push_back(std::move(request));
    }

    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
  }

  // Replace last connector and update state.
  drm_state.connector_properties[connector_and_crtc_count - 1].id =
      kConnectorIdBase + 3;
  fake_drm_->plane_manager()->ResetConnectorsCacheAndGetValidIds(
      fake_drm_->GetResources());

  {
    CommitRequest commit_request;
    fake_drm_->plane_manager()->BeginFrame(&state);
    {
      DrmOverlayPlaneList overlays;
      overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
      commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
          fake_drm_->crtc_property(0).id, kConnectorIdBase, kDefaultMode,
          gfx::Point(), &state, std::move(overlays), /*enable_vrr=*/false));
    }
    {
      DrmOverlayPlaneList overlays;
      overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
      commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
          fake_drm_->crtc_property(1).id, kConnectorIdBase + 1, kDefaultMode,
          gfx::Point(), &state, std::move(overlays), /*enable_vrr=*/false));
    }
    {
      DrmOverlayPlaneList overlays;
      overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
      commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
          fake_drm_->crtc_property(2).id, kConnectorIdBase + 3, kDefaultMode,
          gfx::Point(), &state, std::move(overlays), /*enable_vrr=*/false));
    }

    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SequenceIncrementOnModesetOnly) {
  fake_drm_->ResetStateWithNoProperties();
  // Add some resources so HardwareDisplayPlaneManager can properly initialize
  // within |fake_drm_|.
  fake_drm_->AddCrtcAndConnector();
  fake_drm_->InitializeState(/*use_atomic=*/true);

  // Modeset Test
  {
    int pre_test_sequence_id = fake_drm_->modeset_sequence_id();
    ASSERT_TRUE(fake_drm_->plane_manager()->Commit(
        CommitRequest(),
        DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET));
    EXPECT_EQ(pre_test_sequence_id, fake_drm_->modeset_sequence_id());
  }

  // Successful Modeset
  {
    int pre_modeset_sequence_id = fake_drm_->modeset_sequence_id();
    ASSERT_TRUE(fake_drm_->plane_manager()->Commit(
        CommitRequest(), DRM_MODE_ATOMIC_ALLOW_MODESET));
    EXPECT_EQ(pre_modeset_sequence_id + 1, fake_drm_->modeset_sequence_id());
  }

  // Failed Modeset
  {
    int pre_modeset_sequence_id = fake_drm_->modeset_sequence_id();
    fake_drm_->set_set_crtc_expectation(false);
    ASSERT_FALSE(fake_drm_->plane_manager()->Commit(
        CommitRequest(), DRM_MODE_ATOMIC_ALLOW_MODESET));
    fake_drm_->set_set_crtc_expectation(true);
    EXPECT_EQ(pre_modeset_sequence_id, fake_drm_->modeset_sequence_id());
  }

  // Page Flip
  {
    int pre_flip_sequence_id = fake_drm_->modeset_sequence_id();
    ASSERT_TRUE(fake_drm_->plane_manager()->Commit(CommitRequest(),
                                                   DRM_MODE_ATOMIC_NONBLOCK));
    EXPECT_EQ(pre_flip_sequence_id, fake_drm_->modeset_sequence_id());
  }
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, Modeset) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/false);

  fake_drm_->set_set_crtc_expectation(false);

  HardwareDisplayPlaneList state;
  DrmOverlayPlane plane(DrmOverlayPlane::TestPlane(fake_buffer_));
  CommitRequest commit_request;

  DrmOverlayPlaneList overlays;
  overlays.push_back(plane.Clone());
  commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
      fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
      kDefaultMode, gfx::Point(), &state, std::move(overlays),
      /*enable_vrr=*/false));
  EXPECT_FALSE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  EXPECT_EQ(plane.buffer->framebuffer_id(), fake_drm_->current_framebuffer());
  EXPECT_EQ(1, fake_drm_->get_set_crtc_call_count());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, DisableModeset) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/false);

  HardwareDisplayPlaneList state;
  CommitRequest commit_request;
  commit_request.push_back(CrtcCommitRequest::DisableCrtcRequest(
      fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
      &state));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, SinglePlaneAssignment) {
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_EQ(1u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, AddCursor) {
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(use_atomic_);

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
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_FALSE(
      fake_drm_->plane_manager()->AssignOverlayPlanes(&state_, assigns, 0));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, NotEnoughPlanes) {
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, MultipleCrtcs) {
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(1).id));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, MultiplePlanesAndCrtcs) {
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(1).id));
  EXPECT_EQ(0u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, CheckFramebufferFormatMatch) {
  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> buffer =
      CreateBufferWithFormat(kDefaultBufferSize, DRM_FORMAT_NV12);
  assigns.push_back(DrmOverlayPlane::TestPlane(buffer));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(use_atomic_);

  fake_drm_->plane_manager()->BeginFrame(&state_);
  // This should return false as plane manager creates planes which support
  // DRM_FORMAT_XRGB8888 while buffer returns kDummyFormat as its pixelFormat.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  assigns.clear();
  scoped_refptr<DrmFramebuffer> xrgb_buffer = CreateBuffer(kDefaultBufferSize);
  assigns.push_back(DrmOverlayPlane::TestPlane(xrgb_buffer));
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, Modeset) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/true);

  HardwareDisplayPlaneList state;
  CommitRequest commit_request;
  DrmOverlayPlaneList overlays;
  overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
      fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
      kDefaultMode, gfx::Point(), &state, std::move(overlays),
      /*enable_vrr=*/false));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  EXPECT_EQ(1, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, DisableModeset) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/true);

  HardwareDisplayPlaneList state;
  CommitRequest commit_request;
  commit_request.push_back(CrtcCommitRequest::DisableCrtcRequest(
      fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
      &state));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  EXPECT_EQ(1, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, CheckPropsAfterModeset) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/true);

  HardwareDisplayPlaneList state;
  CommitRequest commit_request;
  DrmOverlayPlaneList overlays;
  overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
      fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
      kDefaultMode, gfx::Point(), &state, std::move(overlays),
      /*enable_vrr=*/false));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  // Test props values after modesetting.
  DrmDevice::Property connector_prop_crtc_id;
  ScopedDrmObjectPropertyPtr connector_props = fake_drm_->GetObjectProperties(
      kConnectorIdBase, DRM_MODE_OBJECT_CONNECTOR);
  GetDrmPropertyForName(fake_drm_.get(), connector_props.get(), "CRTC_ID",
                        &connector_prop_crtc_id);
  EXPECT_EQ(kCrtcIdPropId, connector_prop_crtc_id.id);

  DrmDevice::Property crtc_prop_for_name;
  ScopedDrmObjectPropertyPtr crtc_props =
      fake_drm_->GetObjectProperties(kCrtcIdBase, DRM_MODE_OBJECT_CRTC);
  GetDrmPropertyForName(fake_drm_.get(), crtc_props.get(), "ACTIVE",
                        &crtc_prop_for_name);
  EXPECT_EQ(kActivePropId, crtc_prop_for_name.id);
  EXPECT_EQ(1U, crtc_prop_for_name.value);

  GetDrmPropertyForName(fake_drm_.get(), crtc_props.get(), "MODE_ID",
                        &crtc_prop_for_name);
  EXPECT_EQ(kModePropId, crtc_prop_for_name.id);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, CheckPropsAfterDisable) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/true);

  HardwareDisplayPlaneList state;
  {
    CommitRequest commit_request;
    DrmOverlayPlaneList overlays;
    overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
    commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
        fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
        kDefaultMode, gfx::Point(), &state, std::move(overlays),
        /*enable_vrr=*/false));
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
  }

  // Test props values after disabling.
  {
    CommitRequest commit_request;
    commit_request.push_back(CrtcCommitRequest::DisableCrtcRequest(
        fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
        &state));
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
  }

  DrmDevice::Property crtc_prop_for_name;
  ScopedDrmObjectPropertyPtr crtc_props =
      fake_drm_->GetObjectProperties(kCrtcIdBase, DRM_MODE_OBJECT_CRTC);
  GetDrmPropertyForName(fake_drm_.get(), crtc_props.get(), "ACTIVE",
                        &crtc_prop_for_name);
  EXPECT_EQ(kActivePropId, crtc_prop_for_name.id);
  EXPECT_EQ(0U, crtc_prop_for_name.value);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, CheckVrrAfterModeset) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/2);
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kVrrEnabledPropId, .value = 0});
  fake_drm_->InitializeState(/*use_atomic=*/true);
  HardwareDisplayPlaneList state;

  // Check initial VRR_ENABLED state.
  {
    DrmDevice::Property crtc_prop_vrr_enabled;
    ScopedDrmObjectPropertyPtr crtc_props =
        fake_drm_->GetObjectProperties(kCrtcIdBase, DRM_MODE_OBJECT_CRTC);
    GetDrmPropertyForName(fake_drm_.get(), crtc_props.get(), "VRR_ENABLED",
                          &crtc_prop_vrr_enabled);
    EXPECT_EQ(kVrrEnabledPropId, crtc_prop_vrr_enabled.id);
    EXPECT_EQ(0U, crtc_prop_vrr_enabled.value);
  }

  // Check VRR_ENABLED state is set by modeset.
  {
    CommitRequest commit_request;
    DrmOverlayPlaneList overlays;
    overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
    commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
        fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
        kDefaultMode, gfx::Point(), &state, std::move(overlays),
        /*enable_vrr=*/true));
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

    DrmDevice::Property crtc_prop_vrr_enabled;
    ScopedDrmObjectPropertyPtr crtc_props =
        fake_drm_->GetObjectProperties(kCrtcIdBase, DRM_MODE_OBJECT_CRTC);
    GetDrmPropertyForName(fake_drm_.get(), crtc_props.get(), "VRR_ENABLED",
                          &crtc_prop_vrr_enabled);
    EXPECT_EQ(kVrrEnabledPropId, crtc_prop_vrr_enabled.id);
    EXPECT_EQ(1U, crtc_prop_vrr_enabled.value);
  }

  // Check VRR_ENABLED is reset by modeset.
  {
    CommitRequest commit_request;
    DrmOverlayPlaneList overlays;
    overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
    commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
        fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
        kDefaultMode, gfx::Point(), &state, std::move(overlays),
        /*enable_vrr=*/false));
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

    DrmDevice::Property crtc_prop_vrr_enabled;
    ScopedDrmObjectPropertyPtr crtc_props =
        fake_drm_->GetObjectProperties(kCrtcIdBase, DRM_MODE_OBJECT_CRTC);
    GetDrmPropertyForName(fake_drm_.get(), crtc_props.get(), "VRR_ENABLED",
                          &crtc_prop_vrr_enabled);
    EXPECT_EQ(kVrrEnabledPropId, crtc_prop_vrr_enabled.id);
    EXPECT_EQ(0U, crtc_prop_vrr_enabled.value);
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultiplePlaneAssignment) {
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultiplePlanesAndCrtcs) {
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(1).id));
  EXPECT_EQ(4u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, SharedPlanes) {
  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> buffer = CreateBuffer(gfx::Size(1, 1));

  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  assigns.push_back(DrmOverlayPlane::TestPlane(buffer));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);

  auto in_formats_blob =
      fake_drm_->CreateInFormatsBlob({DRM_FORMAT_XRGB8888}, {});

  auto plane_prop = fake_drm_->AddPlane(
      {fake_drm_->crtc_property(0).id, fake_drm_->crtc_property(1).id},
      DRM_PLANE_TYPE_OVERLAY);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(1).id));
  EXPECT_EQ(2u, state_.plane_list.size());
  // The shared plane is now unavailable for use by the other CRTC.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, UnusedPlanesAreReleased) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(use_atomic_);

  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<DrmFramebuffer> overlay_buffer = CreateBuffer(gfx::Size(1, 1));
  assigns.push_back(DrmOverlayPlane::TestPlane(primary_buffer));
  assigns.push_back(DrmOverlayPlane::TestPlane(overlay_buffer));
  HardwareDisplayPlaneList hdpl;

  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  assigns.clear();
  assigns.push_back(DrmOverlayPlane::TestPlane(primary_buffer));
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));

  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  EXPECT_NE(0u, GetPlanePropertyValue(kPlaneOffset, "FB_ID"));
  EXPECT_EQ(0u, GetPlanePropertyValue(kPlaneOffset + 1, "FB_ID"));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, AssignPlanesRestoresInUse) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(use_atomic_);

  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<DrmFramebuffer> overlay_buffer = CreateBuffer(gfx::Size(1, 1));
  assigns.push_back(DrmOverlayPlane::TestPlane(primary_buffer));
  assigns.push_back(DrmOverlayPlane::TestPlane(overlay_buffer));
  HardwareDisplayPlaneList hdpl;

  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  EXPECT_TRUE(fake_drm_->plane_manager()->planes().front()->in_use());
  assigns.push_back(DrmOverlayPlane::TestPlane(overlay_buffer));

  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  // Assign overlay planes will fail since there aren't enough planes.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id));

  // The primary plane should still be in use since we failed to assign
  // planes and did not commit a new configuration.
  EXPECT_TRUE(fake_drm_->plane_manager()->planes().front()->in_use());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, PageflipTestRestoresInUse) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(use_atomic_);

  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<DrmFramebuffer> overlay_buffer = CreateBuffer(gfx::Size(1, 1));
  assigns.push_back(DrmOverlayPlane::TestPlane(primary_buffer));
  assigns.push_back(DrmOverlayPlane::TestPlane(overlay_buffer));
  HardwareDisplayPlaneList hdpl;

  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  assigns.clear();
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&hdpl, nullptr, nullptr));
  // The primary plane should still be in use since the commit was
  // a pageflip test and did not change any KMS state.
  EXPECT_TRUE(fake_drm_->plane_manager()->planes().front()->in_use());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       PageFlipOnlySwapsPlaneListsOnSuccess) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(use_atomic_);

  DrmOverlayPlaneList single_assign;
  single_assign.push_back(
      DrmOverlayPlane::TestPlane(CreateBuffer(kDefaultBufferSize)));

  DrmOverlayPlaneList overlay_assigns;
  overlay_assigns.push_back(
      DrmOverlayPlane::TestPlane(CreateBuffer(kDefaultBufferSize)));
  overlay_assigns.push_back(
      DrmOverlayPlane::TestPlane(CreateBuffer(kDefaultBufferSize)));

  HardwareDisplayPlaneList hdpl;

  auto flip_with_assigns = [&](bool commit_status,
                               const auto& assigns) -> bool {
    auto page_flip_request =
        base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
    fake_drm_->plane_manager()->BeginFrame(&hdpl);
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &hdpl, assigns, fake_drm_->crtc_property(0).id));
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
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_EQ(1u, state_.plane_list.size());
  // Pretend we committed the frame.
  state_.plane_list.swap(state_.old_plane_list);
  fake_drm_->plane_manager()->BeginFrame(&state_);
  HardwareDisplayPlane* old_plane = state_.old_plane_list[0];
  // The same plane should be used.
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_EQ(1u, state_.plane_list.size());
  EXPECT_EQ(state_.plane_list[0], old_plane);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultipleFramesDifferentPlanes) {
  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_EQ(1u, state_.plane_list.size());
  // The other plane should be used.
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_EQ(2u, state_.plane_list.size());
  EXPECT_NE(state_.plane_list[0], state_.plane_list[1]);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, PlanePinningAndUnpinning) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count*/ 2,
      /*planes_per_crtc=*/1,
      /*movable_planes=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/true);

  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

  DrmOverlayPlaneList assigns_with_overlay;
  assigns_with_overlay.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  assigns_with_overlay.push_back(
      DrmOverlayPlane::TestPlane(CreateBuffer(gfx::Size(1, 1))));

  auto get_overlay_owner = [&]() {
    for (const auto& plane : fake_drm_->plane_manager()->planes()) {
      if (plane->type() == DRM_PLANE_TYPE_OVERLAY) {
        return plane->owning_crtc();
      }
    }
    NOTREACHED_IN_MIGRATION();
    return UINT32_MAX;
  };

  uint32_t crtc_0 = fake_drm_->crtc_property(0).id;
  uint32_t crtc_1 = fake_drm_->crtc_property(1).id;
  HardwareDisplayPlaneList list_0;
  HardwareDisplayPlaneList list_1;

  EXPECT_EQ(0u, get_overlay_owner());

  PerformPageFlip(0, &list_0, assigns_with_overlay);
  EXPECT_EQ(crtc_0, get_overlay_owner())
      << "Assigning a plane should pin it to the CRTC.";
  fake_drm_->RunCallbacks();

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &list_1, assigns_with_overlay, crtc_1))
      << "Pinned planes should be unassignable while they're pinned.";

  PerformPageFlip(0, &list_0, assigns);
  EXPECT_EQ(0u, get_overlay_owner())
      << "Assigning without overlays should unpin the overlay.";

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &list_1, assigns_with_overlay, crtc_1))
      << "Previously pinned planes should be available for use after "
         "unpinning.";
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, PlanesUnpinnedOnFailedFlip) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count*/ 2,
      /*planes_per_crtc=*/1,
      /*movable_planes=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/true);

  DrmOverlayPlaneList assigns_with_overlay;
  assigns_with_overlay.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  assigns_with_overlay.push_back(
      DrmOverlayPlane::TestPlane(CreateBuffer(gfx::Size(1, 1))));

  auto get_overlay_owner = [&]() {
    for (const auto& plane : fake_drm_->plane_manager()->planes()) {
      if (plane->type() == DRM_PLANE_TYPE_OVERLAY) {
        return plane->owning_crtc();
      }
    }
    NOTREACHED_IN_MIGRATION();
    return UINT32_MAX;
  };

  uint32_t crtc_0 = fake_drm_->crtc_property(0).id;
  HardwareDisplayPlaneList hdpl;

  EXPECT_EQ(0u, get_overlay_owner());

  PerformPageFlip(0, &hdpl, assigns_with_overlay);
  EXPECT_EQ(crtc_0, get_overlay_owner())
      << "Assigning a plane should pin it to the CRTC.";
  fake_drm_->RunCallbacks();

  PerformFailingPageFlip(0, &hdpl, assigns_with_overlay);
  EXPECT_EQ(0u, get_overlay_owner())
      << "A failed flip should result in the overlay being freed again.";
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, PlanesUnpinnedOnDisable) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count*/ 2,
      /*planes_per_crtc=*/1,
      /*movable_planes=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/true);

  DrmOverlayPlaneList assigns_with_overlay;
  assigns_with_overlay.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  assigns_with_overlay.push_back(
      DrmOverlayPlane::TestPlane(CreateBuffer(gfx::Size(1, 1))));

  auto get_overlay_owner = [&]() {
    for (const auto& plane : fake_drm_->plane_manager()->planes()) {
      if (plane->type() == DRM_PLANE_TYPE_OVERLAY) {
        return plane->owning_crtc();
      }
    }
    NOTREACHED_IN_MIGRATION();
    return UINT32_MAX;
  };

  uint32_t crtc_0 = fake_drm_->crtc_property(0).id;
  HardwareDisplayPlaneList hdpl;

  EXPECT_EQ(0u, get_overlay_owner());

  PerformPageFlip(0, &hdpl, assigns_with_overlay);
  EXPECT_EQ(crtc_0, get_overlay_owner())
      << "Assigning a plane should pin it to the CRTC.";
  fake_drm_->RunCallbacks();

  fake_drm_->plane_manager()->DisableOverlayPlanes(&hdpl);
  EXPECT_EQ(0u, get_overlay_owner())
      << "After disabling, the pinned overlay owner should be reset.";
}

TEST_P(HardwareDisplayPlaneManagerTest, ColorManagement_Temperature) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);

  // This test has full CTM, DEGAMMA, and GAMMA.
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kCtmPropId, .value = 0});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kDegammaLutSizePropId, .value = 1});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kDegammaLutPropId, .value = 0});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kGammaLutSizePropId, .value = 1});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kGammaLutPropId, .value = 0});

  // Color temperature adjustment will set all properties.
  fake_drm_->InitializeState(use_atomic_);
  display::ColorTemperatureAdjustment cta;
  cta.srgb_matrix.vals[0][0] = 1.0;
  cta.srgb_matrix.vals[1][1] = 0.7;
  cta.srgb_matrix.vals[2][2] = 0.3;
  fake_drm_->plane_manager()->SetColorTemperatureAdjustment(
      fake_drm_->crtc_property(0).id, cta);

  if (use_atomic_) {
    // The color temperature adjustment will get its own commit.
    EXPECT_EQ(1, fake_drm_->get_commit_count());
    EXPECT_NE(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "CTM"));

    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(2, fake_drm_->get_commit_count());
    EXPECT_NE(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "CTM"));
    EXPECT_EQ(
        0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "GAMMA_LUT"));
    EXPECT_EQ(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id,
                                       "DEGAMMA_LUT"));
  } else {
    EXPECT_EQ(3, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, ColorManagement_Profile) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);

  // This test has full CTM, DEGAMMA, and GAMMA.
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kCtmPropId, .value = 0});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kDegammaLutSizePropId, .value = 1});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kDegammaLutPropId, .value = 0});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kGammaLutSizePropId, .value = 1});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kGammaLutPropId, .value = 0});

  // Color profile change will set all properties.
  fake_drm_->InitializeState(use_atomic_);
  display::ColorCalibration calibration;
  calibration.srgb_to_linear = display::GammaCurve::MakeGamma(2.2f);
  calibration.linear_to_device = display::GammaCurve::MakeGamma(1.f / 2.2);
  fake_drm_->plane_manager()->SetColorCalibration(
      fake_drm_->crtc_property(0).id, calibration);

  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(2, fake_drm_->get_commit_count());
    EXPECT_NE(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "CTM"));
    EXPECT_NE(
        0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "GAMMA_LUT"));
    EXPECT_NE(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id,
                                       "DEGAMMA_LUT"));
  } else {
    EXPECT_EQ(3, fake_drm_->get_set_object_property_count());
  }
}

// The effect of color temperature adjustment (night light) on the CTM.
TEST_P(HardwareDisplayPlaneManagerTest,
       CtmColorManagement_ColorTemperatureAdjustment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({display::features::kCtmColorManagement},
                                       {});

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);

  uint32_t crtc_id = fake_drm_->crtc_property(0).id;

  // This test has full CTM, DEGAMMA, and GAMMA.
  fake_drm_->AddProperty(crtc_id, {.id = kCtmPropId, .value = 0});
  fake_drm_->AddProperty(crtc_id, {.id = kDegammaLutSizePropId, .value = 33});
  fake_drm_->AddProperty(crtc_id, {.id = kDegammaLutPropId, .value = 0});
  fake_drm_->AddProperty(crtc_id, {.id = kGammaLutSizePropId, .value = 33});
  fake_drm_->AddProperty(crtc_id, {.id = kGammaLutPropId, .value = 0});

  // Color profile change will set all properties.
  fake_drm_->InitializeState(use_atomic_);

  // Apply color temperature adjustment. The CTM should be updated
  // immediately.
  display::ColorTemperatureAdjustment cta;
  cta.srgb_matrix.vals[0][0] = 0.1f;
  cta.srgb_matrix.vals[1][1] = 0.2f;
  cta.srgb_matrix.vals[2][2] = 0.3f;
  fake_drm_->plane_manager()->SetColorTemperatureAdjustment(
      fake_drm_->crtc_property(0).id, cta);

  {
    constexpr float kEpsilon = 0.001f;
    float rgb[3] = {0.4f, 0.5f, 0.6f};
    ApplyCrtcColorSpaceConversion(fake_drm_.get(), crtc_id, rgb);
    EXPECT_NEAR(rgb[0], 0.1f * 0.4f, kEpsilon);
    EXPECT_NEAR(rgb[1], 0.2f * 0.5f, kEpsilon);
    EXPECT_NEAR(rgb[2], 0.3f * 0.6f, kEpsilon);
  }
}

// The effect of gamma adjustment on the CTM.
TEST_P(HardwareDisplayPlaneManagerTest, CtmColorManagement_GammaAdjustment) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({display::features::kCtmColorManagement},
                                       {});

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);

  uint32_t crtc_id = fake_drm_->crtc_property(0).id;

  // This test has full CTM, DEGAMMA, and GAMMA.
  fake_drm_->AddProperty(crtc_id, {.id = kCtmPropId, .value = 0});
  fake_drm_->AddProperty(crtc_id, {.id = kDegammaLutSizePropId, .value = 33});
  fake_drm_->AddProperty(crtc_id, {.id = kDegammaLutPropId, .value = 0});
  fake_drm_->AddProperty(crtc_id, {.id = kGammaLutSizePropId, .value = 33});
  fake_drm_->AddProperty(crtc_id, {.id = kGammaLutPropId, .value = 0});

  // Color profile change will set all properties.
  fake_drm_->InitializeState(use_atomic_);

  // Apply gamma adjustment.
  display::GammaAdjustment gamma_adjustment;
  gamma_adjustment.curve = display::GammaCurve::MakeScale(0.9, 0.8, 0.7);
  fake_drm_->plane_manager()->SetGammaAdjustment(crtc_id, gamma_adjustment);

  {
    constexpr float kEpsilon = 0.001f;
    float rgb[3] = {0.6f, 0.5f, 0.4f};
    ApplyCrtcColorSpaceConversion(fake_drm_.get(), crtc_id, rgb);
    EXPECT_NEAR(rgb[0], 0.9f * 0.6f, kEpsilon);
    EXPECT_NEAR(rgb[1], 0.8f * 0.5f, kEpsilon);
    EXPECT_NEAR(rgb[2], 0.7f * 0.4f, kEpsilon);
  }
}

// The effect of color conversion (from input plane space to output space) on
// the CTM.
TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       CtmColorManagement_ColorConversion) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({display::features::kCtmColorManagement},
                                       {});

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);

  uint32_t crtc_id = fake_drm_->crtc_property(0).id;

  // This test has full CTM, DEGAMMA, and GAMMA.
  fake_drm_->AddProperty(crtc_id, {.id = kCtmPropId, .value = 0});
  fake_drm_->AddProperty(crtc_id, {.id = kDegammaLutSizePropId, .value = 33});
  fake_drm_->AddProperty(crtc_id, {.id = kDegammaLutPropId, .value = 0});
  fake_drm_->AddProperty(crtc_id, {.id = kGammaLutSizePropId, .value = 33});
  fake_drm_->AddProperty(crtc_id, {.id = kGammaLutPropId, .value = 0});

  // Color profile change will set all properties.
  fake_drm_->InitializeState(use_atomic_);

  fake_drm_->plane_manager()->SetOutputColorSpace(crtc_id,
                                                  SkNamedPrimariesExt::kP3);

  // We should not have committed the CTM yet.
  EXPECT_EQ(0, fake_drm_->get_commit_count());
  EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_id, "CTM"));

  // Commit a plane that is sRGB. Colors should be converted.
  {
    HardwareDisplayPlaneList state;
    auto buffer = CreateBuffer(kDefaultBufferSize);
    DrmOverlayPlaneList planes;
    planes.push_back(DrmOverlayPlane::TestPlane(buffer));
    planes[0].color_space = gfx::ColorSpace::CreateSRGB();

    PerformPageFlip(/*crtc_idx=*/0, &state, planes);
  }

  // This is the conversion of color(--display-p3-linear 0.25 0.5 0.75) to
  // srgb-linear using https://colorjs.io/apps/convert/.
  {
    constexpr float kEpsilon = 0.001f;
    float rgb[3] = {0.1937649f, 0.51051424f, 0.77947779f};
    ApplyCrtcColorSpaceConversion(fake_drm_.get(), crtc_id, rgb);
    EXPECT_NEAR(rgb[0], 0.25f, kEpsilon);
    EXPECT_NEAR(rgb[1], 0.5f, kEpsilon);
    EXPECT_NEAR(rgb[2], 0.75f, kEpsilon);
  }
}

// The combined effects of color conversion, color temperature adjustment, and
// gamma adjustment, on the CTM.
TEST_P(HardwareDisplayPlaneManagerAtomicTest, CtmColorManagement_Combined) {
  constexpr float kEpsilon = 0.001f;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({display::features::kCtmColorManagement},
                                       {});

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);

  uint32_t crtc_id = fake_drm_->crtc_property(0).id;

  // This test has full CTM, DEGAMMA, and GAMMA.
  fake_drm_->AddProperty(crtc_id, {.id = kCtmPropId, .value = 0});
  fake_drm_->AddProperty(crtc_id, {.id = kDegammaLutSizePropId, .value = 33});
  fake_drm_->AddProperty(crtc_id, {.id = kDegammaLutPropId, .value = 0});
  fake_drm_->AddProperty(crtc_id, {.id = kGammaLutSizePropId, .value = 33});
  fake_drm_->AddProperty(crtc_id, {.id = kGammaLutPropId, .value = 0});

  // Color profile change will set all properties.
  fake_drm_->InitializeState(use_atomic_);

  fake_drm_->plane_manager()->SetOutputColorSpace(crtc_id,
                                                  SkNamedPrimariesExt::kP3);

  // We should not have committed the CTM yet.
  EXPECT_EQ(0, fake_drm_->get_commit_count());
  EXPECT_EQ(0u, GetCrtcPropertyValue(crtc_id, "CTM"));
  HardwareDisplayPlaneList state;

  auto buffer = CreateBuffer(kDefaultBufferSize);

  // Constants for a test color value in P3 and sRGB. This is the conversion of
  // color(--display-p3-linear 0.25 0.5 0.75) to srgb-linear using
  // https://colorjs.io/apps/convert/.
  const float kColorP3[3] = {0.25, 0.5, 0.75};
  const float kColorSRGB[3] = {0.1937649f, 0.51051424f, 0.77947779f};

  // Commit a plane that is P3. Color conversion should be a no-op.
  {
    DrmOverlayPlaneList planes;
    planes.push_back(DrmOverlayPlane::TestPlane(buffer));
    planes[0].color_space = gfx::ColorSpace::CreateDisplayP3D65();

    PerformPageFlip(/*crtc_idx=*/0, &state, planes);
    EXPECT_EQ(1, fake_drm_->get_commit_count());
    EXPECT_NE(0u, GetCrtcPropertyValue(crtc_id, "CTM"));

    float rgb[3] = {kColorP3[0], kColorP3[1], kColorP3[2]};
    ApplyCrtcColorSpaceConversion(fake_drm_.get(), crtc_id, rgb);
    EXPECT_NEAR(rgb[0], kColorP3[0], kEpsilon);
    EXPECT_NEAR(rgb[1], kColorP3[1], kEpsilon);
    EXPECT_NEAR(rgb[2], kColorP3[2], kEpsilon);
  }

  // Commit a plane that is sRGB. Colors should be converted.
  {
    DrmOverlayPlaneList planes;
    planes.push_back(DrmOverlayPlane::TestPlane(buffer));
    planes[0].color_space = gfx::ColorSpace::CreateSRGB();

    PerformPageFlip(/*crtc_idx=*/0, &state, planes);
    EXPECT_NE(0u, GetCrtcPropertyValue(crtc_id, "CTM"));

    // This is the conversion of color(--display-p3-linear 0.25 0.5 0.75) to
    // srgb-linear using https://colorjs.io/apps/convert/.
    float rgb[3] = {kColorSRGB[0], kColorSRGB[1], kColorSRGB[2]};
    ApplyCrtcColorSpaceConversion(fake_drm_.get(), crtc_id, rgb);
    EXPECT_NEAR(rgb[0], kColorP3[0], kEpsilon);
    EXPECT_NEAR(rgb[1], kColorP3[1], kEpsilon);
    EXPECT_NEAR(rgb[2], kColorP3[2], kEpsilon);
  }

  // Apply color temperature adjustment. The CTM should be updated
  // immediately.
  {
    display::ColorTemperatureAdjustment cta;
    cta.srgb_matrix.vals[0][0] = 0.5;
    cta.srgb_matrix.vals[1][1] = 1.0;
    cta.srgb_matrix.vals[2][2] = 1.0;
    fake_drm_->plane_manager()->SetColorTemperatureAdjustment(
        fake_drm_->crtc_property(0).id, cta);

    float rgb[3] = {2.f * kColorSRGB[0], kColorSRGB[1], kColorSRGB[2]};
    ApplyCrtcColorSpaceConversion(fake_drm_.get(), crtc_id, rgb);
    EXPECT_NEAR(rgb[0], kColorP3[0], kEpsilon);
    EXPECT_NEAR(rgb[1], kColorP3[1], kEpsilon);
    EXPECT_NEAR(rgb[2], kColorP3[2], kEpsilon);
  }

  // Change the output color space. Nothing should change yet.
  {
    fake_drm_->plane_manager()->SetOutputColorSpace(crtc_id,
                                                    SkNamedPrimariesExt::kSRGB);

    float rgb[3] = {2.f * kColorSRGB[0], kColorSRGB[1], kColorSRGB[2]};
    ApplyCrtcColorSpaceConversion(fake_drm_.get(), crtc_id, rgb);
    EXPECT_NEAR(rgb[0], kColorP3[0], kEpsilon);
    EXPECT_NEAR(rgb[1], kColorP3[1], kEpsilon);
    EXPECT_NEAR(rgb[2], kColorP3[2], kEpsilon);
  }

  // Commit the plane with the same color space as the last flip. The CTM
  // should change now.
  {
    DrmOverlayPlaneList planes;
    planes.push_back(
        DrmOverlayPlane::TestPlane(CreateBuffer(kDefaultBufferSize)));
    planes[0].color_space = gfx::ColorSpace::CreateSRGB();
    planes[0].z_order = 1;

    PerformPageFlip(/*crtc_idx=*/0, &state, planes);
    EXPECT_NE(0u, GetCrtcPropertyValue(crtc_id, "CTM"));

    float rgb[3] = {1.0f, 0.75f, 0.25f};
    ApplyCrtcColorSpaceConversion(fake_drm_.get(), crtc_id, rgb);
    EXPECT_NEAR(rgb[0], 0.5f, kEpsilon);
    EXPECT_NEAR(rgb[1], 0.75f, kEpsilon);
    EXPECT_NEAR(rgb[2], 0.25f, kEpsilon);
  }

  // Apply gamma adjustment.
  {
    display::GammaAdjustment gamma_adjustment;
    gamma_adjustment.curve =
        display::GammaCurve::MakeScale(0.5, 0.25 / 0.75, 0.1 / 0.25);
    fake_drm_->plane_manager()->SetGammaAdjustment(crtc_id, gamma_adjustment);

    float rgb[3] = {1.0f, 0.75f, 0.25f};
    ApplyCrtcColorSpaceConversion(fake_drm_.get(), crtc_id, rgb);
    EXPECT_NEAR(rgb[0], 0.25f, kEpsilon);
    EXPECT_NEAR(rgb[1], 0.25f, kEpsilon);
    EXPECT_NEAR(rgb[2], 0.1f, kEpsilon);
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, ColorManagement_GammaAdjustment) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);

  // This test has full CTM, DEGAMMA, and GAMMA.
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kCtmPropId, .value = 0});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kDegammaLutSizePropId, .value = 1});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kDegammaLutPropId, .value = 0});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kGammaLutSizePropId, .value = 1});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kGammaLutPropId, .value = 0});

  // Gamma adjustment will set all properties.
  fake_drm_->InitializeState(use_atomic_);
  display::GammaAdjustment gamma_adjustment;
  gamma_adjustment.curve = display::GammaCurve::MakeGamma(1.1);
  fake_drm_->plane_manager()->SetGammaAdjustment(fake_drm_->crtc_property(0).id,
                                                 gamma_adjustment);

  if (use_atomic_) {
    // The gamma adjustment will get its own commit.
    EXPECT_EQ(1, fake_drm_->get_commit_count());
    EXPECT_NE(
        0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "GAMMA_LUT"));

    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(2, fake_drm_->get_commit_count());
    EXPECT_NE(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "CTM"));
    EXPECT_NE(
        0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "GAMMA_LUT"));
    EXPECT_EQ(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id,
                                       "DEGAMMA_LUT"));
  } else {
    EXPECT_EQ(3, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, ColorManagement_LegacyGamma) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);

  // This test is missing GAMMA.
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kCtmPropId, .value = 0});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kDegammaLutSizePropId, .value = 1});
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kDegammaLutPropId, .value = 0});

  // Gamma adjustment should call the legacy method.
  fake_drm_->InitializeState(use_atomic_);
  display::GammaAdjustment gamma_adjustment;
  gamma_adjustment.curve = display::GammaCurve::MakeGamma(1.1);
  fake_drm_->plane_manager()->SetGammaAdjustment(fake_drm_->crtc_property(0).id,
                                                 gamma_adjustment);

  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(2, fake_drm_->get_commit_count());
    EXPECT_NE(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "CTM"));

    // If we have DEGAMMA but no GAMMA, we still ignore the DEGAMMA.
    EXPECT_EQ(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id,
                                       "DEGAMMA_LUT"));
  } else {
    EXPECT_EQ(1, fake_drm_->get_set_object_property_count());
  }
  EXPECT_EQ(1, fake_drm_->get_set_gamma_ramp_count());
}

TEST_P(HardwareDisplayPlaneManagerTest, SetBackgroundColor_Success) {
  {
    fake_drm_->ResetStateWithDefaultObjects(
        /*crtc_count=*/1, /*planes_per_crtc=*/1);
    fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                           {.id = kBackgroundColorPropId, .value = 0});
  }
  fake_drm_->InitializeState(use_atomic_);
  fake_drm_->plane_manager()->SetBackgroundColor(fake_drm_->crtc_property(0).id,
                                                 0);
  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(1, fake_drm_->get_commit_count());
    EXPECT_EQ(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id,
                                       "BACKGROUND_COLOR"));
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }

  {
    fake_drm_->ResetStateWithDefaultObjects(
        /*crtc_count=*/1, /*planes_per_crtc=*/1);
    fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                           {.id = kBackgroundColorPropId, .value = 1});
  }
  fake_drm_->InitializeState(use_atomic_);
  fake_drm_->plane_manager()->SetBackgroundColor(fake_drm_->crtc_property(0).id,
                                                 1);
  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(2, fake_drm_->get_commit_count());
    EXPECT_EQ(1u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id,
                                       "BACKGROUND_COLOR"));
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       CommitReturnsNullOutFenceIfOutFencePtrNotSupported) {
  scoped_refptr<DrmFramebuffer> fake_buffer2 = CreateBuffer(kDefaultBufferSize);

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(use_atomic_);

  DrmOverlayPlaneList assigns1;
  assigns1.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  DrmOverlayPlaneList assigns2;
  assigns2.push_back(DrmOverlayPlane::TestPlane(fake_buffer2));

  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns1, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns2, fake_drm_->crtc_property(1).id));

  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());

  gfx::GpuFenceHandle release_fence;
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                 &release_fence));
  EXPECT_TRUE(release_fence.is_null());
}

TEST_P(HardwareDisplayPlaneManagerTest,
       InitializationFailsIfSupportForOutFencePropertiesIsPartial) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/3, /*planes_per_crtc=*/1);
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kOutFencePtrPropId, .value = 1});
  fake_drm_->AddProperty(fake_drm_->crtc_property(2).id,
                         {.id = kOutFencePtrPropId, .value = 2});

  EXPECT_FALSE(fake_drm_->InitializeStateWithResult(use_atomic_));
}

TEST_P(HardwareDisplayPlaneManagerTest,
       InitializationSucceedsIfSupportForOutFencePropertiesIsComplete) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/3, /*planes_per_crtc=*/1);
  fake_drm_->AddProperty(fake_drm_->crtc_property(0).id,
                         {.id = kOutFencePtrPropId, .value = 1});
  fake_drm_->AddProperty(fake_drm_->crtc_property(1).id,
                         {.id = kOutFencePtrPropId, .value = 2});
  fake_drm_->AddProperty(fake_drm_->crtc_property(2).id,
                         {.id = kOutFencePtrPropId, .value = 3});

  EXPECT_TRUE(fake_drm_->InitializeStateWithResult(use_atomic_));
}

// Verifies that formats with 2 bits of alpha decay to opaques for AddFB2().
TEST_P(HardwareDisplayPlaneManagerTest, ForceOpaqueFormatsForAddFramebuffer) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/3, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(use_atomic_);

  struct {
    uint32_t input_fourcc;  // FourCC presented to AddFramebuffer.
    uint32_t used_fourcc;   // FourCC expected to be used in AddFramebuffer.
  } kFourCCFormats[] = {
      {DRM_FORMAT_ABGR2101010, DRM_FORMAT_XBGR2101010},
      {DRM_FORMAT_ARGB2101010, DRM_FORMAT_XRGB2101010},
  };

  for (const auto& format_pair : kFourCCFormats) {
    scoped_refptr<DrmFramebuffer> drm_fb =
        CreateBufferWithFormat(kDefaultBufferSize, format_pair.input_fourcc);

    EXPECT_EQ(drm_fb->framebuffer_pixel_format(), format_pair.used_fourcc);
    EXPECT_EQ(drm_fb->opaque_framebuffer_pixel_format(),
              format_pair.used_fourcc);
  }

  // If DRM supports high-bitdepth formats with Alpha, there's no need for
  // opaque decaying. Note that we have to support all |kFourCCFormats|.
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/3, /*planes_per_crtc=*/1, /*movable_planes=*/0,
      {DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010}, {});

  fake_drm_->InitializeState(use_atomic_);

  for (const auto& format_pair : kFourCCFormats) {
    scoped_refptr<DrmFramebuffer> drm_fb =
        CreateBufferWithFormat(kDefaultBufferSize, format_pair.input_fourcc);

    EXPECT_EQ(drm_fb->framebuffer_pixel_format(), format_pair.input_fourcc);
    EXPECT_EQ(drm_fb->opaque_framebuffer_pixel_format(),
              format_pair.used_fourcc);
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, GetHardwareCapabilities) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/4, /*planes_per_crtc=*/7);
  fake_drm_->InitializeState(use_atomic_);

  for (int i = 0; i < 4; ++i) {
    auto hc =
        fake_drm_->plane_manager()->GetHardwareCapabilities(kCrtcIdBase + i);
    EXPECT_TRUE(hc.is_valid);
    // Legacy doesn't support OVERLAY planes.
    int expected_planes = use_atomic_ ? 7 : 1;
    EXPECT_EQ(hc.num_overlay_capable_planes, expected_planes);
  }

  {
    auto& drm_state = fake_drm_->ResetStateWithDefaultObjects(
        /*crtc_count=*/4, /*planes_per_crtc=*/7);
    // Change the last (CURSOR) plane into a PRIMARY plane that is available to
    // only the first two CRTCs.
    auto& last_props =
        drm_state.plane_properties[drm_state.plane_properties.size() - 1];
    last_props.crtc_mask = (1 << 0) | (1 << 1);
    // Find the type property and change it to PRIMARY.
    for (auto& property : last_props.properties) {
      if (property.id == kTypePropId) {
        property.value = DRM_PLANE_TYPE_PRIMARY;
        break;
      }
    }

    fake_drm_->InitializeState(use_atomic_);
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
    fake_drm_->SetDriverName(std::nullopt);
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
  handle.Adopt(base::ScopedFD(HANDLE_EINTR(dup(read_fd.get()))));
  return std::make_unique<gfx::GpuFence>(std::move(handle));
}

void FakeFenceFD::Signal() const {
  base::WriteFileDescriptor(write_fd.get(), "a");
}

class HardwareDisplayPlaneManagerPlanesReadyTest : public testing::Test {
 public:
  HardwareDisplayPlaneManagerPlanesReadyTest(
      const HardwareDisplayPlaneManagerPlanesReadyTest&) = delete;
  HardwareDisplayPlaneManagerPlanesReadyTest& operator=(
      const HardwareDisplayPlaneManagerPlanesReadyTest&) = delete;

 protected:
  HardwareDisplayPlaneManagerPlanesReadyTest() = default;

  void SetUp() override {
    auto gbm_device = std::make_unique<MockGbmDevice>();
    fake_drm_ = new FakeDrmDevice(std::move(gbm_device));
    drm_framebuffer_ = CreateBuffer(kDefaultBufferSize);
    planes_without_fences_ = CreatePlanesWithoutFences();
    planes_with_fences_ = CreatePlanesWithFences();
  }

  void UseLegacyManager();
  void UseAtomicManager();
  void RequestPlanesReady(DrmOverlayPlaneList planes);

  scoped_refptr<DrmFramebuffer> CreateBuffer(const gfx::Size& size) {
    std::unique_ptr<GbmBuffer> buffer = fake_drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_XRGB8888, size, GBM_BO_USE_SCANOUT);
    return DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(), size);
  }

  DrmOverlayPlaneList CreatePlanesWithoutFences() {
    DrmOverlayPlaneList planes;
    planes.push_back(
        DrmOverlayPlane::TestPlane(CreateBuffer(kDefaultBufferSize)));
    planes.push_back(
        DrmOverlayPlane::TestPlane(CreateBuffer(kDefaultBufferSize)));
    return planes;
  }

  DrmOverlayPlaneList CreatePlanesWithFences() {
    DrmOverlayPlaneList planes;
    planes.push_back(DrmOverlayPlane::TestPlane(
        CreateBuffer(kDefaultBufferSize), gfx::ColorSpace::CreateSRGB(),
        fake_fence_fd1_.GetGpuFence()));
    planes.push_back(DrmOverlayPlane::TestPlane(
        CreateBuffer(kDefaultBufferSize), gfx::ColorSpace::CreateSRGB(),
        fake_fence_fd2_.GetGpuFence()));
    return planes;
  }

  scoped_refptr<FakeDrmDevice> fake_drm_;
  std::unique_ptr<HardwareDisplayPlaneManager> plane_manager_;
  bool callback_called = false;
  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  scoped_refptr<DrmFramebuffer> drm_framebuffer_;
  const FakeFenceFD fake_fence_fd1_;
  const FakeFenceFD fake_fence_fd2_;

  DrmOverlayPlaneList planes_without_fences_;
  DrmOverlayPlaneList planes_with_fences_;
};

void HardwareDisplayPlaneManagerPlanesReadyTest::RequestPlanesReady(
    DrmOverlayPlaneList planes) {
  auto set_true = [](bool* b, DrmOverlayPlaneList planes) { *b = true; };
  plane_manager_->RequestPlanesReadyCallback(
      std::move(planes), base::BindOnce(set_true, &callback_called));
}

void HardwareDisplayPlaneManagerPlanesReadyTest::UseLegacyManager() {
  plane_manager_ =
      std::make_unique<HardwareDisplayPlaneManagerLegacy>(fake_drm_.get());
}

void HardwareDisplayPlaneManagerPlanesReadyTest::UseAtomicManager() {
  plane_manager_ =
      std::make_unique<HardwareDisplayPlaneManagerAtomic>(fake_drm_.get());
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       LegacyWithoutFencesIsAsynchronousWithoutFenceWait) {
  UseLegacyManager();
  RequestPlanesReady(DrmOverlayPlane::Clone(planes_without_fences_));

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       LegacyWithFencesIsAsynchronousWithFenceWait) {
  UseLegacyManager();
  RequestPlanesReady(DrmOverlayPlane::Clone(planes_with_fences_));

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
  RequestPlanesReady(DrmOverlayPlane::Clone(planes_without_fences_));

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_F(HardwareDisplayPlaneManagerPlanesReadyTest,
       AtomicWithFencesIsAsynchronousWithoutFenceWait) {
  UseAtomicManager();
  RequestPlanesReady(DrmOverlayPlane::Clone(planes_with_fences_));

  EXPECT_FALSE(callback_called);

  task_env_.RunUntilIdle();

  EXPECT_TRUE(callback_called);
}

TEST_P(HardwareDisplayPlaneManagerTest, GetPossibleCrtcsBitmaskForConnector) {
  fake_drm_->ResetStateWithAllProperties();
  fake_drm_->AddCrtc();
  fake_drm_->AddCrtc();
  fake_drm_->AddCrtc();

  const uint32_t connector_1_id = AddConnector(/*possible_crtcs=*/0b101u);
  const uint32_t connector_2_id = AddConnector(/*possible_crtcs=*/0b110u);
  const uint32_t connector_3_id = AddConnector(/*possible_crtcs=*/0b011u);

  fake_drm_->InitializeState(use_atomic_);

  fake_drm_->plane_manager()->ResetConnectorsCacheAndGetValidIds(
      fake_drm_->GetResources());

  EXPECT_EQ(fake_drm_->plane_manager()->GetPossibleCrtcsBitmaskForConnector(
                connector_1_id),
            0b101u);
  EXPECT_EQ(fake_drm_->plane_manager()->GetPossibleCrtcsBitmaskForConnector(
                connector_2_id),
            0b110u);
  EXPECT_EQ(fake_drm_->plane_manager()->GetPossibleCrtcsBitmaskForConnector(
                connector_3_id),
            0b011u);
}

TEST_P(HardwareDisplayPlaneManagerTest,
       GetPossibleCrtcsBitmaskForConnectorInvalidConnector) {
  fake_drm_->ResetStateWithAllProperties();
  fake_drm_->AddCrtc();
  FakeDrmDevice::EncoderProperties& encoder = fake_drm_->AddEncoder();
  encoder.possible_crtcs = 0b1;
  const uint32_t encoder_id = encoder.id;

  FakeDrmDevice::ConnectorProperties& connector = fake_drm_->AddConnector();
  connector.connection = true;
  connector.encoders = std::vector<uint32_t>{encoder_id};
  const uint32_t connector_id = connector.id;

  fake_drm_->InitializeState(use_atomic_);

  fake_drm_->plane_manager()->ResetConnectorsCacheAndGetValidIds(
      fake_drm_->GetResources());

  EXPECT_EQ(fake_drm_->plane_manager()->GetPossibleCrtcsBitmaskForConnector(
                connector_id + 1),
            0u);
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, OriginalModifiersSupportOnly) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1, /*movable_planes=*/0,
      {DRM_FORMAT_NV12}, {});
  fake_drm_->InitializeState(use_atomic_);

  {
    DrmOverlayPlaneList assigns;
    // Create as NV12 since this is required for rotation support.
    std::unique_ptr<GbmBuffer> buffer = fake_drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_NV12, kDefaultBufferSize, GBM_BO_USE_SCANOUT);
    scoped_refptr<DrmFramebuffer> framebuffer_original =
        DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(),
                                       kDefaultBufferSize, {}, true);
    assigns.push_back(DrmOverlayPlane::TestPlane(framebuffer_original));
    assigns.back().plane_transform =
        gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270;

    fake_drm_->plane_manager()->BeginFrame(&state_);
    // Rotation should be supported for this buffer as it is the original buffer
    // with the original modifiers.
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, fake_drm_->crtc_property(0).id));

    gfx::GpuFenceHandle release_fence;
    scoped_refptr<PageFlipRequest> page_flip_request =
        base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                   &release_fence));
  }

  {
    DrmOverlayPlaneList assigns;
    assigns.clear();
    fake_drm_->plane_manager()->BeginFrame(&state_);
    // The test buffer would not have accurate modifiers and therefore should
    // fail rotation.
    std::unique_ptr<GbmBuffer> buffer = fake_drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_NV12, kDefaultBufferSize, GBM_BO_USE_SCANOUT);
    scoped_refptr<DrmFramebuffer> framebuffer_non_original =
        DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(),
                                       kDefaultBufferSize, {}, false);
    assigns.push_back(DrmOverlayPlane::TestPlane(framebuffer_non_original));
    assigns.back().plane_transform =
        gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270;
    EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, fake_drm_->crtc_property(0).id));
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, OverlaySourceCrop) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(use_atomic_);

  {
    DrmOverlayPlaneList assigns;
    assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

    fake_drm_->plane_manager()->BeginFrame(&state_);
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, fake_drm_->crtc_property(0).id));

    gfx::GpuFenceHandle release_fence;
    scoped_refptr<PageFlipRequest> page_flip_request =
        base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                   &release_fence));

    EXPECT_EQ(2u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_W"));
    EXPECT_EQ(2u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_H"));
  }

  {
    DrmOverlayPlaneList assigns;
    assigns.emplace_back(fake_buffer_, gfx::ColorSpace::CreateSRGB(), 0,
                         gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
                         gfx::Rect(), gfx::Rect(kDefaultBufferSize),
                         gfx::RectF(0, 0, .5, 1), false, nullptr);

    fake_drm_->plane_manager()->BeginFrame(&state_);
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, fake_drm_->crtc_property(0).id));

    scoped_refptr<PageFlipRequest> page_flip_request =
        base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
    gfx::GpuFenceHandle release_fence;
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                   &release_fence));

    EXPECT_EQ(1u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_W"));
    EXPECT_EQ(2u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_H"));
  }

  {
    DrmOverlayPlaneList assigns;
    assigns.emplace_back(fake_buffer_, gfx::ColorSpace::CreateSRGB(), 0,
                         gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE,
                         gfx::Rect(), gfx::Rect(kDefaultBufferSize),
                         gfx::RectF(0, 0, .999, .501), false, nullptr);

    fake_drm_->plane_manager()->BeginFrame(&state_);
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, fake_drm_->crtc_property(0).id));

    scoped_refptr<PageFlipRequest> page_flip_request =
        base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
    gfx::GpuFenceHandle release_fence;
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                   &release_fence));

    EXPECT_EQ(2u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_W"));
    EXPECT_EQ(1u << 16, GetPlanePropertyValue(kPlaneOffset, "SRC_H"));
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, ColorEncodingAndRange) {
  // These values are chosen arbitrarily % avoiding 0 in order to test that the
  // classes under test don't assume that a value of 0 is special,
  constexpr uint64_t kColorEncodingBT601 = 2u;
  constexpr uint64_t kColorEncodingBT709 = 3u;
  constexpr uint64_t kColorRangeLimited = 4u;
  constexpr uint64_t kColorRangeFull = 5u;

  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1, /*movable_planes=*/0,
      /*plane_supported_formats=*/{DRM_FORMAT_NV12});

  fake_drm_->SetPossibleValuesForEnumProperty(
      /*property_id=*/kColorEncodingPropId, /*values=*/{
          {kColorEncodingBT601, "ITU-R BT.601 YCbCr"},
          {kColorEncodingBT709, "ITU-R BT.709 YCbCr"},
      });
  fake_drm_->AddProperty(
      /*object_id=*/fake_drm_->plane_property(0).id,
      {.id = kColorEncodingPropId, .value = kColorEncodingBT601});

  fake_drm_->SetPossibleValuesForEnumProperty(
      /*property_id=*/kColorRangePropId, /*values=*/{
          {kColorRangeLimited, "YCbCr limited range"},
          {kColorRangeFull, "YCbCr full range"},
      });
  fake_drm_->AddProperty(/*object_id=*/fake_drm_->plane_property(0).id,
                         {.id = kColorRangePropId, .value = kColorRangeFull});

  fake_drm_->InitializeState(use_atomic_);

  // TODO: bug 233667677 - Notice that the `expected_color_range` is always
  // limited regardless of the `color_space`. That's because we're currently not
  // confident about having widespread support for full range, so the
  // HardwareDisplayPlaneAtomic should always set the range to limited.
  // Eventually, we'll probably want to plumb the right range.
  struct TestCase {
    gfx::ColorSpace color_space;
    uint64_t expected_color_encoding;
    uint64_t expected_color_range;
  };
  std::vector<TestCase> test_cases = {
      {
          .color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                         gfx::ColorSpace::TransferID::BT709,
                                         gfx::ColorSpace::MatrixID::BT709,
                                         gfx::ColorSpace::RangeID::LIMITED),
          .expected_color_encoding = kColorEncodingBT709,
          .expected_color_range = kColorRangeLimited,
      },
      {
          .color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                                         gfx::ColorSpace::TransferID::BT709,
                                         gfx::ColorSpace::MatrixID::BT709,
                                         gfx::ColorSpace::RangeID::FULL),
          .expected_color_encoding = kColorEncodingBT709,
          .expected_color_range = kColorRangeLimited,
      },
      {
          .color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                                         gfx::ColorSpace::TransferID::SMPTE170M,
                                         gfx::ColorSpace::MatrixID::SMPTE170M,
                                         gfx::ColorSpace::RangeID::LIMITED),
          .expected_color_encoding = kColorEncodingBT601,
          .expected_color_range = kColorRangeLimited,
      },
      {
          .color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                                         gfx::ColorSpace::TransferID::SMPTE170M,
                                         gfx::ColorSpace::MatrixID::SMPTE170M,
                                         gfx::ColorSpace::RangeID::FULL),
          .expected_color_encoding = kColorEncodingBT601,
          .expected_color_range = kColorRangeLimited,
      },
  };

  for (const auto& test_case : test_cases) {
    DrmOverlayPlaneList assigns;
    std::unique_ptr<GbmBuffer> buffer = fake_drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_NV12, kDefaultBufferSize, GBM_BO_USE_SCANOUT);
    scoped_refptr<DrmFramebuffer> framebuffer_original =
        DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(),
                                       kDefaultBufferSize, {}, true);
    assigns.push_back(DrmOverlayPlane::TestPlane(framebuffer_original));
    assigns.back().color_space = test_case.color_space;

    fake_drm_->plane_manager()->BeginFrame(&state_);
    EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, fake_drm_->crtc_property(0).id));

    scoped_refptr<PageFlipRequest> page_flip_request =
        base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
    gfx::GpuFenceHandle release_fence;
    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(&state_, page_flip_request,
                                                   &release_fence));

    EXPECT_EQ(test_case.expected_color_encoding,
              GetPlanePropertyValue(kPlaneOffset, "COLOR_ENCODING"));
    EXPECT_EQ(test_case.expected_color_range,
              GetPlanePropertyValue(kPlaneOffset, "COLOR_RANGE"));
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, OldPlaneInAnotherList) {
  fake_drm_->ResetStateWithDefaultObjects(/*connector_and_crtc_count=*/2,
                                          /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/true);

  const uint32_t crtc_0_id = fake_drm_->crtc_property(0).id;
  const uint32_t crtc_1_id = fake_drm_->crtc_property(1).id;

  CommitRequest commit_request;
  HardwareDisplayPlaneList plane_list_0;
  HardwareDisplayPlaneList plane_list_1;
  const auto& planes = fake_drm_->plane_manager()->planes();
  ASSERT_THAT(planes, testing::SizeIs(4));
  HardwareDisplayPlane *plane_0, *plane_1;
  for (auto& plane : planes) {
    // Skip non-primary planes.
    if ((plane->type() & DRM_PLANE_TYPE_PRIMARY) == 0) {
      continue;
    }

    // Primary planes created by FakeDrmDevice::ResetStateWithDefaultObjects()
    // should only be compatible with one CRTC.
    if (plane->CanUseForCrtcId(crtc_0_id)) {
      plane_0 = plane.get();
    } else if (plane->CanUseForCrtcId(crtc_1_id)) {
      plane_1 = plane.get();
    }
  }

  ASSERT_NE(plane_0, nullptr);
  ASSERT_NE(plane_1, nullptr);
  ASSERT_NE(plane_0, plane_1);

  plane_list_0.plane_list.push_back(plane_0);
  plane_list_0.old_plane_list.push_back(plane_1);

  plane_list_1.plane_list.push_back(plane_1);
  plane_list_1.old_plane_list.push_back(plane_0);
  {
    DrmOverlayPlaneList overlays;
    overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

    CrtcCommitRequest request = CrtcCommitRequest::EnableCrtcRequest(
        crtc_0_id, fake_drm_->connector_property(0).id, kDefaultMode,
        gfx::Point(), &plane_list_0, std::move(overlays),
        /*enable_vrr=*/false);
    commit_request.push_back(std::move(request));
  }
  {
    DrmOverlayPlaneList overlays;
    overlays.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));

    CrtcCommitRequest request = CrtcCommitRequest::EnableCrtcRequest(
        crtc_1_id, fake_drm_->connector_property(1).id, kDefaultMode,
        gfx::Point(), &plane_list_1, std::move(overlays),
        /*enable_vrr=*/false);
    commit_request.push_back(std::move(request));
  }

  ASSERT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request),
      DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET));

  EXPECT_EQ(plane_0->owning_crtc(), crtc_0_id);
  EXPECT_EQ(plane_1->owning_crtc(), crtc_1_id);
}

class HardwareDisplayPlaneAtomicMock : public HardwareDisplayPlaneAtomic {
 public:
  HardwareDisplayPlaneAtomicMock() : HardwareDisplayPlaneAtomic(1) {}
  ~HardwareDisplayPlaneAtomicMock() override = default;

  bool AssignPlaneProps(DrmDevice* drm,
                        uint32_t crtc_id,
                        uint32_t framebuffer,
                        const gfx::Rect& crtc_rect,
                        const gfx::Rect& src_rect,
                        const gfx::Rect& damage_rect,
                        const gfx::OverlayTransform transform,
                        const gfx::ColorSpace& color_space,
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
  auto gbm_device = std::make_unique<MockGbmDevice>();
  auto drm_device = base::MakeRefCounted<FakeDrmDevice>(std::move(gbm_device));
  auto plane_manager =
      std::make_unique<HardwareDisplayPlaneManagerAtomic>(drm_device.get());
  HardwareDisplayPlaneList plane_list;
  HardwareDisplayPlaneAtomicMock hw_plane;
  std::unique_ptr<GbmBuffer> buffer = drm_device->gbm_device()->CreateBuffer(
      DRM_FORMAT_XRGB8888, kDefaultBufferSize, GBM_BO_USE_SCANOUT);
  scoped_refptr<DrmFramebuffer> framebuffer = DrmFramebuffer::AddFramebuffer(
      drm_device, buffer.get(), kDefaultBufferSize);
  DrmOverlayPlane overlay(DrmOverlayPlane::TestPlane(framebuffer));
  overlay.enable_blend = true;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, std::nullopt,
                              gfx::Rect());
  EXPECT_EQ(hw_plane.framebuffer(), framebuffer->framebuffer_id());

  overlay.enable_blend = false;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, std::nullopt,
                              gfx::Rect());
  EXPECT_EQ(hw_plane.framebuffer(), framebuffer->opaque_framebuffer_id());
}

class HardwareDisplayPlaneManagerSeamlessModeTest : public testing::Test {
 protected:
  void SetUp() override {
    drm_device_ = MockDrmDevice::Create();

    // Initialize FakeDrmDevice state to have a single configured display.
    drm_device_->ResetStateWithAllProperties();
    crtc_id_ = drm_device_->AddCrtcWithPrimaryAndCursorPlanes().id;

    auto& encoder = drm_device_->AddEncoder();
    encoder.possible_crtcs = 0b1;

    auto& connector = drm_device_->AddConnector();
    connector.connection = true;
    connector.modes = {
        ResolutionAndRefreshRate{gfx::Size(3840, 2160), 120u},
    };
    connector.encoders = std::vector<uint32_t>{encoder.id};

    drm_device_->InitializeState(/* use_atomic */ true);
    plane_manager_ =
        std::make_unique<HardwareDisplayPlaneManagerAtomic>(drm_device_.get());
    CHECK(plane_manager_->Initialize());
  }

  int64_t crtc_id_;
  scoped_refptr<MockDrmDevice> drm_device_;
  std::unique_ptr<HardwareDisplayPlaneManagerAtomic> plane_manager_;
};

TEST_F(HardwareDisplayPlaneManagerSeamlessModeTest, TestSeamlessMode) {
  // Any arbitrary mode to be tested for seamless configuration.
  drmModeModeInfo arbitrary_mode = {
      .hdisplay = 1234, .vdisplay = 567, .vrefresh = 19u};

  // CommitProperties is called with DRM_MODE_ATOMIC_TEST_ONLY. The result of
  // CommitProperties propagates to the result of TestSeamlessMode.

  // CommitProperties returns false.
  EXPECT_CALL(*drm_device_,
              CommitProperties(_, DRM_MODE_ATOMIC_TEST_ONLY, 1, _))
      .Times(1)
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(plane_manager_->TestSeamlessMode(crtc_id_, arbitrary_mode));

  // CommitProperties returns true.
  EXPECT_CALL(*drm_device_,
              CommitProperties(_, DRM_MODE_ATOMIC_TEST_ONLY, 1, _))
      .Times(1)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(plane_manager_->TestSeamlessMode(crtc_id_, arbitrary_mode));
}

TEST_F(HardwareDisplayPlaneManagerSeamlessModeTest,
       TestSeamlessMode_InvalidCrtcId) {
  // Any arbitrary mode to be tested for seamless configuration.
  drmModeModeInfo arbitrary_mode = {
      .hdisplay = 1234, .vdisplay = 567, .vrefresh = 19u};

  // Invalid crtc will result in a DCHECK.
  int32_t wrong_crtc_id = 9999;
  EXPECT_NE(wrong_crtc_id, crtc_id_);
  EXPECT_DCHECK_DEATH(
      plane_manager_->TestSeamlessMode(wrong_crtc_id, arbitrary_mode));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, CrtcOffsetPageFlip) {
  fake_drm_->ResetStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(/*use_atomic=*/true);

  DrmOverlayPlaneList assigns;
  assigns.push_back(DrmOverlayPlane::TestPlane(fake_buffer_));
  HardwareDisplayPlaneList hdpl;

  const gfx::Point crtc_offset = gfx::Point(-100, -200);
  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id, crtc_offset));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));

  // DrmWrapper::Property::value is a uint64_t, even though the crtc offset is
  // negative.
  EXPECT_EQ(GetPlanePropertyValue(kPlaneOffset, "CRTC_X"),
            static_cast<uint64_t>(-100));
  EXPECT_EQ(GetPlanePropertyValue(kPlaneOffset, "CRTC_Y"),
            static_cast<uint64_t>(-200));
}
}  // namespace ui
