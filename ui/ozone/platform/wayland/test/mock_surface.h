// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_SURFACE_H_

#include <memory>
#include <utility>

#include <wayland-server-protocol-core.h>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_popup.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

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

  void set_xdg_surface(std::unique_ptr<MockXdgSurface> xdg_surface) {
    xdg_surface_ = std::move(xdg_surface);
  }
  MockXdgSurface* xdg_surface() const { return xdg_surface_.get(); }

  void set_xdg_popup(std::unique_ptr<MockXdgPopup> xdg_popup) {
    xdg_popup_ = std::move(xdg_popup);
  }
  MockXdgPopup* xdg_popup() const { return xdg_popup_.get(); }

  void set_frame_callback(wl_resource* callback_resource) {
    DCHECK(!frame_callback_);
    frame_callback_ = callback_resource;
  }

  void AttachNewBuffer(wl_resource* buffer_resource, int32_t x, int32_t y);
  void ReleasePrevAttachedBuffer();
  void SendFrameCallback();

 private:
  std::unique_ptr<MockXdgSurface> xdg_surface_;
  std::unique_ptr<MockXdgPopup> xdg_popup_;

  wl_resource* frame_callback_ = nullptr;

  wl_resource* attached_buffer_ = nullptr;
  wl_resource* prev_attached_buffer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockSurface);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_SURFACE_H_
