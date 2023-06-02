// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_POINTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_POINTER_H_

#include <wayland-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct wl_pointer_interface kMockPointerImpl;

class TestZcrPointerStylus;

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

  void set_pointer_stylus(TestZcrPointerStylus* pointer_stylus) {
    pointer_stylus_ = pointer_stylus;
  }
  TestZcrPointerStylus* pointer_stylus() const { return pointer_stylus_; }

 private:
  raw_ptr<TestZcrPointerStylus, DanglingUntriaged> pointer_stylus_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_POINTER_H_
