// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WP_POINTER_GESTURES_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WP_POINTER_GESTURES_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_client;
struct wl_resource;

namespace wl {

class TestPinchGesture : public ServerObject {
 public:
  explicit TestPinchGesture(wl_resource* resource);
  TestPinchGesture(const TestPinchGesture&) = delete;
  TestPinchGesture& operator=(const TestPinchGesture&) = delete;
  ~TestPinchGesture() override;
};

class TestHoldGesture : public ServerObject {
 public:
  explicit TestHoldGesture(wl_resource* resource);
  TestHoldGesture(const TestHoldGesture&) = delete;
  TestHoldGesture& operator=(const TestHoldGesture&) = delete;
  ~TestHoldGesture() override;
};

// Manage zwp_pointer_gestures_v1 object.
class TestWpPointerGestures : public GlobalObject {
 public:
  TestWpPointerGestures();
  TestWpPointerGestures(const TestWpPointerGestures&) = delete;
  TestWpPointerGestures& operator=(const TestWpPointerGestures&) = delete;
  ~TestWpPointerGestures() override;

  TestPinchGesture* pinch() const { return pinch_; }

  TestHoldGesture* hold() const { return hold_; }

  void set_pinch(TestPinchGesture* pinch) { pinch_ = pinch; }
  void set_hold(TestHoldGesture* hold) { hold_ = hold; }

  base::WeakPtr<TestWpPointerGestures> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  static void GetSwipeGesture(struct wl_client* client,
                              struct wl_resource* resource,
                              uint32_t id,
                              struct wl_resource* pointer);

  static void GetPinchGesture(struct wl_client* client,
                              struct wl_resource* pointer_gestures_resource,
                              uint32_t id,
                              struct wl_resource* pointer);

  static void GetHoldGesture(struct wl_client* client,
                             struct wl_resource* pointer_gestures_resource,
                             uint32_t id,
                             struct wl_resource* pointer);

 private:
  raw_ptr<TestPinchGesture> pinch_;
  raw_ptr<TestHoldGesture> hold_;
  base::WeakPtrFactory<TestWpPointerGestures> weak_ptr_factory_{this};
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_WP_POINTER_GESTURES_H_
