// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zcr_stylus.h"

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_zcr_pointer_stylus.h"
#include "ui/ozone/platform/wayland/test/test_zcr_touch_stylus.h"

namespace wl {

namespace {

constexpr uint32_t kZcrStylusVersion = 2;

void GetTouchStylus(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    wl_resource* touch_resource) {
  wl_resource* touch_stylus_resource =
      CreateResourceWithImpl<TestZcrTouchStylus>(
          client, &zcr_touch_stylus_v2_interface,
          wl_resource_get_version(resource), &kTestZcrTouchStylusImpl, id);
  GetUserDataAs<TestTouch>(touch_resource)
      ->set_touch_stylus(
          GetUserDataAs<TestZcrTouchStylus>(touch_stylus_resource));
}

void GetPointerStylus(wl_client* client,
                      wl_resource* resource,
                      uint32_t id,
                      wl_resource* pointer_resource) {
  wl_resource* pointer_stylus_resource =
      CreateResourceWithImpl<TestZcrPointerStylus>(
          client, &zcr_pointer_stylus_v2_interface,
          wl_resource_get_version(resource), &kTestZcrPointerStylusImpl, id);
  GetUserDataAs<MockPointer>(pointer_resource)
      ->set_pointer_stylus(
          GetUserDataAs<TestZcrPointerStylus>(pointer_stylus_resource));
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
