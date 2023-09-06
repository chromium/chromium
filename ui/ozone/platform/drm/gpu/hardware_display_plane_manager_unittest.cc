// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/test/mock_gbm_device.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_atomic.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"
#include "ui/ozone/platform/drm/gpu/mock_drm_device.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"

namespace ui {

namespace {

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

  void SetUp() override;

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
  scoped_refptr<MockDrmDevice> fake_drm_;

  bool use_atomic_ = false;
};

void HardwareDisplayPlaneManagerTest::SetUp() {
  use_atomic_ = GetParam();

  auto gbm_device = std::make_unique<MockGbmDevice>();
  fake_drm_ = new MockDrmDevice(std::move(gbm_device));
  fake_drm_->SetPropertyBlob(MockDrmDevice::AllocateInFormatsBlob(
      kInFormatsBlobIdBase, {DRM_FORMAT_XRGB8888}, {}));

  fake_buffer_ = CreateBuffer(kDefaultBufferSize);
}

void HardwareDisplayPlaneManagerTest::PerformPageFlip(
    size_t crtc_idx,
    HardwareDisplayPlaneList* state) {
  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> xrgb_buffer = CreateBuffer(kDefaultBufferSize);
  assigns.emplace_back(xrgb_buffer, nullptr);
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

// TODO(crbug.com/1431767): Re-enable this test
#if defined(LEAK_SANITIZER)
#define MAYBE_ResettingConnectorCache DISABLED_ResettingConnectorCache
#else
#define MAYBE_ResettingConnectorCache ResettingConnectorCache
#endif
TEST_P(HardwareDisplayPlaneManagerTest, MAYBE_ResettingConnectorCache) {
  const int connector_and_crtc_count = 3;
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      connector_and_crtc_count,
      /*planes_per_crtc=*/1);

  drm_state.connector_properties.clear();
  // Create 3 connectors, kConnectorIdBase + 0/1/2
  for (size_t i = 0; i < connector_and_crtc_count; ++i) {
    auto& connector_properties = drm_state.connector_properties.emplace_back();
    connector_properties.id = kConnectorIdBase + i;
    connector_properties.properties.push_back(
        {.id = kCrtcIdPropId, .value = 0});
  }

  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);

  HardwareDisplayPlaneList state;

