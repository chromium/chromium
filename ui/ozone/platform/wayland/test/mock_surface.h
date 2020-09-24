// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_SURFACE_H_

#include <memory>
#include <utility>

#include <wayland-server-protocol.h>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_subsurface.h"
#include "ui/ozone/platform/wayland/test/test_viewport.h"
#include "ui/ozone/platform/wayland/test/test_xdg_popup.h"

struct wl_resource;

namespace wl {

extern const struct wl_surface_interface kMockSurfaceImpl;

// Manage client surface
class MockSurface : public ServerObject {
 public:
  explicit MockSurface(wl_resource* resource);
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
  MOCK_METHOD4(DamageBuffer,
               void(int32_t x, int32_t y, int32_t width, int32_t height));

  void set_xdg_surface(MockXdgSurface* xdg_surface) {
    xdg_surface_ = xdg_surface;
  }
  MockXdgSurface* xdg_surface() const { return xdg_surface_; }

  void set_sub_surface(TestSubSurface* sub_surface) {
    sub_surface_ = sub_surface;
  }
  TestSubSurface* sub_surface() const { return sub_surface_; }

  void set_viewport(TestViewport* viewport) { viewport_ = viewport; }
  TestViewport* viewport() { return viewport_; }

  void set_frame_callback(wl_resource* callback_resource) {
    DCHECK(!frame_callback_);
    frame_callback_ = callback_resource;
  }

  wl_resource* attached_buffer() const { return attached_buffer_; }
  wl_resource* prev_attached_buffer() const { return prev_attached_buffer_; }

  bool has_role() const { return !!xdg_surface_ || !!sub_surface_; }

  void AttachNewBuffer(wl_resource* buffer_resource, int32_t x, int32_t y);
  void DestroyPrevAttachedBuffer();
  void ReleaseBuffer(wl_resource* buffer);
  void SendFrameCallback();

 private:
  MockXdgSurface* xdg_surface_ = nullptr;
  TestSubSurface* sub_surface_ = nullptr;
  TestViewport* viewport_ = nullptr;

  wl_resource* frame_callback_ = nullptr;

  wl_resource* attached_buffer_ = nullptr;
  wl_resource* prev_attached_buffer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockSurface);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_SURFACE_H_
