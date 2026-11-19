// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZXDG_DECORATION_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZXDG_DECORATION_MANAGER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

// Manages the zxdg_decoration_manager_v1 global. Like some compositors (e.g.
// Sway for tiled windows), it can force a decoration mode regardless of the
// mode the client requests.
class TestZxdgDecorationManagerV1 : public GlobalObject {
 public:
  TestZxdgDecorationManagerV1();
  TestZxdgDecorationManagerV1(const TestZxdgDecorationManagerV1&) = delete;
  TestZxdgDecorationManagerV1& operator=(const TestZxdgDecorationManagerV1&) =
      delete;
  ~TestZxdgDecorationManagerV1() override;

  // The mode sent in zxdg_toplevel_decoration_v1.configure in response to
  // every set_mode request. 0 means the requested mode is used.
  void set_forced_mode(uint32_t mode) { forced_mode_ = mode; }
  uint32_t forced_mode() const { return forced_mode_; }

  // Modes requested by the client with zxdg_toplevel_decoration_v1.set_mode,
  // in order.
  std::vector<uint32_t>& requested_modes() { return requested_modes_; }

  // When set, every set_mode request is also answered with an
  // xdg_toplevel.configure of this size and an xdg_surface.configure, like
  // compositors do for mapped windows.
  void set_toplevel_configure_size(std::optional<gfx::Size> size) {
    toplevel_configure_size_ = size;
  }
  const std::optional<gfx::Size>& toplevel_configure_size() const {
    return toplevel_configure_size_;
  }

 private:
  uint32_t forced_mode_ = 0;
  std::optional<gfx::Size> toplevel_configure_size_;
  std::vector<uint32_t> requested_modes_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZXDG_DECORATION_MANAGER_H_
