// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_FRACTIONAL_SCALE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_FRACTIONAL_SCALE_H_

#include "base/memory/raw_ptr.h"
#include "ui/ozone/platform/wayland/test/global_object.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

class TestFractionalScaleManager : public GlobalObject {
 public:
  TestFractionalScaleManager();
  ~TestFractionalScaleManager() override;

  TestFractionalScaleManager(const TestFractionalScaleManager& rhs) = delete;
  TestFractionalScaleManager& operator=(const TestFractionalScaleManager& rhs) =
      delete;
};

class TestFractionalScale : public ServerObject {
 public:
  TestFractionalScale(wl_resource* resource, wl_resource* surface);
  ~TestFractionalScale() override;

  TestFractionalScale(const TestFractionalScale&) = delete;
  TestFractionalScale& operator=(const TestFractionalScale&) = delete;

  // Sends `wp_fractional_scale_v1::preferred_scale` event with the integral
  // value corresponding to `scale`.
  void SendPreferredScale(float scale);

 private:
  raw_ptr<wl_resource> surface_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_FRACTIONAL_SCALE_H_
