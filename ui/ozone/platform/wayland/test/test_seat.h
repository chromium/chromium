// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SEAT_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SEAT_H_

#include <wayland-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

extern const struct wl_seat_interface kTestSeatImpl;

class MockPointer;
class TestKeyboard;
class TestTouch;

// Manages a global wl_seat object.
// A seat groups keyboard, pointer, and touch devices.  This object is
// published as a global during start up, or when such a device is hot plugged.
// A seat typically has a pointer and maintains a keyboard focus and a pointer
// focus.
// https://people.freedesktop.org/~whot/wayland-doxygen/wayland/Server/structwl__seat__interface.html
class TestSeat : public GlobalObject {
 public:
  TestSeat();

  TestSeat(const TestSeat&) = delete;
  TestSeat& operator=(const TestSeat&) = delete;

  ~TestSeat() override;

  void set_pointer(MockPointer* pointer) { pointer_ = pointer; }
  MockPointer* pointer() const { return pointer_; }

  void set_keyboard(TestKeyboard* keyboard) { keyboard_ = keyboard; }
  TestKeyboard* keyboard() const { return keyboard_; }

  void set_touch(TestTouch* touch) { touch_ = touch; }
  TestTouch* touch() const { return touch_; }

 private:
  raw_ptr<MockPointer, DanglingUntriaged> pointer_;
  raw_ptr<TestKeyboard, DanglingUntriaged> keyboard_;
  raw_ptr<TestTouch, DanglingUntriaged> touch_;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SEAT_H_
