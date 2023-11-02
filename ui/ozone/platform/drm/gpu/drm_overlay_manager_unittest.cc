// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_overlay_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {
namespace {

constexpr gfx::AcceleratedWidget kPrimaryWidget = 1;
constexpr gfx::AcceleratedWidget kSecondaryWidget = 2;

class TestDrmOverlayManager : public DrmOverlayManager {
 public:
  TestDrmOverlayManager() : DrmOverlayManager(false) {}
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
    return {};
  }
  void GetHardwareCapabilities(
      gfx::AcceleratedWidget widget,
      ui::HardwareCapabilitiesCallback& receive_callback) override {
    ui::HardwareCapabilities hardware_capabilities;
    hardware_capabilities.num_overlay_capable_planes = num_planes_response_;
    // Immediately respond to the callback.
    receive_callback.Run(hardware_capabilities);
  }

  int num_planes_response_ = 0;

 private:
  std::vector<std::vector<OverlaySurfaceCandidate>> requests_;
};

OverlaySurfaceCandidate CreateCandidate(const gfx::Rect& rect,
                                        int plane_z_order) {
  ui::OverlaySurfaceCandidate candidate;
  candidate.transform = gfx::OVERLAY_TRANSFORM_NONE;
  candidate.format = gfx::BufferFormat::YUV_420_BIPLANAR;
  candidate.plane_z_order = plane_z_order;
  candidate.buffer_size = rect.size();
  candidate.display_rect = gfx::RectF(rect);
  candidate.crop_rect = gfx::RectF(rect);
  return candidate;
}

}  // namespace

TEST(DrmOverlayManagerTest, CacheLogic) {
  TestDrmOverlayManager manager;

  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // The first three times that CheckOverlaySupport() is called for an overlay
  // configuration it won't send a validation request.
  for (int i = 0; i < 3; ++i) {
    manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates[0].overlay_handled);
    EXPECT_FALSE(candidates[1].overlay_handled);
    EXPECT_EQ(manager.requests().size(), 0u);
  }

  // The fourth call with the same overlay configuration should trigger a
  // request to validate the configuration. Still assume the overlay
  // configuration won't work until we get a response.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_FALSE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
  EXPECT_EQ(manager.requests().size(), 1u);

  // While waiting for a response we shouldn't send the same request.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_FALSE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
  ASSERT_EQ(manager.requests().size(), 1u);

  // Receive response that the overlay configuration will work.
  manager.UpdateCacheForOverlayCandidates(
      manager.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));
  manager.requests().clear();

  // CheckOverlaySupport() should now indicate the overlay configuration will
  // work.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  EXPECT_EQ(manager.requests().size(), 0u);
}

// Tests that the crop rect changing will make a new request.
TEST(DrmOverlayManagerTest, CropRectCacheLogic) {
  TestDrmOverlayManager manager;

  // Candidates fo single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0)};

  // The first three times won't send a validation request. The fourth will send
  // a request.
  for (int i = 0; i < 3; ++i) {
    manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates.back().overlay_handled);
    EXPECT_EQ(manager.requests().size(), 0u);
  }
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_EQ(manager.requests().size(), 1u);

  // Receive response that the overlay configuration will work.
  manager.UpdateCacheForOverlayCandidates(
      manager.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));
  manager.requests().clear();

  // Overlay configuration should work.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates.back().overlay_handled);

  // Now, adjust the crop rect.
  candidates.back().overlay_handled = false;
  candidates.back().crop_rect = gfx::RectF(0, 0, 0.5f, 0.5f);

  // The first three times won't send a validation request. The fourth will send
  // a request.
  for (int i = 0; i < 3; ++i) {
    manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates.back().overlay_handled);
    EXPECT_EQ(manager.requests().size(), 0u);
  }
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_EQ(manager.requests().size(), 1u);

  // Receive response that the overlay configuration won't work.
  manager.UpdateCacheForOverlayCandidates(
      manager.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_NOT));
  manager.requests().clear();

  // Overlay configuration should not work.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_FALSE(candidates.back().overlay_handled);
}