  {
    CommitRequest commit_request;
    fake_drm_->plane_manager()->BeginFrame(&state);
    // Check all 3 connectors exist
    for (size_t i = 0; i < connector_and_crtc_count; ++i) {
      DrmOverlayPlaneList overlays;
      overlays.emplace_back(fake_buffer_, nullptr);

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
  fake_drm_->UpdateStateBesidesPlaneManager(drm_state);
  fake_drm_->plane_manager()->ResetConnectorsCacheAndGetValidIds(
      fake_drm_->GetResources());

  {
    CommitRequest commit_request;
    fake_drm_->plane_manager()->BeginFrame(&state);
    {
      DrmOverlayPlaneList overlays;
      overlays.emplace_back(fake_buffer_, nullptr);
      commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
          fake_drm_->crtc_property(0).id, kConnectorIdBase, kDefaultMode,
          gfx::Point(), &state, std::move(overlays), /*enable_vrr=*/false));
    }
    {
      DrmOverlayPlaneList overlays;
      overlays.emplace_back(fake_buffer_, nullptr);
      commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
          fake_drm_->crtc_property(1).id, kConnectorIdBase + 1, kDefaultMode,
          gfx::Point(), &state, std::move(overlays), /*enable_vrr=*/false));
    }
    {
      DrmOverlayPlaneList overlays;
      overlays.emplace_back(fake_buffer_, nullptr);
      commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
          fake_drm_->crtc_property(2).id, kConnectorIdBase + 3, kDefaultMode,
          gfx::Point(), &state, std::move(overlays), /*enable_vrr=*/false));
    }

    EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
        std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SequenceIncrementOnModesetOnly) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithNoProperties();
  // Add some resources so HardwareDisplayPlaneManager can properly initialize
  // within |fake_drm_|.
  drm_state.AddCrtcAndConnector();
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);

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
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/false);

  fake_drm_->set_set_crtc_expectation(false);

  HardwareDisplayPlaneList state;
  DrmOverlayPlane plane(fake_buffer_, nullptr);
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
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/false);

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
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_EQ(1u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, AddCursor) {
  DrmOverlayPlaneList assigns;
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

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
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_FALSE(
      fake_drm_->plane_manager()->AssignOverlayPlanes(&state_, assigns, 0));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, NotEnoughPlanes) {
  DrmOverlayPlaneList assigns;
  assigns.emplace_back(fake_buffer_, nullptr);
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, MultipleCrtcs) {
  DrmOverlayPlaneList assigns;
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(1).id));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerLegacyTest, MultiplePlanesAndCrtcs) {
  DrmOverlayPlaneList assigns;
  assigns.emplace_back(fake_buffer_, nullptr);
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(drm_state, use_atomic_);

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
  assigns.emplace_back(buffer, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  fake_drm_->plane_manager()->BeginFrame(&state_);
  // This should return false as plane manager creates planes which support
  // DRM_FORMAT_XRGB8888 while buffer returns kDummyFormat as its pixelFormat.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  assigns.clear();
  scoped_refptr<DrmFramebuffer> xrgb_buffer = CreateBuffer(kDefaultBufferSize);
  assigns.emplace_back(xrgb_buffer, nullptr);
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  fake_drm_->plane_manager()->BeginFrame(&state_);
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
}

// TODO(crbug.com/1431767): Re-enable this test
#if defined(LEAK_SANITIZER)
#define MAYBE_Modeset DISABLED_Modeset
#else
#define MAYBE_Modeset Modeset
#endif
TEST_P(HardwareDisplayPlaneManagerAtomicTest, MAYBE_Modeset) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);

  HardwareDisplayPlaneList state;
  CommitRequest commit_request;
  DrmOverlayPlaneList overlays;
  overlays.emplace_back(fake_buffer_, nullptr);

  commit_request.push_back(CrtcCommitRequest::EnableCrtcRequest(
      fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
      kDefaultMode, gfx::Point(), &state, std::move(overlays),
      /*enable_vrr=*/false));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  EXPECT_EQ(1, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, DisableModeset) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);

  HardwareDisplayPlaneList state;
  CommitRequest commit_request;
  commit_request.push_back(CrtcCommitRequest::DisableCrtcRequest(
      fake_drm_->crtc_property(0).id, fake_drm_->connector_property(0).id,
      &state));
  EXPECT_TRUE(fake_drm_->plane_manager()->Commit(
      std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET));

  EXPECT_EQ(1, fake_drm_->get_commit_count());
}

// TODO(crbug.com/1431767): Re-enable this test
#if defined(LEAK_SANITIZER)
#define MAYBE_CheckPropsAfterModeset DISABLED_CheckPropsAfterModeset
#else
#define MAYBE_CheckPropsAfterModeset CheckPropsAfterModeset
#endif
TEST_P(HardwareDisplayPlaneManagerAtomicTest, MAYBE_CheckPropsAfterModeset) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);

  HardwareDisplayPlaneList state;
  CommitRequest commit_request;
  DrmOverlayPlaneList overlays;
  overlays.emplace_back(fake_buffer_, nullptr);
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

