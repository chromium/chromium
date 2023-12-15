// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_keyboard.h"

namespace wl {

const struct wl_keyboard_interface kTestKeyboardImpl = {
    &DestroyResource,  // release
};

TestKeyboard::TestKeyboard(wl_resource* resource) : ServerObject(resource) {}

TestKeyboard::~TestKeyboard() = default;

}  // namespace wl
