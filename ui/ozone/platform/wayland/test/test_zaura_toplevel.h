// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_TOPLEVEL_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_TOPLEVEL_H_

#include <aura-shell-server-protocol.h>

#include <optional>

#include "base/functional/callback.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_region.h"

namespace wl {

extern const struct zaura_toplevel_interface kTestZAuraToplevelImpl;

// Manages zaura_toplevel object.
class TestZAuraToplevel : public ServerObject {
 public:
  explicit TestZAuraToplevel(wl_resource* resource);

  TestZAuraToplevel(const TestZAuraToplevel&) = delete;
  TestZAuraToplevel& operator=(const TestZAuraToplevel&) = delete;

  ~TestZAuraToplevel() override;

  // TODO(tluk): `shape_` should really not have a public setter method, the
  // member should instead only be set by the handler that responds to
  // aura_toplevel.set_shape events from the server.
  const std::optional<TestRegion>& shape() const { return shape_; }
  void set_shape(const std::optional<TestRegion>& shape) { shape_ = shape; }

  int top_inset() const { return top_inset_; }
  void set_top_inset(int top_inset) { top_inset_ = top_inset; }

  using AckRotateFocusCallback =
      base::RepeatingCallback<void(uint32_t serial, uint32_t handled)>;
  void set_ack_rotate_focus_callback(const AckRotateFocusCallback cb) {
    ack_rotate_focus_callback_ = cb;
  }
  AckRotateFocusCallback ack_rotate_focus_callback() {
    return ack_rotate_focus_callback_;
  }

  void set_can_maximize(bool can_maximize) { can_maximize_ = can_maximize; }
  bool can_maximize() const { return can_maximize_; }

  void set_can_fullscreen(bool can_fullscreen) {
    can_fullscreen_ = can_fullscreen;
  }
  bool can_fullscreen() const { return can_fullscreen_; }

  using SetUnsetFloatCallback =
      base::RepeatingCallback<void(bool floated, uint32_t start_location)>;
  void set_set_unset_float_callback(const SetUnsetFloatCallback cb) {
    set_unset_float_callback_ = cb;
  }
  SetUnsetFloatCallback set_unset_float_callback() {
    return set_unset_float_callback_;
  }

 private:
  std::optional<TestRegion> shape_;
  int top_inset_;
  AckRotateFocusCallback ack_rotate_focus_callback_;
  bool can_maximize_ = false;
  bool can_fullscreen_ = false;
  SetUnsetFloatCallback set_unset_float_callback_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_TOPLEVEL_H_