// TODO(crbug.com/1431767): Re-enable this test
#if defined(LEAK_SANITIZER)
#define MAYBE_CheckPropsAfterDisable DISABLED_CheckPropsAfterDisable
#else
#define MAYBE_CheckPropsAfterDisable CheckPropsAfterDisable
#endif
TEST_P(HardwareDisplayPlaneManagerAtomicTest, MAYBE_CheckPropsAfterDisable) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);

  HardwareDisplayPlaneList state;
  {
    CommitRequest commit_request;
    DrmOverlayPlaneList overlays;
    overlays.emplace_back(fake_buffer_, nullptr);
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

// TODO(crbug.com/1431767): Re-enable this test
#if defined(LEAK_SANITIZER)
#define MAYBE_CheckVrrAfterModeset DISABLED_CheckVrrAfterModeset
#else
#define MAYBE_CheckVrrAfterModeset CheckVrrAfterModeset
#endif
TEST_P(HardwareDisplayPlaneManagerAtomicTest, MAYBE_CheckVrrAfterModeset) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/2);
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kVrrEnabledPropId, .value = 0});
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);
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
    overlays.emplace_back(fake_buffer_, nullptr);
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
    overlays.emplace_back(fake_buffer_, nullptr);
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
  assigns.emplace_back(fake_buffer_, nullptr);
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_EQ(2u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, MultiplePlanesAndCrtcs) {
  DrmOverlayPlaneList assigns;
  assigns.emplace_back(fake_buffer_, nullptr);
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(1).id));
  EXPECT_EQ(4u, state_.plane_list.size());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, SharedPlanes) {
  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> buffer = CreateBuffer(gfx::Size(1, 1));

  assigns.emplace_back(fake_buffer_, nullptr);
  assigns.emplace_back(buffer, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);

  MockDrmDevice::PlaneProperties& plane_prop =
      drm_state.plane_properties.emplace_back();
  plane_prop.id = 102;
  plane_prop.crtc_mask = (1 << 0) | (1 << 1);
  plane_prop.properties = {
      {.id = kTypePropId, .value = DRM_PLANE_TYPE_OVERLAY},
      {.id = kInFormatsPropId, .value = kInFormatsBlobIdBase},
  };
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(1).id));
  EXPECT_EQ(2u, state_.plane_list.size());
  // The shared plane is now unavailable for use by the other CRTC.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &state_, assigns, fake_drm_->crtc_property(0).id));
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, UnusedPlanesAreReleased) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<DrmFramebuffer> overlay_buffer = CreateBuffer(gfx::Size(1, 1));
  assigns.emplace_back(primary_buffer, nullptr);
  assigns.emplace_back(overlay_buffer, nullptr);
  HardwareDisplayPlaneList hdpl;

  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  assigns.clear();
  assigns.emplace_back(primary_buffer, nullptr);
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
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<DrmFramebuffer> overlay_buffer = CreateBuffer(gfx::Size(1, 1));
  assigns.emplace_back(primary_buffer, nullptr);
  assigns.emplace_back(overlay_buffer, nullptr);
  HardwareDisplayPlaneList hdpl;

  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(base::TimeDelta());
  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  EXPECT_TRUE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id));
  EXPECT_TRUE(
      fake_drm_->plane_manager()->Commit(&hdpl, page_flip_request, nullptr));
  EXPECT_TRUE(fake_drm_->plane_manager()->planes().front()->in_use());
  assigns.emplace_back(overlay_buffer, nullptr);

  fake_drm_->plane_manager()->BeginFrame(&hdpl);
  // Assign overlay planes will fail since there aren't enough planes.
  EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
      &hdpl, assigns, fake_drm_->crtc_property(0).id));

  // The primary plane should still be in use since we failed to assign
  // planes and did not commit a new configuration.
  EXPECT_TRUE(fake_drm_->plane_manager()->planes().front()->in_use());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, PageflipTestRestoresInUse) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  DrmOverlayPlaneList assigns;
  scoped_refptr<DrmFramebuffer> primary_buffer =
      CreateBuffer(kDefaultBufferSize);
  scoped_refptr<DrmFramebuffer> overlay_buffer = CreateBuffer(gfx::Size(1, 1));
  assigns.emplace_back(primary_buffer, nullptr);
  assigns.emplace_back(overlay_buffer, nullptr);
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
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  DrmOverlayPlaneList single_assign;
  single_assign.emplace_back(CreateBuffer(kDefaultBufferSize), nullptr);

  DrmOverlayPlaneList overlay_assigns;
  overlay_assigns.emplace_back(CreateBuffer(kDefaultBufferSize), nullptr);
  overlay_assigns.emplace_back(CreateBuffer(kDefaultBufferSize), nullptr);

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
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(drm_state, use_atomic_);

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
  assigns.emplace_back(fake_buffer_, nullptr);

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/2);
  fake_drm_->InitializeState(drm_state, use_atomic_);

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
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count*/ 2,
      /*planes_per_crtc=*/1,
      /*movable_planes=*/1);
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);

  DrmOverlayPlaneList assigns;
  assigns.emplace_back(fake_buffer_, nullptr);

  DrmOverlayPlaneList assigns_with_overlay;
  assigns_with_overlay.emplace_back(fake_buffer_, nullptr);
  assigns_with_overlay.emplace_back(CreateBuffer(gfx::Size(1, 1)), nullptr);

  auto get_overlay_owner = [&]() {
    for (const auto& plane : fake_drm_->plane_manager()->planes()) {
      if (plane->type() == DRM_PLANE_TYPE_OVERLAY) {
        return plane->owning_crtc();
      }
    }
    NOTREACHED();
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
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count*/ 2,
      /*planes_per_crtc=*/1,
      /*movable_planes=*/1);
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);

  DrmOverlayPlaneList assigns_with_overlay;
  assigns_with_overlay.emplace_back(fake_buffer_, nullptr);
  assigns_with_overlay.emplace_back(CreateBuffer(gfx::Size(1, 1)), nullptr);

  auto get_overlay_owner = [&]() {
    for (const auto& plane : fake_drm_->plane_manager()->planes()) {
      if (plane->type() == DRM_PLANE_TYPE_OVERLAY) {
        return plane->owning_crtc();
      }
    }
    NOTREACHED();
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
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count*/ 2,
      /*planes_per_crtc=*/1,
      /*movable_planes=*/1);
  fake_drm_->InitializeState(drm_state, /*use_atomic=*/true);

  DrmOverlayPlaneList assigns_with_overlay;
  assigns_with_overlay.emplace_back(fake_buffer_, nullptr);
  assigns_with_overlay.emplace_back(CreateBuffer(gfx::Size(1, 1)), nullptr);

  auto get_overlay_owner = [&]() {
    for (const auto& plane : fake_drm_->plane_manager()->planes()) {
      if (plane->type() == DRM_PLANE_TYPE_OVERLAY) {
        return plane->owning_crtc();
      }
    }
    NOTREACHED();
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

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       SetColorCorrectionOnAllCrtcPlanes_Success) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  drm_state.plane_properties[0].properties.push_back(
      {.id = kPlaneCtmId, .value = 0});
  drm_state.plane_properties[1].properties.push_back(
      {.id = kPlaneCtmId, .value = 0});
  fake_drm_->InitializeState(drm_state, use_atomic_);

  ScopedDrmColorCtmPtr ctm_blob(CreateCTMBlob(std::vector<float>(9)));
  EXPECT_TRUE(fake_drm_->plane_manager()->SetColorCorrectionOnAllCrtcPlanes(
      fake_drm_->crtc_property(0).id, std::move(ctm_blob)));
  EXPECT_EQ(1, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       SetColorCorrectionOnAllCrtcPlanes_NoPlaneCtmProperty) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  ScopedDrmColorCtmPtr ctm_blob(CreateCTMBlob(std::vector<float>(9)));
  EXPECT_FALSE(fake_drm_->plane_manager()->SetColorCorrectionOnAllCrtcPlanes(
      fake_drm_->crtc_property(0).id, std::move(ctm_blob)));
  EXPECT_EQ(0, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest,
       SetColorCorrectionOnAllCrtcPlanes_OnePlaneMissingCtmProperty) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/2);
  drm_state.plane_properties[0].properties.push_back(
      {.id = kPlaneCtmId, .value = 0});
  fake_drm_->InitializeState(drm_state, use_atomic_);

  ScopedDrmColorCtmPtr ctm_blob(CreateCTMBlob(std::vector<float>(9)));
  EXPECT_FALSE(fake_drm_->plane_manager()->SetColorCorrectionOnAllCrtcPlanes(
      fake_drm_->crtc_property(0).id, std::move(ctm_blob)));
  EXPECT_EQ(0, fake_drm_->get_commit_count());
}

