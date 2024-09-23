// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_manager.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {

namespace {

constexpr gfx::AcceleratedWidget kPrimaryWidget = 1;
constexpr gfx::AcceleratedWidget kSecondaryWidget = 2;

class TestDrmOverlayManager : public DrmOverlayManager {
 public:
  explicit TestDrmOverlayManager(
      bool allow_sync_and_real_buffer_page_flip_testing)
      : DrmOverlayManager(/*handle_overlays_swap_failure=*/false,
                          allow_sync_and_real_buffer_page_flip_testing) {}
  TestDrmOverlayManager(bool handle_overlays_swap_failure,
                        bool allow_sync_and_real_buffer_page_flip_testing)
      : DrmOverlayManager(handle_overlays_swap_failure,
                          allow_sync_and_real_buffer_page_flip_testing) {}
  TestDrmOverlayManager() : TestDrmOverlayManager(false) {}
  ~TestDrmOverlayManager() override = default;

  using DrmOverlayManager::UpdateCacheForOverlayCandidates;

  std::vector<std::vector<OverlaySurfaceCandidate>>& requests() {
    return requests_;
  }

  // DrmOverlayManager:
  void SendOverlayValidationRequest(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      gfx::AcceleratedWidget widget) override {
    requests_.push_back(candidates);
  }
  std::vector<OverlayStatus> SendOverlayValidationRequestSync(
      const std::vector<OverlaySurfaceCandidate>& candidates,
      gfx::AcceleratedWidget widget) override {
    requests_.push_back(candidates);
    std::vector<OverlayStatus> status;
    status.reserve(candidates.size());
    for (size_t i = 0; i < candidates.size(); i++) {
      status.emplace_back(candidates[i].overlay_handled
                              ? OverlayStatus::OVERLAY_STATUS_ABLE
                              : OverlayStatus::OVERLAY_STATUS_NOT);
    }
    return status;
  }
  void GetHardwareCapabilities(
      gfx::AcceleratedWidget widget,
      HardwareCapabilitiesCallback& receive_callback) override {
    HardwareCapabilities hardware_capabilities;
    hardware_capabilities.num_overlay_capable_planes = num_planes_response_;
    // Immediately respond to the callback.
    receive_callback.Run(hardware_capabilities);
  }

  base::TimeTicks GetDisallowFullscreenOverlaysEndTime() const {
    return disallow_fullscreen_overlays_end_time();
  }

  int num_planes_response_ = 0;

 private:
  std::vector<std::vector<OverlaySurfaceCandidate>> requests_;
};

OverlaySurfaceCandidate CreateCandidate(const gfx::Rect& rect,
                                        int plane_z_order) {
  OverlaySurfaceCandidate candidate;
  candidate.transform = gfx::OVERLAY_TRANSFORM_NONE;
  candidate.format = gfx::BufferFormat::YUV_420_BIPLANAR;
  candidate.plane_z_order = plane_z_order;
  candidate.buffer_size = rect.size();
  candidate.display_rect = gfx::RectF(rect);
  candidate.crop_rect = gfx::RectF(rect);
  return candidate;
}

class DrmOverlayManagerTest : public testing::Test {
 public:
  DrmOverlayManagerTest() = default;

  void SetUp() override {
    manager_.SetSupportedBufferFormats(kPrimaryWidget,
                                       {gfx::BufferFormat::YUV_420_BIPLANAR});
    manager_.SetSupportedBufferFormats(kSecondaryWidget,
                                       {gfx::BufferFormat::YUV_420_BIPLANAR});
  }

 protected:
  TestDrmOverlayManager manager_;
};

}  // namespace

