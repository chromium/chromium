// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_KEYBOARD_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_KEYBOARD_H_

#include <wayland-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct wl_keyboard_interface kTestKeyboardImpl;

class TestKeyboard : public ServerObject {
 public:
  explicit TestKeyboard(wl_resource* resource);

  TestKeyboard(const TestKeyboard&) = delete;
  TestKeyboard& operator=(const TestKeyboard&) = delete;

  ~TestKeyboard() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_KEYBOARD_H_
