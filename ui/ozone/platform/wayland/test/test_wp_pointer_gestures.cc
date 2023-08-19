// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wp_pointer_gestures.h"

#include <pointer-gestures-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>

#include "base/logging.h"
#include "base/notreached.h"

namespace wl {

namespace {

const struct zwp_pointer_gesture_pinch_v1_interface kTestPinchImpl = {
    DestroyResource};
const struct zwp_pointer_gesture_hold_v1_interface kTestHoldImpl = {
    DestroyResource};

constexpr uint32_t kInterfaceVersion = 3;

}  // namespace

const struct zwp_pointer_gestures_v1_interface kInterfaceImpl = {
    TestWpPointerGestures::GetSwipeGesture,
    TestWpPointerGestures::GetPinchGesture,
    DestroyResource,
    TestWpPointerGestures::GetHoldGesture,
};

TestWpPointerGestures::TestWpPointerGestures()
    : GlobalObject(&zwp_pointer_gestures_v1_interface,
                   &kInterfaceImpl,
                   kInterfaceVersion) {}

TestWpPointerGestures::~TestWpPointerGestures() = default;

// static
void TestWpPointerGestures::GetSwipeGesture(struct wl_client* client,
                                            struct wl_resource* resource,
                                            uint32_t id,
                                            struct wl_resource* pointer) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void TestWpPointerGestures::GetPinchGesture(
    struct wl_client* client,
    struct wl_resource* pointer_gestures_resource,
    uint32_t id,
    struct wl_resource* pointer) {
  wl_resource* pinch_gesture_resource =
      CreateResourceWithImpl<TestPinchGesture>(
          client, &zwp_pointer_gesture_pinch_v1_interface, 1, &kTestPinchImpl,
          id);

  GetUserDataAs<TestWpPointerGestures>(pointer_gestures_resource)->pinch_ =
      GetUserDataAs<TestPinchGesture>(pinch_gesture_resource);
}

// static
void TestWpPointerGestures::GetHoldGesture(
    struct wl_client* client,
    struct wl_resource* pointer_gestures_resource,
    uint32_t id,
    struct wl_resource* pointer) {
  wl_resource* hold_gesture_resource = CreateResourceWithImpl<TestHoldGesture>(
      client, &zwp_pointer_gesture_hold_v1_interface, 3, &kTestHoldImpl, id);

  GetUserDataAs<TestWpPointerGestures>(pointer_gestures_resource)->hold_ =
      GetUserDataAs<TestHoldGesture>(hold_gesture_resource);
}

TestPinchGesture::TestPinchGesture(wl_resource* resource)
    : ServerObject(resource) {}

TestPinchGesture::~TestPinchGesture() = default;

TestHoldGesture::TestHoldGesture(wl_resource* resource)
    : ServerObject(resource) {}

TestHoldGesture::~TestHoldGesture() = default;

}  // namespace wl
