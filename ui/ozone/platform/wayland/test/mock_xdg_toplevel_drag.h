// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_TOPLEVEL_DRAG_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_TOPLEVEL_DRAG_H_

#include <xdg-toplevel-drag-v1-server-protocol.h>

#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

class MockXdgToplevelDragV1;

extern const struct xdg_toplevel_drag_manager_v1_interface
    kMockXdgToplevelDragManagerImpl;
extern const struct xdg_toplevel_drag_v1_interface kMockXdgToplevelDragImpl;

// Manage xdg_toplevel_drag_manager_v1 object.
class MockXdgToplevelDragManagerV1 : public GlobalObject {
 public:
  MockXdgToplevelDragManagerV1();

  MockXdgToplevelDragManagerV1(const MockXdgToplevelDragManagerV1&) = delete;
  MockXdgToplevelDragManagerV1& operator=(const MockXdgToplevelDragManagerV1&) =
      delete;

  ~MockXdgToplevelDragManagerV1() override;

  void set_drag(MockXdgToplevelDragV1* drag) { drag_ = drag; }
  MockXdgToplevelDragV1* drag() { return drag_; }

 private:
  raw_ptr<MockXdgToplevelDragV1> drag_ = nullptr;
};

// Manage xdg_toplevel_drag_v1 object.
class MockXdgToplevelDragV1 : public ServerObject {
 public:
  MockXdgToplevelDragV1(wl_resource* resource,
                        MockXdgToplevelDragManagerV1* global);

  MockXdgToplevelDragV1(const MockXdgToplevelDragV1&) = delete;
  MockXdgToplevelDragV1& operator=(const MockXdgToplevelDragV1&) = delete;

  ~MockXdgToplevelDragV1() override;

  // The offset passed in the latest xdg_toplevel_drag_v1.attach request, if
  // any.
  const std::optional<gfx::Vector2d>& attached_offset() const {
    return attached_offset_;
  }
  void set_attached_offset(const gfx::Vector2d& offset) {
    attached_offset_ = offset;
  }

 private:
  raw_ptr<MockXdgToplevelDragManagerV1> global_ = nullptr;

  std::optional<gfx::Vector2d> attached_offset_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_TOPLEVEL_DRAG_H_