TEST_F(DrmOverlayManagerTest, CacheLogic) {
  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // The first three times that CheckOverlaySupport() is called for an overlay
  // configuration it won't send a validation request.
  for (int i = 0; i < 3; ++i) {
    manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates[0].overlay_handled);
    EXPECT_FALSE(candidates[1].overlay_handled);
    EXPECT_EQ(manager_.requests().size(), 0u);
  }

  // The fourth call with the same overlay configuration should trigger a
  // request to validate the configuration. Still assume the overlay
  // configuration won't work until we get a response.
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_FALSE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
  EXPECT_EQ(manager_.requests().size(), 1u);

  // While waiting for a response we shouldn't send the same request.
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_FALSE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
  ASSERT_EQ(manager_.requests().size(), 1u);

  // Receive response that the overlay configuration will work.
  manager_.UpdateCacheForOverlayCandidates(
      manager_.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));
  manager_.requests().clear();

  // CheckOverlaySupport() should now indicate the overlay configuration will
  // work.
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  EXPECT_EQ(manager_.requests().size(), 0u);
}

// Tests that the crop rect changing will make a new request.
TEST_F(DrmOverlayManagerTest, CropRectCacheLogic) {
  // Candidates fo single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0)};

  // The first three times won't send a validation request. The fourth will send
  // a request.
  for (int i = 0; i < 3; ++i) {
    manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates.back().overlay_handled);
    EXPECT_EQ(manager_.requests().size(), 0u);
  }
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_EQ(manager_.requests().size(), 1u);

  // Receive response that the overlay configuration will work.
  manager_.UpdateCacheForOverlayCandidates(
      manager_.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));
  manager_.requests().clear();

  // Overlay configuration should work.
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates.back().overlay_handled);

  // Now, adjust the crop rect.
  candidates.back().overlay_handled = false;
  candidates.back().crop_rect = gfx::RectF(0, 0, 0.5f, 0.5f);

  // The first three times won't send a validation request. The fourth will send
  // a request.
  for (int i = 0; i < 3; ++i) {
    manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates.back().overlay_handled);
    EXPECT_EQ(manager_.requests().size(), 0u);
  }
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_EQ(manager_.requests().size(), 1u);

  // Receive response that the overlay configuration won't work.
  manager_.UpdateCacheForOverlayCandidates(
      manager_.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_NOT));
  manager_.requests().clear();

  // Overlay configuration should not work.
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_FALSE(candidates.back().overlay_handled);
}

TEST_F(DrmOverlayManagerTest, DifferentWidgetCache) {
  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Call 4 Times to go beyond the Throttle Request Size
  ASSERT_EQ(manager_.requests().size(), 0u);
  for (int i = 0; i < 4; ++i) {
    manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates[0].overlay_handled);
    EXPECT_FALSE(candidates[1].overlay_handled);
  }
  ASSERT_EQ(manager_.requests().size(), 1u);

  // Receive response that the overlay configuration on kPrimaryWidget will
  // work.
  manager_.UpdateCacheForOverlayCandidates(
      manager_.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));
  manager_.requests().clear();

  // Overlay should not be handled when using a different widget
  manager_.CheckOverlaySupport(&candidates, kSecondaryWidget);
  EXPECT_FALSE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
  EXPECT_EQ(manager_.requests().size(), 0u);
}

TEST_F(DrmOverlayManagerTest, MultipleWidgetCacheSupport) {
  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Call 4 Times to go beyond the Throttle Request Size
  ASSERT_EQ(manager_.requests().size(), 0u);
  for (int i = 0; i < 4; ++i) {
    manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates[0].overlay_handled);
    EXPECT_FALSE(candidates[1].overlay_handled);
    manager_.CheckOverlaySupport(&candidates, kSecondaryWidget);
    EXPECT_FALSE(candidates[0].overlay_handled);
    EXPECT_FALSE(candidates[1].overlay_handled);
  }
  EXPECT_EQ(manager_.requests().size(), 2u);

  // Receive response that the overlay configuration on kPrimaryWidget will
  // work.
  manager_.UpdateCacheForOverlayCandidates(
      manager_.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));

  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);

  // Receive response that the overlay configuration on kSecondaryWidget will
  // work.
  manager_.UpdateCacheForOverlayCandidates(
      manager_.requests().front(), kSecondaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));
  manager_.requests().clear();

  // Both Widgets should be handled
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  manager_.CheckOverlaySupport(&candidates, kSecondaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  EXPECT_EQ(manager_.requests().size(), 0u);
}

