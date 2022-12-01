// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zcr_pointer_stylus.h"

namespace wl {

const struct zcr_pointer_stylus_v2_interface kTestZcrPointerStylusImpl = {
    &DestroyResource,  // destroy
};

TestZcrPointerStylus::TestZcrPointerStylus(wl_resource* resource)
    : ServerObject(resource) {}

TestZcrPointerStylus::~TestZcrPointerStylus() = default;

}  // namespace wl
