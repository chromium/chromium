// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/mock_zcr_touch_stylus.h"

namespace wl {

const struct zcr_touch_stylus_v2_interface kMockZcrTouchStylusImpl = {
    &DestroyResource,  // destroy
};

MockZcrTouchStylus::MockZcrTouchStylus(wl_resource* resource)
    : ServerObject(resource) {}

MockZcrTouchStylus::~MockZcrTouchStylus() = default;

}  // namespace wl
