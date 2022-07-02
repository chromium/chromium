// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zcr_stylus.h"

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/mock_zcr_touch_stylus.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"

namespace wl {

namespace {

constexpr uint32_t kZcrStylusVersion = 2;

void GetTouchStylus(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    wl_resource* touch_resource) {
  wl_resource* touch_stylus_resource =
      CreateResourceWithImpl<MockZcrTouchStylus>(
          client, &zcr_touch_stylus_v2_interface,
          wl_resource_get_version(resource), &kMockZcrTouchStylusImpl, id);
  GetUserDataAs<TestTouch>(touch_resource)
      ->set_touch_stylus(
          GetUserDataAs<MockZcrTouchStylus>(touch_stylus_resource));
}

void GetPointerStylus(wl_client* client,
                      wl_resource* resource,
                      uint32_t id,
                      wl_resource* touch_resource) {
  NOTIMPLEMENTED();
}

const struct zcr_stylus_v2_interface kTestZcrStylusImpl = {&GetTouchStylus,
                                                           &GetPointerStylus};

}  // namespace

TestZcrStylus::TestZcrStylus()
    : GlobalObject(&zcr_stylus_v2_interface,
                   &kTestZcrStylusImpl,
                   kZcrStylusVersion) {}

TestZcrStylus::~TestZcrStylus() = default;

}  // namespace wl
