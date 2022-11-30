// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_AUGMENTED_SUBSURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_AUGMENTED_SUBSURFACE_H_

#include <surface-augmenter-server-protocol.h>

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

namespace wl {

extern const struct augmented_sub_surface_interface
    kTestAugmentedSubSurfaceImpl;

// Manage surface_augmenter object.
class TestAugmentedSubSurface : public ServerObject {
 public:
  TestAugmentedSubSurface(wl_resource* resource, wl_resource* sub_surface);
  ~TestAugmentedSubSurface() override;
  TestAugmentedSubSurface(const TestAugmentedSubSurface& rhs) = delete;
  TestAugmentedSubSurface& operator=(const TestAugmentedSubSurface& rhs) =
      delete;

  wl_resource* sub_surface() const { return sub_surface_; }

 private:
  // Subsurface resource that is the ground for this augmented surface.
  raw_ptr<wl_resource> sub_surface_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_AUGMENTED_SUBSURFACE_H_
