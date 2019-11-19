// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_SURFACE_H_

#include <memory>
#include <utility>

#include <xdg-shell-unstable-v5-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_popup.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct xdg_surface_interface kMockXdgSurfaceImpl;
extern const struct zxdg_surface_v6_interface kMockZxdgSurfaceV6Impl;
extern const struct zxdg_toplevel_v6_interface kMockZxdgToplevelV6Impl;

class MockXdgTopLevel;

// Manage xdg_surface, zxdg_surface_v6 and zxdg_toplevel for providing desktop
// UI.
class MockXdgSurface : public ServerObject {
 public:
  MockXdgSurface(wl_resource* resource, const void* implementation);
  ~MockXdgSurface() override;

  // These mock methods are shared between xdg_surface and zxdg_toplevel
  // surface.
  MOCK_METHOD1(SetParent, void(wl_resource* parent));
  MOCK_METHOD1(SetTitle, void(const char* title));
  MOCK_METHOD1(SetAppId, void(const char* app_id));
  MOCK_METHOD1(Move, void(uint32_t serial));
  MOCK_METHOD2(Resize, void(uint32_t serial, uint32_t edges));
  MOCK_METHOD1(AckConfigure, void(uint32_t serial));
  MOCK_METHOD4(SetWindowGeometry,
               void(int32_t x, int32_t y, int32_t width, int32_t height));
  MOCK_METHOD0(SetMaximized, void());
  MOCK_METHOD0(UnsetMaximized, void());
  MOCK_METHOD0(SetFullscreen, void());
  MOCK_METHOD0(UnsetFullscreen, void());
  MOCK_METHOD0(SetMinimized, void());

  void set_xdg_toplevel(std::unique_ptr<MockXdgTopLevel> xdg_toplevel) {
    xdg_toplevel_ = std::move(xdg_toplevel);
  }
  MockXdgTopLevel* xdg_toplevel() const { return xdg_toplevel_.get(); }

  void set_xdg_popup(std::unique_ptr<MockXdgPopup> xdg_popup) {
    xdg_popup_ = std::move(xdg_popup);
  }
  MockXdgPopup* xdg_popup() const { return xdg_popup_.get(); }

 private:
  // Used when xdg v6 is used.
  std::unique_ptr<MockXdgTopLevel> xdg_toplevel_;

  std::unique_ptr<MockXdgPopup> xdg_popup_;

  DISALLOW_COPY_AND_ASSIGN(MockXdgSurface);
};

// Manage zxdg_toplevel for providing desktop UI.
class MockXdgTopLevel : public MockXdgSurface {
 public:
  explicit MockXdgTopLevel(wl_resource* resource);
  ~MockXdgTopLevel() override;

  // TODO(msisov): mock other zxdg_toplevel specific methods once
  // implementation
  // is done. example: MOCK_METHOD2(SetMaxSize, void(int32_t width, int32_t
  // height());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXdgTopLevel);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_SURFACE_H_
