// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_SURFACE_H_

#include <linux-explicit-synchronization-unstable-v1-server-protocol.h>
#include <wayland-server-protocol.h>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_alpha_blending.h"
#include "ui/ozone/platform/wayland/test/test_augmented_surface.h"
#include "ui/ozone/platform/wayland/test/test_fractional_scale.h"
#include "ui/ozone/platform/wayland/test/test_overlay_prioritized_surface.h"
#include "ui/ozone/platform/wayland/test/test_subsurface.h"
#include "ui/ozone/platform/wayland/test/test_viewport.h"
#include "ui/ozone/platform/wayland/test/test_xdg_popup.h"

struct wl_resource;

namespace wl {

extern const struct wl_surface_interface kMockSurfaceImpl;
extern const struct zwp_linux_surface_synchronization_v1_interface
    kMockZwpLinuxSurfaceSynchronizationImpl;

// Manage client surface
class MockSurface : public ServerObject {
 public:
  explicit MockSurface(wl_resource* resource);

  MockSurface(const MockSurface&) = delete;
  MockSurface& operator=(const MockSurface&) = delete;

  ~MockSurface() override;

  static MockSurface* FromResource(wl_resource* resource);

  MOCK_METHOD3(Attach, void(wl_resource* buffer, int32_t x, int32_t y));
  MOCK_METHOD1(SetOpaqueRegion, void(wl_resource* region));
  MOCK_METHOD1(SetInputRegion, void(wl_resource* region));
  MOCK_METHOD1(Frame, void(uint32_t callback));
  MOCK_METHOD4(Damage,
               void(int32_t x, int32_t y, int32_t width, int32_t height));
  MOCK_METHOD0(Commit, void());
  MOCK_METHOD1(SetBufferScale, void(int32_t scale));
  MOCK_METHOD1(SetBufferTransform, void(int32_t transform));
  MOCK_METHOD4(DamageBuffer,
               void(int32_t x, int32_t y, int32_t width, int32_t height));

  void set_xdg_surface(MockXdgSurface* xdg_surface) {
    xdg_surface_ = xdg_surface;
  }
  MockXdgSurface* xdg_surface() const { return xdg_surface_; }

  // Must be set iff this MockSurface has role of subsurface.
  void set_sub_surface(TestSubSurface* sub_surface) {
    sub_surface_ = sub_surface;
  }
  TestSubSurface* sub_surface() const { return sub_surface_; }

  void set_viewport(TestViewport* viewport) { viewport_ = viewport; }
  TestViewport* viewport() { return viewport_; }

  void set_fractional_scale(TestFractionalScale* fractional_scale) {
    fractional_scale_ = fractional_scale;
  }
  TestFractionalScale* fractional_scale() { return fractional_scale_; }

  void set_overlay_prioritized_surface(
      TestOverlayPrioritizedSurface* prioritized_surface) {
    prioritized_surface_ = prioritized_surface;
  }
  TestOverlayPrioritizedSurface* prioritized_surface() {
    return prioritized_surface_;
  }

  void set_augmented_surface(TestAugmentedSurface* augmented_surface) {
    augmented_surface_ = augmented_surface;
  }
  TestAugmentedSurface* augmented_surface() { return augmented_surface_; }

  void set_blending(TestAlphaBlending* blending) { blending_ = blending; }
  TestAlphaBlending* blending() { return blending_; }

  gfx::Rect opaque_region() const { return opaque_region_; }
  gfx::Rect input_region() const { return input_region_; }

  void set_frame_callback(wl_resource* callback_resource) {
    DCHECK(!frame_callback_);
    frame_callback_ = callback_resource;
  }

  void set_linux_buffer_release(wl_resource* buffer,
                                wl_resource* linux_buffer_release) {
    DCHECK(!linux_buffer_releases_.contains(buffer));
    linux_buffer_releases_.emplace(buffer, linux_buffer_release);
  }
  bool has_linux_buffer_release() const {
    return !linux_buffer_releases_.empty();
  }
  void ClearBufferReleases();

  wl_resource* attached_buffer() const { return attached_buffer_; }
  wl_resource* prev_attached_buffer() const { return prev_attached_buffer_; }

  bool has_role() const { return !!xdg_surface_ || !!sub_surface_; }

  void SetOpaqueRegionImpl(wl_resource* region);
  void SetInputRegionImpl(wl_resource* region);
  void AttachNewBuffer(wl_resource* buffer_resource, int32_t x, int32_t y);
  void DestroyPrevAttachedBuffer();
  void ReleaseBuffer(wl_resource* buffer);
  void ReleaseBufferFenced(wl_resource* buffer,
                           gfx::GpuFenceHandle release_fence);
  void SendFrameCallback();

  int32_t buffer_scale() const { return buffer_scale_; }
  void set_buffer_scale(int32_t buffer_scale) { buffer_scale_ = buffer_scale; }

 private:
  raw_ptr<MockXdgSurface, AcrossTasksDanglingUntriaged> xdg_surface_ = nullptr;
  raw_ptr<TestSubSurface, AcrossTasksDanglingUntriaged> sub_surface_ = nullptr;
  raw_ptr<TestViewport, AcrossTasksDanglingUntriaged> viewport_ = nullptr;
  raw_ptr<TestFractionalScale> fractional_scale_ = nullptr;
  raw_ptr<TestAlphaBlending, AcrossTasksDanglingUntriaged> blending_ = nullptr;
  raw_ptr<TestOverlayPrioritizedSurface, AcrossTasksDanglingUntriaged>
      prioritized_surface_ = nullptr;
  raw_ptr<TestAugmentedSurface, AcrossTasksDanglingUntriaged>
      augmented_surface_ = nullptr;
  gfx::Rect opaque_region_ = {-1, -1, 0, 0};
  gfx::Rect input_region_ = {-1, -1, 0, 0};

  raw_ptr<wl_resource, AcrossTasksDanglingUntriaged> frame_callback_ = nullptr;
  base::flat_map<wl_resource*, raw_ptr<wl_resource, CtnExperimental>>
      linux_buffer_releases_;

  raw_ptr<wl_resource, AcrossTasksDanglingUntriaged> attached_buffer_ = nullptr;
  raw_ptr<wl_resource, AcrossTasksDanglingUntriaged> prev_attached_buffer_ =
      nullptr;

  int32_t buffer_scale_ = -1;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_SURFACE_H_
