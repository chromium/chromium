// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/wayland/gpu/wayland_overlay_manager.h"

#include <drm_fourcc.h>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/overlay_surface_candidate.h"

namespace ui {
namespace {

constexpr gfx::AcceleratedWidget kPrimaryWidget = 1;
constexpr uint32_t kAugmentedSurfaceNotSupportedVersion = 0;

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

class WaylandOverlayManagerTest : public WaylandTest {
 public:
  WaylandOverlayManagerTest() = default;

  WaylandOverlayManagerTest(const WaylandOverlayManagerTest&) = delete;
  WaylandOverlayManagerTest& operator=(const WaylandOverlayManagerTest&) =
      delete;

  ~WaylandOverlayManagerTest() override = default;

  void SetUp() override {
    const base::flat_map<gfx::BufferFormat, std::vector<uint64_t>>
        kSupportedFormatsWithModifiers{
            {gfx::BufferFormat::YUV_420_BIPLANAR, {DRM_FORMAT_MOD_LINEAR}}};

    WaylandTest::SetUp();

    auto manager_ptr = connection_->buffer_manager_host()->BindInterface();
    buffer_manager_gpu_->Initialize(
        std::move(manager_ptr), kSupportedFormatsWithModifiers,
        /*supports_dma_buf=*/false,
        /*supports_viewporter=*/true,
        /*supports_acquire_fence=*/false,
        /*supports_overlays=*/true, kAugmentedSurfaceNotSupportedVersion,
        /*supports_single_pixel_buffer=*/true,
        /*server_version=*/{});

    // Wait until initialization and mojo calls go through.
    base::RunLoop().RunUntilIdle();
  }
};

TEST_P(WaylandOverlayManagerTest, MultipleOverlayCandidates) {
  // WaylandBufferManagerGpu manager_gpu;
  WaylandOverlayManager manager(buffer_manager_gpu_.get());

  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(10, 10, 20, 20), -2),
      CreateCandidate(gfx::Rect(30, 30, 10, 10), -1),
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(40, 40, 20, 20), 1),
      CreateCandidate(gfx::Rect(60, 60, 20, 20), 2)};

  // Submit a set of candidates that could potentially be displayed in an
  // overlay. All candidates should be marked as handled.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);
  EXPECT_TRUE(candidates[2].overlay_handled);
  EXPECT_TRUE(candidates[3].overlay_handled);
  EXPECT_TRUE(candidates[4].overlay_handled);
}

TEST_P(WaylandOverlayManagerTest, FormatSupportTest) {
  WaylandOverlayManager manager(buffer_manager_gpu_.get());

  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};
  candidates[1].format = gfx::BufferFormat::RGBX_8888;
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_FALSE(candidates[1].overlay_handled);
}

namespace {

void NonIntegerDisplayRectTestHelper(WaylandBufferManagerGpu* manager_gpu,
                                     bool is_context_delegated,
                                     bool expect_candidates_handled) {
  WaylandOverlayManager manager(manager_gpu);
  if (is_context_delegated)
    manager.SetContextDelegated();

  // Candidates for output surface and single-on-top quad.
  std::vector<OverlaySurfaceCandidate> candidates = {
      CreateCandidate(gfx::Rect(0, 0, 100, 100), 0),
      CreateCandidate(gfx::Rect(10, 10, 20, 20), 1)};

  // Submit a set of candidates that could potentially be displayed in an
  // overlay.
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_TRUE(candidates[1].overlay_handled);

  candidates[0].overlay_handled = false;
  candidates[1].overlay_handled = false;

  // Modify the display_rect for the second candidate so it's non-integer. We
  // will try to promote this to an overlay iff subpixel accurate position is
  // supported and overlay delegation is enabled.
  candidates[1].display_rect = gfx::RectF(9.4, 10.43, 20.11, 20.99);
  manager.CheckOverlaySupport(&candidates, kPrimaryWidget);
  EXPECT_TRUE(candidates[0].overlay_handled);
  EXPECT_EQ(expect_candidates_handled, candidates[1].overlay_handled);
}

}  // namespace

TEST_P(WaylandOverlayManagerTest, DoesNotSupportNonIntegerDisplayRect) {
  constexpr std::array<std::array<bool, 2>, 2> test_data = {
      {{false, false}, {true, false}}};
  for (const auto& data : test_data) {
    NonIntegerDisplayRectTestHelper(buffer_manager_gpu_.get(),
                                    data[0] /* is_delegated_context */,
                                    data[1] /* expect_candidates_handled */);
  }
}

TEST_P(WaylandOverlayManagerTest, SupportsNonIntegerDisplayRect) {
  // WaylandBufferManagerGpu manager_gpu;
  buffer_manager_gpu_->supports_subpixel_accurate_position_ = true;

  constexpr std::array<std::array<bool, 2>, 2> test_data = {
      {{false, false}, {true, false}}};
  for (const auto& data : test_data) {
    NonIntegerDisplayRectTestHelper(buffer_manager_gpu_.get(),
                                    data[0] /* is_delegated_context */,
                                    data[1] /* expect_candidates_handled */);
  }
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(WaylandOverlayManagerTest);

}  // namespace ui
