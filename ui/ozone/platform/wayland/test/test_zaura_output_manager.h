// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_MANAGER_H_

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class TestZAuraOutputManager : public GlobalObject {
 public:
  TestZAuraOutputManager();
  TestZAuraOutputManager(const TestZAuraOutputManager&) = delete;
  TestZAuraOutputManager& operator=(const TestZAuraOutputManager&) = delete;
  ~TestZAuraOutputManager() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_MANAGER_H_