TEST(DrmOverlayManagerTest, DifferentWidgetCache) {
  TestDrmOverlayManager manager;

  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Call 4 Times to go beyond the Throttle Request Size
  ASSERT_EQ(manager.requests().size(), 0u);
  for (int i = 0; i < 4; ++i) {
    manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates[0].overlay_handled);
    EXPECT_FALSE(candidates[1].overlay_handled);
  }
  ASSERT_EQ(manager.requests().size(), 1u);

  // Receive response that the overlay configuration on kPrimaryWidget will
  // work.
  manager.UpdateCacheForOverlayCandidates(
      manager.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));
  manager.requests().clear();

  // Overlay should not be handled when using a different widget
  manager.CheckOverlaySupport(&candidates, kSecondaryWidget);
  EXPECT_FALSE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
  EXPECT_EQ(manager.requests().size(), 0u);
}

TEST(DrmOverlayManagerTest, MultipleWidgetCacheSupport) {
  TestDrmOverlayManager manager;

  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Call 4 Times to go beyond the Throttle Request Size
  ASSERT_EQ(manager.requests().size(), 0u);
  for (int i = 0; i < 4; ++i) {
    manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
    EXPECT_FALSE(candidates[0].overlay_handled);
    EXPECT_FALSE(candidates[1].overlay_handled);
    manager.CheckOverlaySupport(&candidates, kSecondaryWidget);
    EXPECT_FALSE(candidates[0].overlay_handled);
    EXPECT_FALSE(candidates[1].overlay_handled);
  }
  EXPECT_EQ(manager.requests().size(), 2u);

  // Receive response that the overlay configuration on kPrimaryWidget will
  // work.
  manager.UpdateCacheForOverlayCandidates(
      manager.requests().front(), kPrimaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));

  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);

  // Receive response that the overlay configuration on kSecondaryWidget will
  // work.
  manager.UpdateCacheForOverlayCandidates(
      manager.requests().front(), kSecondaryWidget,
      std::vector<OverlayStatus>(candidates.size(), OVERLAY_STATUS_ABLE));
  manager.requests().clear();

  // Both Widgets should be handled
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  manager.CheckOverlaySupport(&candidates, kSecondaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  EXPECT_EQ(manager.requests().size(), 0u);
}

TEST(DrmOverlayManagerTest, DifferentWidgetsSameCandidatesAreDistinct) {
  TestDrmOverlayManager manager;

  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Looping 4 times, but each widget is only checked twice, below the Throttle
  // Request Size (3).
  for (int i = 0; i < 4; ++i) {
    manager.CheckOverlaySupport(&candidates,
                                i % 2 ? kPrimaryWidget : kSecondaryWidget);
  }
  EXPECT_EQ(manager.requests().size(), 0u);
}

TEST(DrmOverlayManagerTest, NonIntegerDisplayRect) {
  TestDrmOverlayManager manager;

  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Submit a set of candidates that could potentially be displayed in an
  // overlay so they are stored in the cache. This ensures a comparison gets
  // made adding the next value to the cache.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);

  // Modify the display_rect for the second candidate so it's non-integer. We
  // will never try to promote this to an overlay but something will get stored
  // in the cache. This verifies we don't try to convert the non-integer RectF
  // into a Rect which DCHECKs.
  candidates[1].display_rect = gfx::RectF(9.4, 10.43, 20.11, 20.99);
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
}

TEST(DrmOverlayManagerTest, RequiredOverlayMultiDisplay) {
  TestDrmOverlayManager manager;

  // Primary has a requirement, secondary does not, should only make a request
  // on the primary.
  std::vector<OverlaySurfaceCandidate> candidates1 = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0)};

  manager.RegisterOverlayRequirement(kPrimaryWidget, true);
  manager.RegisterOverlayRequirement(kSecondaryWidget, false);
  // Call 4 Times to go beyond the Throttle Request Size
  for (int i = 0; i < 4; ++i)
    manager.CheckOverlaySupport(&candidates1, kPrimaryWidget);
  EXPECT_EQ(manager.requests().size(), 1u);
  // Call 4 Times to go beyond the Throttle Request Size
  for (int i = 0; i < 4; ++i)
    manager.CheckOverlaySupport(&candidates1, kSecondaryWidget);
  EXPECT_EQ(manager.requests().size(), 1u);
  manager.requests().clear();

  // Secondary has a requirement, primary does not, should only make a request
  // on the secondary.
  std::vector<OverlaySurfaceCandidate> candidates2 = {
      CreateCandidate(gfx::Rect(0, 0, 200, 200), 0)};

  manager.RegisterOverlayRequirement(kPrimaryWidget, false);
  manager.RegisterOverlayRequirement(kSecondaryWidget, true);
  // Call 4 Times to go beyond the Throttle Request Size
  for (int i = 0; i < 4; ++i)
    manager.CheckOverlaySupport(&candidates2, kPrimaryWidget);
  EXPECT_TRUE(manager.requests().empty());
  // Call 4 Times to go beyond the Throttle Request Size
  for (int i = 0; i < 4; ++i)
    manager.CheckOverlaySupport(&candidates2, kSecondaryWidget);
  EXPECT_EQ(manager.requests().size(), 1u);
}

