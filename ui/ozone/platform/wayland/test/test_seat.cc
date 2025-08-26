// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_seat.h"

#include "base/functional/bind.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"

namespace wl {

namespace {

constexpr uint32_t kSeatVersion = 4;

void GetPointer(wl_client* client, wl_resource* resource, uint32_t id) {
  // We need to pass a pointer to the MockPointer to the destruction callback.
  // That callback is passed to the method creating the MockPointer, so we use
  // this struct to work around "variable is uninitialized when used within its
  // own initialization" errors.
  struct DestroyStruct {
    base::WeakPtr<TestSeat> seat;
    raw_ptr<MockPointer> pointer;
  };

  auto* destroy_struct = new DestroyStruct;
  wl_resource* pointer_resource = CreateResourceWithImpl<MockPointer>(
      client, &wl_pointer_interface, wl_resource_get_version(resource),
      &kMockPointerImpl, id,
      base::BindOnce(
          [](DestroyStruct* d) {
            if (d->seat) {
              // Only reset if we haven't already set a different pointer.
              if (d->seat->pointer() == d->pointer) {
                d->seat->set_pointer(nullptr);
              }
            }
            delete d;
          },
          destroy_struct));
  destroy_struct->seat = GetUserDataAs<TestSeat>(resource)->GetWeakPtr();
  destroy_struct->pointer = GetUserDataAs<MockPointer>(pointer_resource);

  GetUserDataAs<TestSeat>(resource)->set_pointer(
      GetUserDataAs<MockPointer>(pointer_resource));
}

void GetKeyboard(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* keyboard_resource = CreateResourceWithImpl<TestKeyboard>(
      client, &wl_keyboard_interface, wl_resource_get_version(resource),
      &kTestKeyboardImpl, id,
      base::BindOnce(&TestSeat::set_keyboard,
                     GetUserDataAs<TestSeat>(resource)->GetWeakPtr(), nullptr));
  GetUserDataAs<TestSeat>(resource)->set_keyboard(
      GetUserDataAs<TestKeyboard>(keyboard_resource));
}

void GetTouch(wl_client* client, wl_resource* resource, uint32_t id) {
  // See `GetPointer()`.
  struct DestroyStruct {
    base::WeakPtr<TestSeat> seat;
    raw_ptr<TestTouch> touch;
  };

  auto* destroy_struct = new DestroyStruct;
  wl_resource* touch_resource = CreateResourceWithImpl<TestTouch>(
      client, &wl_touch_interface, wl_resource_get_version(resource),
      &kTestTouchImpl, id,
      base::BindOnce(
          [](DestroyStruct* d) {
            if (d->seat) {
              // Only reset if we haven't already set a different touch.
              if (d->seat->touch() == d->touch) {
                d->seat->set_touch(nullptr);
              }
            }
            delete d;
          },
          destroy_struct));
  destroy_struct->seat = GetUserDataAs<TestSeat>(resource)->GetWeakPtr();
  destroy_struct->touch = GetUserDataAs<TestTouch>(touch_resource);

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
