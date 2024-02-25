// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_MANAGER_V2_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_MANAGER_V2_H_

#include "ui/ozone/platform/wayland/test/global_object.h"

namespace wl {

class TestOutput;
struct TestOutputMetrics;

class TestZAuraOutputManagerV2 : public GlobalObject {
 public:
  TestZAuraOutputManagerV2();
  TestZAuraOutputManagerV2(const TestZAuraOutputManagerV2&) = delete;
  TestZAuraOutputManagerV2& operator=(const TestZAuraOutputManagerV2&) = delete;
  ~TestZAuraOutputManagerV2() override;

  // Sends all testing output metrics for this output via the
  // aura_output_manager_v2 protocol. Also sends the done event if
  // `flush_on_send_metrics_` is true.
  void SendOutputMetrics(TestOutput* test_output,
                         const TestOutputMetrics& metrics);

  // Sends the protocol's done event, signalling the end of the current output
  // configuration change transaction.
  void SendDone();

  // Sends the activated event for the given output.
  void SendActivated(TestOutput* test_output);

  // Called after a test output has been destroyed. The test manager will send
  // a done event if `send_done_on_config_change_` is true.
  void OnTestOutputGlobalDestroy(TestOutput* test_output);

  void set_send_done_on_config_change(bool send_done_on_config_change) {
    send_done_on_config_change_ = send_done_on_config_change;
  }

 private:
  // Controls whether an event that is part of an output configuration change
  // will trigger an implicit done event.
  bool send_done_on_config_change_ = true;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_ZAURA_OUTPUT_MANAGER_V2_H_
