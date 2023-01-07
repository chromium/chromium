// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SURFACE_AUGMENTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SURFACE_AUGMENTER_H_

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

// Manage surface_augmenter object.
class TestSurfaceAugmenter : public GlobalObject {
 public:
  TestSurfaceAugmenter();
  ~TestSurfaceAugmenter() override;
  TestSurfaceAugmenter(const TestSurfaceAugmenter& rhs) = delete;
  TestSurfaceAugmenter& operator=(const TestSurfaceAugmenter& rhs) = delete;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SURFACE_AUGMENTER_H_
