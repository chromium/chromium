// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zcr_touch_stylus.h"

namespace wl {

const struct zcr_touch_stylus_v2_interface kTestZcrTouchStylusImpl = {
    &DestroyResource,  // destroy
};

TestZcrTouchStylus::TestZcrTouchStylus(wl_resource* resource)
    : ServerObject(resource) {}

TestZcrTouchStylus::~TestZcrTouchStylus() = default;

}  // namespace wl
