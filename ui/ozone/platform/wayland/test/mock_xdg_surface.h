// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_SURFACE_H_

#include <memory>
#include <utility>

#include <xdg-shell-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_xdg_popup.h"

struct wl_resource;

namespace wl {

extern const struct xdg_surface_interface kMockXdgSurfaceImpl;
extern const struct xdg_toplevel_interface kMockXdgToplevelImpl;
extern const struct zxdg_surface_v6_interface kMockZxdgSurfaceV6Impl;
extern const struct zxdg_toplevel_v6_interface kMockZxdgToplevelV6Impl;

class MockXdgTopLevel;
class TestZAuraToplevel;

// Manage xdg_surface, zxdg_surface_v6 and zxdg_toplevel for providing desktop
// UI.
class MockXdgSurface : public ServerObject {
 public:
  MockXdgSurface(wl_resource* resource, wl_resource* surface);

  MockXdgSurface(const MockXdgSurface&) = delete;
  MockXdgSurface& operator=(const MockXdgSurface&) = delete;

  ~MockXdgSurface() override;

  MOCK_METHOD1(AckConfigure, void(uint32_t serial));
  MOCK_METHOD1(SetWindowGeometry, void(const gfx::Rect&));

  void set_xdg_toplevel(std::unique_ptr<MockXdgTopLevel> xdg_toplevel) {
    xdg_toplevel_ = std::move(xdg_toplevel);
  }
  MockXdgTopLevel* xdg_toplevel() const { return xdg_toplevel_.get(); }

  void set_xdg_popup(TestXdgPopup* xdg_popup) { xdg_popup_ = xdg_popup; }
  TestXdgPopup* xdg_popup() const { return xdg_popup_; }

 private:
  // Has either toplevel role..
  std::unique_ptr<MockXdgTopLevel> xdg_toplevel_;
  // Or popup role.
  raw_ptr<TestXdgPopup, DanglingUntriaged> xdg_popup_ = nullptr;

  // MockSurface that is the ground for this xdg_surface.
  raw_ptr<wl_resource> surface_ = nullptr;
};

// Manage zxdg_toplevel for providing desktop UI.
class MockXdgTopLevel : public ServerObject {
 public:
  MockXdgTopLevel(wl_resource* resource, const void* implementation);

  MockXdgTopLevel(const MockXdgTopLevel&) = delete;
  MockXdgTopLevel& operator=(const MockXdgTopLevel&) = delete;

  ~MockXdgTopLevel() override;

  MOCK_METHOD1(SetParent, void(wl_resource* parent));
  MOCK_METHOD1(SetTitle, void(const std::string& title));
  MOCK_METHOD1(SetAppId, void(const char* app_id));
  MOCK_METHOD1(Move, void(uint32_t serial));
  MOCK_METHOD2(Resize, void(uint32_t serial, uint32_t edges));
  MOCK_METHOD0(SetMaximized, void());
  MOCK_METHOD0(UnsetMaximized, void());
  MOCK_METHOD0(SetFullscreen, void());
  MOCK_METHOD0(UnsetFullscreen, void());
  MOCK_METHOD0(SetMinimized, void());
  MOCK_METHOD2(SetMaxSize, void(int32_t width, int32_t height));
  MOCK_METHOD2(SetMinSize, void(int32_t width, int32_t height));

  const std::string& app_id() const { return app_id_; }
  void set_app_id(const char* app_id) { app_id_ = std::string(app_id); }

  std::string title() const { return title_; }
  void set_title(const char* title) { title_ = std::string(title); }

  const gfx::Size& min_size() const { return min_size_; }
  void set_min_size(const gfx::Size& min_size) { min_size_ = min_size; }

  const gfx::Size& max_size() const { return max_size_; }
  void set_max_size(const gfx::Size& max_size) { max_size_ = max_size; }

  TestZAuraToplevel* zaura_toplevel() { return zaura_toplevel_; }
  void set_zaura_toplevel(TestZAuraToplevel* zaura_toplevel) {
    zaura_toplevel_ = zaura_toplevel;
  }

 private:
  raw_ptr<TestZAuraToplevel, DanglingUntriaged> zaura_toplevel_ = nullptr;

  gfx::Size min_size_;
  gfx::Size max_size_;

  std::string title_;
  std::string app_id_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_SURFACE_H_