TEST_P(HardwareDisplayPlaneManagerTest, SetColorMatrix_Success) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_TRUE(fake_drm_->plane_manager()->SetColorMatrix(
      fake_drm_->crtc_property(0).id, std::vector<float>(9)));
  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
#if defined(COMMIT_PROPERTIES_ON_PAGE_FLIP)
    EXPECT_EQ(1, fake_drm_->get_commit_count());
#else
    EXPECT_EQ(2, fake_drm_->get_commit_count());
#endif
    EXPECT_NE(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "CTM"));
  } else {
    EXPECT_EQ(1, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetColorMatrix_ErrorEmptyCtm) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetColorMatrix(
      fake_drm_->crtc_property(0).id, {}));
  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    EXPECT_EQ(1, fake_drm_->get_commit_count());
    EXPECT_EQ(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "CTM"));
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetGammaCorrection_MissingDegamma) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      fake_drm_->crtc_property(0).id, {{0, 0, 0}}, {}));
  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    // Page flip should succeed even if the properties failed to be updated.
    EXPECT_EQ(1, fake_drm_->get_commit_count());
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }

  drm_state.crtc_properties[0].properties.push_back(
      {.id = kDegammaLutSizePropId, .value = 1});
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      fake_drm_->crtc_property(0).id, {{0, 0, 0}}, {}));
  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    // Page flip should succeed even if the properties failed to be updated.
    EXPECT_EQ(2, fake_drm_->get_commit_count());
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetGammaCorrection_MissingGamma) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      fake_drm_->crtc_property(0).id, {}, {{0, 0, 0}}));
  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    // Page flip should succeed even if the properties failed to be updated.
    EXPECT_EQ(1, fake_drm_->get_commit_count());
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }

  drm_state.crtc_properties[0].properties.push_back(
      {.id = kGammaLutSizePropId, .value = 1});

  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      fake_drm_->crtc_property(0).id, {}, {{0, 0, 0}}));
  if (use_atomic_) {
    HardwareDisplayPlaneList state;
    PerformPageFlip(/*crtc_idx=*/0, &state);
    // Page flip should succeed even if the properties failed to be updated.
    EXPECT_EQ(2, fake_drm_->get_commit_count());
  } else {
    EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetGammaCorrection_LegacyGamma) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  fake_drm_->set_legacy_gamma_ramp_expectation(true);
  EXPECT_TRUE(fake_drm_->plane_manager()->SetGammaCorrection(
      fake_drm_->crtc_property(0).id, {}, {{0, 0, 0}}));
  EXPECT_EQ(1, fake_drm_->get_set_gamma_ramp_count());
  EXPECT_EQ(0, fake_drm_->get_commit_count());
  EXPECT_EQ(0, fake_drm_->get_set_object_property_count());

  // Ensure disabling gamma also works on legacy.
  EXPECT_TRUE(fake_drm_->plane_manager()->SetGammaCorrection(
      fake_drm_->crtc_property(0).id, {}, {}));
  EXPECT_EQ(2, fake_drm_->get_set_gamma_ramp_count());
  EXPECT_EQ(0, fake_drm_->get_commit_count());
  EXPECT_EQ(0, fake_drm_->get_set_object_property_count());
}