TEST(DrmOverlayManagerTest, ObservingHardwareCapabilities) {
  TestDrmOverlayManager manager;
  manager.num_planes_response_ = 2;

  int primary_calls = 0;
  HardwareCapabilitiesCallback primary_callback = base::BindRepeating(
      [](int* calls, HardwareCapabilities hc) {
        (*calls)++;
        EXPECT_EQ(hc.num_overlay_capable_planes, 2);
      },
      &primary_calls);
  manager.StartObservingHardwareCapabilities(kPrimaryWidget, primary_callback);
  EXPECT_EQ(primary_calls, 1);

  manager.DisplaysConfigured();

  EXPECT_EQ(primary_calls, 2);

  int secondary_calls = 0;
  HardwareCapabilitiesCallback secondary_callback = base::BindRepeating(
      [](int* calls, HardwareCapabilities hc) {
        (*calls)++;
        EXPECT_EQ(hc.num_overlay_capable_planes, 2);
      },
      &secondary_calls);
  manager.StartObservingHardwareCapabilities(kSecondaryWidget,
                                             secondary_callback);
  // Only the secondary callback should be called.
  EXPECT_EQ(primary_calls, 2);
  EXPECT_EQ(secondary_calls, 1);

  manager.DisplaysConfigured();

  // Both callbacks are called.
  EXPECT_EQ(primary_calls, 3);
  EXPECT_EQ(secondary_calls, 2);

  manager.StopObservingHardwareCapabilities(kPrimaryWidget);
  manager.DisplaysConfigured();
  manager.DisplaysConfigured();

  // The primary callback won't be called anymore.
  EXPECT_EQ(primary_calls, 3);
  EXPECT_EQ(secondary_calls, 4);
}

TEST(DrmOverlayManagerTest, SingleClipRectUnderlaySupport) {
  TestDrmOverlayManager manager;

  // Candidates for output surface and underlay quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), -1)};

  // Set a clip rect that imposes a restriction on |display_rect|.
  candidates[1].clip_rect = gfx::Rect(10, 10, 15, 15);

  for (int i = 0; i < 4; i++)
    manager.CheckOverlaySupport(&candidates, kPrimaryWidget);

  EXPECT_EQ(manager.requests().size(), 1u);
  EXPECT_TRUE(manager.requests()[0][1].overlay_handled);

  manager.requests().clear();
  // Now make the overlay candidate a single-on-top overlay. Single-on-top
  // overlays with restrictive clip rects are not supported.
  candidates[1].plane_z_order = 1;
  for (int i = 0; i < 4; i++)
    manager.CheckOverlaySupport(&candidates, kPrimaryWidget);

  EXPECT_EQ(manager.requests().size(), 1u);
  EXPECT_FALSE(manager.requests()[0][1].overlay_handled);
}

TEST(DrmOverlayManagerTest, MultiClipRectUnderlaySupport) {
  TestDrmOverlayManager manager;

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
    manager.CheckOverlaySupport(&candidates, kPrimaryWidget);

  EXPECT_EQ(manager.requests().size(), 1u);
  EXPECT_FALSE(manager.requests()[0][1].overlay_handled);
  EXPECT_TRUE(manager.requests()[0][2].overlay_handled);

  manager.requests().clear();
  // Now remove the clipping constraint on the -1 underlay which should allow
  // the -2 underlay to be handled.
  candidates[2].clip_rect = gfx::Rect(20, 20, 50, 50);
  for (int i = 0; i < 4; i++)
    manager.CheckOverlaySupport(&candidates, kPrimaryWidget);

  EXPECT_EQ(manager.requests().size(), 1u);
  EXPECT_TRUE(manager.requests()[0][1].overlay_handled);
  EXPECT_TRUE(manager.requests()[0][2].overlay_handled);
}

}  // namespace ui