TEST_F(DrmOverlayManagerTest, DifferentWidgetsSameCandidatesAreDistinct) {
  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Looping 4 times, but each widget is only checked twice, below the Throttle
  // Request Size (3).
  for (int i = 0; i < 4; ++i) {
    manager_.CheckOverlaySupport(&candidates,
                                 i % 2 ? kPrimaryWidget : kSecondaryWidget);
  }
  EXPECT_EQ(manager_.requests().size(), 0u);
}

TEST_F(DrmOverlayManagerTest, NonIntegerDisplayRect) {
  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Submit a set of candidates that could potentially be displayed in an
  // overlay so they are stored in the cache. This ensures a comparison gets
  // made adding the next value to the cache.
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);

  // Modify the display_rect for the second candidate so it's non-integer. We
  // will never try to promote this to an overlay but something will get stored
  // in the cache. This verifies we don't try to convert the non-integer RectF
  // into a Rect which DCHECKs.
  candidates[1].display_rect = gfx::RectF(9.4, 10.43, 20.11, 20.99);
  manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);
}

TEST_F(DrmOverlayManagerTest, RequiredOverlayMultiDisplay) {
  // Primary has a requirement, secondary does not, should only make a request
  // on the primary.
  std::vector<OverlaySurfaceCandidate> candidates1 = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0)};

  manager_.RegisterOverlayRequirement(kPrimaryWidget, true);
  manager_.RegisterOverlayRequirement(kSecondaryWidget, false);
  // Call 4 Times to go beyond the Throttle Request Size
  for (int i = 0; i < 4; ++i)
    manager_.CheckOverlaySupport(&candidates1, kPrimaryWidget);
  EXPECT_EQ(manager_.requests().size(), 1u);
  // Call 4 Times to go beyond the Throttle Request Size
  for (int i = 0; i < 4; ++i)
    manager_.CheckOverlaySupport(&candidates1, kSecondaryWidget);
  EXPECT_EQ(manager_.requests().size(), 1u);
  manager_.requests().clear();

  // Secondary has a requirement, primary does not, should only make a request
  // on the secondary.
  std::vector<OverlaySurfaceCandidate> candidates2 = {
      CreateCandidate(gfx::Rect(0, 0, 200, 200), 0)};

  manager_.RegisterOverlayRequirement(kPrimaryWidget, false);
  manager_.RegisterOverlayRequirement(kSecondaryWidget, true);
  // Call 4 Times to go beyond the Throttle Request Size
  for (int i = 0; i < 4; ++i)
    manager_.CheckOverlaySupport(&candidates2, kPrimaryWidget);
  EXPECT_TRUE(manager_.requests().empty());
  // Call 4 Times to go beyond the Throttle Request Size
  for (int i = 0; i < 4; ++i)
    manager_.CheckOverlaySupport(&candidates2, kSecondaryWidget);
  EXPECT_EQ(manager_.requests().size(), 1u);
}

TEST_F(DrmOverlayManagerTest, ObservingHardwareCapabilities) {
  manager_.num_planes_response_ = 2;

  int primary_calls = 0;
  HardwareCapabilitiesCallback primary_callback = base::BindRepeating(
      [](int* calls, HardwareCapabilities hc) {
        (*calls)++;
        EXPECT_EQ(hc.num_overlay_capable_planes, 2);
      },
      &primary_calls);
  manager_.StartObservingHardwareCapabilities(kPrimaryWidget, primary_callback);
  EXPECT_EQ(primary_calls, 1);

  manager_.DisplaysConfigured();

  EXPECT_EQ(primary_calls, 2);

  int secondary_calls = 0;
  HardwareCapabilitiesCallback secondary_callback = base::BindRepeating(
      [](int* calls, HardwareCapabilities hc) {
        (*calls)++;
        EXPECT_EQ(hc.num_overlay_capable_planes, 2);
      },
      &secondary_calls);
  manager_.StartObservingHardwareCapabilities(kSecondaryWidget,
                                              secondary_callback);
  // Only the secondary callback should be called.
  EXPECT_EQ(primary_calls, 2);
  EXPECT_EQ(secondary_calls, 1);

  manager_.DisplaysConfigured();

  // Both callbacks are called.
  EXPECT_EQ(primary_calls, 3);
  EXPECT_EQ(secondary_calls, 2);

  manager_.StopObservingHardwareCapabilities(kPrimaryWidget);
  manager_.DisplaysConfigured();
  manager_.DisplaysConfigured();

  // The primary callback won't be called anymore.
  EXPECT_EQ(primary_calls, 3);
  EXPECT_EQ(secondary_calls, 4);
}