TEST_P(HardwareDisplayPlaneManagerTest, SetGammaCorrection_Success) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kCtmPropId, .value = 0});
  fake_drm_->InitializeState(drm_state, use_atomic_);

  EXPECT_FALSE(fake_drm_->plane_manager()->SetGammaCorrection(
      fake_drm_->crtc_property(0).id, {{0, 0, 0}}, {}));
  EXPECT_EQ(0, fake_drm_->get_commit_count());

  drm_state.crtc_properties[0].properties.push_back(
      {.id = kDegammaLutSizePropId, .value = 1});
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kDegammaLutPropId, .value = 0});
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kGammaLutSizePropId, .value = 1});
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kGammaLutPropId, .value = 0});
  fake_drm_->InitializeState(drm_state, use_atomic_);

  HardwareDisplayPlaneList state;
  // Check that we reset the properties correctly.
  EXPECT_TRUE(fake_drm_->plane_manager()->SetGammaCorrection(
      fake_drm_->crtc_property(0).id, {}, {}));
  if (use_atomic_) {
    PerformPageFlip(/*crtc_idx=*/0, &state);
#if defined(COMMIT_PROPERTIES_ON_PAGE_FLIP)
    EXPECT_EQ(1, fake_drm_->get_commit_count());
#else
    EXPECT_EQ(2, fake_drm_->get_commit_count());
#endif
    EXPECT_EQ(
        0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "GAMMA_LUT"));
    EXPECT_EQ(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id,
                                       "DEGAMMA_LUT"));
  } else {
    EXPECT_EQ(2, fake_drm_->get_set_object_property_count());
  }

  EXPECT_TRUE(fake_drm_->plane_manager()->SetGammaCorrection(
      fake_drm_->crtc_property(0).id, {{0, 0, 0}}, {{0, 0, 0}}));
  if (use_atomic_) {
    PerformPageFlip(/*crtc_idx=*/0, &state);
#if defined(COMMIT_PROPERTIES_ON_PAGE_FLIP)
    EXPECT_EQ(2, fake_drm_->get_commit_count());
#else
    EXPECT_EQ(4, fake_drm_->get_commit_count());
#endif
    EXPECT_NE(
        0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id, "GAMMA_LUT"));
    EXPECT_NE(0u, GetCrtcPropertyValue(fake_drm_->crtc_property(0).id,
                                       "DEGAMMA_LUT"));
  } else {
    EXPECT_EQ(4, fake_drm_->get_set_object_property_count());
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, SetBackgroundColor_Success) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kBackgroundColorPropId, .value = 0});
  fake_drm_->InitializeState(drm_state, use_atomic_);
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

  drm_state.crtc_properties[0].properties.push_back(
      {.id = kBackgroundColorPropId, .value = 1});
  fake_drm_->InitializeState(drm_state, use_atomic_);
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

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/2, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  DrmOverlayPlaneList assigns1;
  assigns1.emplace_back(fake_buffer_, nullptr);
  DrmOverlayPlaneList assigns2;
  assigns2.emplace_back(fake_buffer2, nullptr);

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
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/3, /*planes_per_crtc=*/1);
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 1});
  drm_state.crtc_properties[2].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 2});

  EXPECT_FALSE(fake_drm_->InitializeStateWithResult(drm_state, use_atomic_));
}

