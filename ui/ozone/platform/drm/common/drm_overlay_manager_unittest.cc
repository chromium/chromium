// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/drm_overlay_manager.h"

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
  TestDrmOverlayManager() = default;
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

}  // namespace ui