TEST_F(DrmOverlayManagerTest, SingleClipRectUnderlaySupport) {
  // Candidates for output surface and underlay quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), -1)};

  // Set a clip rect that imposes a restriction on |display_rect|.
  candidates[1].clip_rect = gfx::Rect(10, 10, 15, 15);

  for (int i = 0; i < 4; i++)
    manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);

  EXPECT_EQ(manager_.requests().size(), 1u);
  EXPECT_TRUE(manager_.requests()[0][1].overlay_handled);

  manager_.requests().clear();
  // Now make the overlay candidate a single-on-top overlay. Single-on-top
  // overlays with restrictive clip rects are not supported.
  candidates[1].plane_z_order = 1;
  for (int i = 0; i < 4; i++)
    manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);

  EXPECT_EQ(manager_.requests().size(), 1u);
  EXPECT_FALSE(manager_.requests()[0][1].overlay_handled);
}

TEST_F(DrmOverlayManagerTest, MultiClipRectUnderlaySupport) {
  // Two underlay quads who's |display_rect| overlap. The order here is
  // important; even though the -2 underlay comes first in the list it will be
  // occluded by the -1 underlay and when -1 clipped the -2 underlay should
  // fail.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), -2),
      CreateCandidate(gfx::Rect(20, 20, 20, 20), -1)};

  // Set a clip rect that imposes a restriction on |display_rect|.
  candidates[1].clip_rect = gfx::Rect(10, 10, 15, 15);
  candidates[2].clip_rect = gfx::Rect(20, 20, 15, 15);

  for (int i = 0; i < 4; i++)
    manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);

  EXPECT_EQ(manager_.requests().size(), 1u);
  EXPECT_FALSE(manager_.requests()[0][1].overlay_handled);
  EXPECT_TRUE(manager_.requests()[0][2].overlay_handled);

  manager_.requests().clear();
  // Now remove the clipping constraint on the -1 underlay which should allow
  // the -2 underlay to be handled.
  candidates[2].clip_rect = gfx::Rect(20, 20, 50, 50);
  for (int i = 0; i < 4; i++)
    manager_.CheckOverlaySupport(&candidates, kPrimaryWidget);

  EXPECT_EQ(manager_.requests().size(), 1u);
  EXPECT_TRUE(manager_.requests()[0][1].overlay_handled);
  EXPECT_TRUE(manager_.requests()[0][2].overlay_handled);
}

TEST_F(DrmOverlayManagerTest, SupportedBufferFormat) {
  // Make the manager to use sync testing for convenience.
  TestDrmOverlayManager manager(true);
  manager.SetSupportedBufferFormats(
      kPrimaryWidget,
      {gfx::BufferFormat::BGRA_8888, gfx::BufferFormat::RGBA_8888});
  manager.SetSupportedBufferFormats(kSecondaryWidget,
                                    {gfx::BufferFormat::YUV_420_BIPLANAR});

  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 150, 150), -1),
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};
  candidates[0].format = gfx::BufferFormat::RGBA_8888;
  candidates[1].format = gfx::BufferFormat::YUV_420_BIPLANAR;
  candidates[2].format = gfx::BufferFormat::BGRA_8888;

  // Primary widget supports BGRA/RGBA only.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
  EXPECT_TRUE(candidates[2].overlay_handled);
  EXPECT_EQ(manager.requests().size(), 1u);

  auto reset_candidates = [](std::vector<OverlaySurfaceCandidate>& candidates) {
    for (auto& candidate : candidates) {
      candidate.overlay_handled = false;
    }
  };

  manager.requests().clear();
  reset_candidates(candidates);

  // Secondary widget supports N12 only.
  manager.CheckOverlaySupport(&candidates, kSecondaryWidget);
  EXPECT_FALSE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  EXPECT_FALSE(candidates[2].overlay_handled);
  EXPECT_EQ(manager.requests().size(), 1u);

  manager.requests().clear();
  reset_candidates(candidates);

  // Make primary widget support more buffer formats.
  manager.SetSupportedBufferFormats(
      kPrimaryWidget,
      {gfx::BufferFormat::YUV_420_BIPLANAR, gfx::BufferFormat::BGRA_8888,
       gfx::BufferFormat::RGBA_8888});

  // Primary widget supports BGRA/RGBA and NV12 now.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  EXPECT_TRUE(candidates[2].overlay_handled);
  EXPECT_EQ(manager.requests().size(), 1u);
}