TEST_P(HardwareDisplayPlaneManagerTest,
       InitializationSucceedsIfSupportForOutFencePropertiesIsComplete) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/3, /*planes_per_crtc=*/1);
  drm_state.crtc_properties[0].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 1});
  drm_state.crtc_properties[1].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 2});
  drm_state.crtc_properties[2].properties.push_back(
      {.id = kOutFencePtrPropId, .value = 3});

  EXPECT_TRUE(fake_drm_->InitializeStateWithResult(drm_state, use_atomic_));
}

// Verifies that formats with 2 bits of alpha decay to opaques for AddFB2().
TEST_P(HardwareDisplayPlaneManagerTest, ForceOpaqueFormatsForAddFramebuffer) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/3, /*planes_per_crtc=*/1);

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
  fake_drm_->SetPropertyBlob(MockDrmDevice::AllocateInFormatsBlob(
      kInFormatsBlobIdBase, {DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010},
      {}));
  fake_drm_->InitializeState(drm_state, use_atomic_);

  for (const auto& format_pair : kFourCCFormats) {
    scoped_refptr<DrmFramebuffer> drm_fb =
        CreateBufferWithFormat(kDefaultBufferSize, format_pair.input_fourcc);

    EXPECT_EQ(drm_fb->framebuffer_pixel_format(), format_pair.input_fourcc);
    EXPECT_EQ(drm_fb->opaque_framebuffer_pixel_format(),
              format_pair.used_fourcc);
  }
}

