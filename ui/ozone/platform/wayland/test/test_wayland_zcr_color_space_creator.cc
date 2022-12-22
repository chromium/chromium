// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wayland_zcr_color_space_creator.h"

namespace wl {

TestZcrColorSpaceCreatorV1::TestZcrColorSpaceCreatorV1(wl_resource* resource)
    : ServerObject(resource) {}

TestZcrColorSpaceCreatorV1::~TestZcrColorSpaceCreatorV1() = default;

}  // namespace wl