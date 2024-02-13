// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_MANAGER_H_

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class TestOutput;
struct TestOutputMetrics;

class TestZAuraOutputManager : public GlobalObject {
 public:
  TestZAuraOutputManager();
  TestZAuraOutputManager(const TestZAuraOutputManager&) = delete;
  TestZAuraOutputManager& operator=(const TestZAuraOutputManager&) = delete;
  ~TestZAuraOutputManager() override;

  // Propagates events for metrics to bound clients for the output.
  void SendOutputMetrics(TestOutput* test_output,
                         const TestOutputMetrics& metrics);

  // Sends the activated event for the given output.
  void SendActivated(TestOutput* test_output);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_MANAGER_H_
