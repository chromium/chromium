// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_POINTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_POINTER_H_

#include <wayland-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

class MockWpCursorShapeDevice;

extern const struct wl_pointer_interface kMockPointerImpl;

class MockPointer : public ServerObject {
 public:
  explicit MockPointer(wl_resource* resource);

  MockPointer(const MockPointer&) = delete;
  MockPointer& operator=(const MockPointer&) = delete;

  ~MockPointer() override;

  MOCK_METHOD3(SetCursor,
               void(wl_resource* surface_resource,
                    int32_t hotspot_x,
                    int32_t hotspot_y));

  // The wp_cursor_shape_device_v1 the client created for this pointer, if any.
  MockWpCursorShapeDevice* cursor_shape_device() const {
    return cursor_shape_device_;
  }
  void set_cursor_shape_device(MockWpCursorShapeDevice* device) {
    cursor_shape_device_ = device;
  }

  base::WeakPtr<MockPointer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<MockWpCursorShapeDevice> cursor_shape_device_ = nullptr;
  base::WeakPtrFactory<MockPointer> weak_ptr_factory_{this};
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_POINTER_H_