TEST_P(HardwareDisplayPlaneManagerTest, GetHardwareCapabilities) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/4, /*planes_per_crtc=*/7);
  fake_drm_->InitializeState(drm_state, use_atomic_);

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

    fake_drm_->InitializeState(drm_state, use_atomic_);
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
    fake_drm_ = new MockDrmDevice(std::move(gbm_device));
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
    planes.emplace_back(CreateBuffer(kDefaultBufferSize), nullptr);
    planes.emplace_back(CreateBuffer(kDefaultBufferSize), nullptr);
    return planes;
  }

  DrmOverlayPlaneList CreatePlanesWithFences() {
    DrmOverlayPlaneList planes;
    planes.emplace_back(CreateBuffer(kDefaultBufferSize),
                        fake_fence_fd1_.GetGpuFence());
    planes.emplace_back(CreateBuffer(kDefaultBufferSize),
                        fake_fence_fd2_.GetGpuFence());
    return planes;
  }

  scoped_refptr<MockDrmDevice> fake_drm_;
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

TEST_P(HardwareDisplayPlaneManagerAtomicTest, OriginalModifiersSupportOnly) {
  fake_drm_->SetPropertyBlob(MockDrmDevice::AllocateInFormatsBlob(
      kInFormatsBlobIdBase, {DRM_FORMAT_NV12}, {}));

  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  {
    DrmOverlayPlaneList assigns;
    // Create as NV12 since this is required for rotation support.
    std::unique_ptr<GbmBuffer> buffer = fake_drm_->gbm_device()->CreateBuffer(
        DRM_FORMAT_NV12, kDefaultBufferSize, GBM_BO_USE_SCANOUT);
    scoped_refptr<DrmFramebuffer> framebuffer_original =
        DrmFramebuffer::AddFramebuffer(fake_drm_, buffer.get(),
                                       kDefaultBufferSize, {}, true);
    assigns.emplace_back(framebuffer_original, nullptr);
    assigns.back().plane_transform = gfx::OVERLAY_TRANSFORM_ROTATE_270;

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
    assigns.emplace_back(framebuffer_non_original, nullptr);
    assigns.back().plane_transform = gfx::OVERLAY_TRANSFORM_ROTATE_270;
    EXPECT_FALSE(fake_drm_->plane_manager()->AssignOverlayPlanes(
        &state_, assigns, fake_drm_->crtc_property(0).id));
  }
}

TEST_P(HardwareDisplayPlaneManagerAtomicTest, OverlaySourceCrop) {
  auto drm_state = MockDrmDevice::MockDrmState::CreateStateWithDefaultObjects(
      /*crtc_count=*/1, /*planes_per_crtc=*/1);
  fake_drm_->InitializeState(drm_state, use_atomic_);

  {
    DrmOverlayPlaneList assigns;
    assigns.emplace_back(fake_buffer_, nullptr);

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
    assigns.emplace_back(fake_buffer_, 0,
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
    assigns.emplace_back(fake_buffer_, 0,
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
  auto drm_device = base::MakeRefCounted<MockDrmDevice>(std::move(gbm_device));
  auto plane_manager =
      std::make_unique<HardwareDisplayPlaneManagerAtomic>(drm_device.get());
  HardwareDisplayPlaneList plane_list;
  HardwareDisplayPlaneAtomicMock hw_plane;
  std::unique_ptr<GbmBuffer> buffer = drm_device->gbm_device()->CreateBuffer(
      DRM_FORMAT_XRGB8888, kDefaultBufferSize, GBM_BO_USE_SCANOUT);
  scoped_refptr<DrmFramebuffer> framebuffer = DrmFramebuffer::AddFramebuffer(
      drm_device, buffer.get(), kDefaultBufferSize);
  DrmOverlayPlane overlay(framebuffer, nullptr);
  overlay.enable_blend = true;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, gfx::Rect());
  EXPECT_EQ(hw_plane.framebuffer(), framebuffer->framebuffer_id());

  overlay.enable_blend = false;
  plane_manager->SetPlaneData(&plane_list, &hw_plane, overlay, 1, gfx::Rect());
  EXPECT_EQ(hw_plane.framebuffer(), framebuffer->opaque_framebuffer_id());
}

}  // namespace ui
