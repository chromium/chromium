// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_touch.h"

namespace wl {

const struct wl_pointer_interface kTestTouchImpl = {
    nullptr,           // set_cursor
    &DestroyResource,  // release
};

TestTouch::TestTouch(wl_resource* resource) : ServerObject(resource) {}

TestTouch::~TestTouch() = default;

}  // namespace wl