// Verifies that the |TestDrmOverlayManager| uses fast path for fullscreen
// overlays. That is, if |handle_overlays_swap_failure| is enabled, it marks
// fullscreen overlays as suitable candidates, but once it gets a swap failure
// notification, it fallbacks to drm testing.
TEST_F(DrmOverlayManagerTest, HandleFastPathFullScreenOverlays) {
  base::test::SingleThreadTaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  TestDrmOverlayManager manager(
      /*handle_overlays_swap_failure=*/true,
      /*allow_sync_and_real_buffer_page_flip_testing=*/true);
  manager.SetSupportedBufferFormats(kPrimaryWidget,
                                    {gfx::BufferFormat::YUV_420_BIPLANAR});

  // Check overlay support and expect fullscreen is handled without any requests
  // for overlays' validation sent.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0)};
  candidates.front().overlay_type = gfx::OverlayType::kFullScreen;

  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);

  EXPECT_EQ(manager.requests().size(), 0u);
  EXPECT_TRUE(candidates.front().overlay_handled);

  // Notify the manager that the fullscreen overlay were promoted and the
  // next swap is a fullscreen one.
  manager.OnPromotedOverlayTypes({gfx::OverlayType::kFullScreen});

  // Store the current time and use it later to fast forward it.
  const auto time_now = base::TimeTicks::Now();
  // The swap has failed. The manager must stop fullscreen overlays' promotion.
  manager.OnSwapBuffersComplete(
      gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED);
  EXPECT_TRUE(!manager.GetDisallowFullscreenOverlaysEndTime().is_null());

  // Now that the previous fullscreen overlay's swap failed, the manager must
  // fallback to drm test for these overlays as well.
  std::vector<OverlaySurfaceCandidate> candidates2 = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0)};
  candidates2.front().overlay_type = gfx::OverlayType::kFullScreen;

  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);

  // As expected, there are validation requests.
  EXPECT_EQ(manager.requests().size(), 1u);
  EXPECT_TRUE(candidates.front().overlay_handled);
  manager.requests().clear();

  // Fast forward the time as the manager waits X hours until it can promote
  // the fullscreen overlays again.
  size_t kFastForwardAttempts = 5;
  while (!manager.GetDisallowFullscreenOverlaysEndTime().is_null()) {
    env.FastForwardBy(manager.GetDisallowFullscreenOverlaysEndTime() -
                      time_now);
    // Break in case if something goes very wrong.
    if (--kFastForwardAttempts <= 0) {
      break;
    }
  }
  std::vector<OverlaySurfaceCandidate> candidates3 = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0)};
  candidates3.front().overlay_type = gfx::OverlayType::kFullScreen;

  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);

  // Sanity check.
  ASSERT_TRUE(manager.GetDisallowFullscreenOverlaysEndTime().is_null());

  // As expected, there are no validation requests now and fullscreen overlays
  // can be promoted without validation now.
  EXPECT_EQ(manager.requests().size(), 0u);
  EXPECT_TRUE(candidates.front().overlay_handled);
}

}  // namespace ui
