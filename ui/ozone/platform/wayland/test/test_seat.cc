// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_seat.h"

#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"

namespace wl {

namespace {

constexpr uint32_t kSeatVersion = 4;

void GetPointer(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* pointer_resource = CreateResourceWithImpl<MockPointer>(
      client, &wl_pointer_interface, wl_resource_get_version(resource),
      &kMockPointerImpl, id);
  GetUserDataAs<TestSeat>(resource)->set_pointer(
      GetUserDataAs<MockPointer>(pointer_resource));
}

void GetKeyboard(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* keyboard_resource = CreateResourceWithImpl<TestKeyboard>(
      client, &wl_keyboard_interface, wl_resource_get_version(resource),
      &kTestKeyboardImpl, id);
  GetUserDataAs<TestSeat>(resource)->set_keyboard(
      GetUserDataAs<TestKeyboard>(keyboard_resource));
}

void GetTouch(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* touch_resource = CreateResourceWithImpl<TestTouch>(
      client, &wl_touch_interface, wl_resource_get_version(resource),
      &kTestTouchImpl, id);
  GetUserDataAs<TestSeat>(resource)->set_touch(
      GetUserDataAs<TestTouch>(touch_resource));
}

}  // namespace

const struct wl_seat_interface kTestSeatImpl = {
    &GetPointer,       // get_pointer
    &GetKeyboard,      // get_keyboard
    &GetTouch,         // get_touch,
    &DestroyResource,  // release
};

TestSeat::TestSeat()
    : GlobalObject(&wl_seat_interface, &kTestSeatImpl, kSeatVersion) {}

TestSeat::~TestSeat() {}

}  // namespace wl
