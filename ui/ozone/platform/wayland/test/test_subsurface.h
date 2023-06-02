// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SUBSURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SUBSURFACE_H_

#include <wayland-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

class TestAugmentedSubSurface;

extern const struct wl_subsurface_interface kTestSubSurfaceImpl;

class TestSubSurface : public ServerObject {
 public:
  explicit TestSubSurface(wl_resource* resource,
                          wl_resource* surface,
                          wl_resource* parent_resource);
  ~TestSubSurface() override;
  TestSubSurface(const TestSubSurface& rhs) = delete;
  TestSubSurface& operator=(const TestSubSurface& rhs) = delete;

  MOCK_METHOD1(PlaceAbove, void(wl_resource* reference_resource));
  MOCK_METHOD1(PlaceBelow, void(wl_resource* sibling_resource));
  MOCK_METHOD2(SetPosition, void(float x, float y));

  void SetPositionImpl(float x, float y);
  gfx::PointF position() const { return position_; }

  void set_sync(bool sync) { sync_ = sync; }
  bool sync() const { return sync_; }

  wl_resource* parent_resource() const { return parent_resource_; }

  void set_augmented_subsurface(TestAugmentedSubSurface* augmented_subsurface) {
    augmented_subsurface_ = augmented_subsurface;
  }
  TestAugmentedSubSurface* augmented_subsurface() const {
    return augmented_subsurface_;
  }

 private:
  gfx::PointF position_;
  bool sync_ = false;

  // Surface resource that is the ground for this subsurface.
  raw_ptr<wl_resource> surface_ = nullptr;

  // Parent surface resource.
  raw_ptr<wl_resource, DanglingUntriaged> parent_resource_ = nullptr;

  raw_ptr<TestAugmentedSubSurface, DanglingUntriaged> augmented_subsurface_ =
      nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SUBSURFACE_H_
