// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_XDG_POPUP_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_XDG_POPUP_H_

#include <utility>

#include <xdg-shell-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_positioner.h"

struct wl_resource;

namespace wl {

extern const struct xdg_popup_interface kXdgPopupImpl;
extern const struct zxdg_popup_v6_interface kZxdgPopupV6Impl;

class TestXdgPopup : public ServerObject {
 public:
  TestXdgPopup(wl_resource* resource, wl_resource* surface);
  TestXdgPopup(const TestXdgPopup&) = delete;
  TestXdgPopup& operator=(const TestXdgPopup&) = delete;
  ~TestXdgPopup() override;

  struct TestPositioner::PopupPosition position() const {
    return position_;
  }
  void set_position(struct TestPositioner::PopupPosition position) {
    position_ = std::move(position);
  }

  // Returns and stores the serial used for grab.
  uint32_t grab_serial() const { return grab_serial_; }
  void set_grab_serial(uint32_t serial) { grab_serial_ = serial; }

  gfx::Rect anchor_rect() const { return position_.anchor_rect; }
  gfx::Size size() const { return position_.size; }
  uint32_t anchor() const { return position_.anchor; }
  uint32_t gravity() const { return position_.gravity; }
  uint32_t constraint_adjustment() const {
    return position_.constraint_adjustment;
  }

 private:
  struct TestPositioner::PopupPosition position_;

  // Ground surface for this popup.
  raw_ptr<wl_resource> surface_ = nullptr;

  uint32_t grab_serial_ = 0;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_XDG_POPUP_H_
