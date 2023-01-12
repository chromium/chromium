// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/numberpad_metrics.h"

#include <linux/input.h>

#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_converter_evdev_impl.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/event_factory_evdev.h"
#include "ui/events/ozone/evdev/numberpad_metrics.h"
#include "ui/events/ozone/evdev/testing/fake_cursor_delegate_evdev.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

namespace ui {

namespace {

using FeatureUsageEvent = ::ash::feature_usage::FeatureUsageMetrics::Event;

// TODO(b/202039817): Should not need internal detail of feature_usage_metrics
constexpr char kFeatureUsageMetricPrefix[] = "ChromeOS.FeatureUsage.";

// Utility to provide clear gtest caller information for nested
// expectations; requires scope_level_ variable in caller.
#define OUTERMOST_SCOPED_TRACE(code)                \
  do {                                              \
    if (scope_level_++ == 0) {                      \
      SCOPED_TRACE("Caller of failed expectation"); \
      code;                                         \
    } else {                                        \
      code;                                         \
    }                                               \
    scope_level_--;                                 \
  } while (0)

}  // namespace

class NumberpadMetricsTest : public ::testing::Test {
  // Structures for keyword parameters to check utilities.
  struct DynamicMetricsExpectations {
    int enter_keys = 0;
    int non_enter_keys = 0;
    int activations = 0;
    int cancellations = 0;
    bool eligible = false;
    bool enabled = false;
  };

  struct InternalMetricsExpectations {
    int enter_keys = 0;
    int non_enter_keys = 0;
    bool eligible = false;
    bool enabled = false;
  };

  struct ExternalMetricsExpectations {
    int enter_keys = 0;
    int non_enter_keys = 0;
    bool eligible = false;
    bool enabled = false;
  };

 public:
  NumberpadMetricsTest() = default;
  NumberpadMetricsTest(const NumberpadMetricsTest&) = delete;
  NumberpadMetricsTest& operator=(const NumberpadMetricsTest&) = delete;
  ~NumberpadMetricsTest() override = default;

  // NOTE: This is only creates a simple ui::InputDevice based on a device
  // capabilities report; it is not suitable for subclasses of ui::InputDevice.
  ui::InputDevice InputDeviceFromCapabilities(
      const ui::DeviceCapabilities& capabilities) {
    ui::EventDeviceInfo device_info = {};
    ui::CapabilitiesToDeviceInfo(capabilities, &device_info);

    device_id_++;

    return ui::InputDevice(device_id_, device_info.device_type(),
                           device_info.name(), device_info.phys(),
                           base::FilePath(capabilities.path),
                           device_info.vendor_id(), device_info.product_id(),
                           device_info.version());
  }

  // Utility routines for common expectations; each has a macro for invocation
  // that tracks the line number of the calling scope, which otherwise is not
  // available.

  // feature_usage_metrics testing utility: verify particular metric matches
  // expectations.
  // TODO(b/202039817): More precisely model eligibility and enabled buckets,
  // right now we just check them for 0 or >= 1 entries, rather than trying to
  // be precise.

#define EXPECT_METRIC(...) OUTERMOST_SCOPED_TRACE((ExpectMetric(__VA_ARGS__)))

  void ExpectMetric(const std::string& core_metric_name,
                    int success_count,
                    int failure_count,
                    bool eligible,
                    bool enabled) {
    std::string metric_name =
        std::string(kFeatureUsageMetricPrefix) + core_metric_name;

    EXPECT_EQ(
        histogram_tester_.GetBucketCount(
            metric_name, static_cast<int>(FeatureUsageEvent::kUsedWithSuccess)),
        success_count)
        << "Expected BucketCount == " << success_count << " for " << metric_name
        << ".kUsedWithSuccess";

    EXPECT_EQ(
        histogram_tester_.GetBucketCount(
            metric_name, static_cast<int>(FeatureUsageEvent::kUsedWithFailure)),
        failure_count)
        << "Expected BucketCount == " << success_count << " for " << metric_name
        << ".kUsedWithFailure";

    if (eligible) {
      EXPECT_GE(
          histogram_tester_.GetBucketCount(
              metric_name, static_cast<int>(FeatureUsageEvent::kEligible)),
          1)
          << "Expected BucketCount >= 1 for " << metric_name << ".kEligible";
    } else {
      EXPECT_EQ(
          histogram_tester_.GetBucketCount(
              metric_name, static_cast<int>(FeatureUsageEvent::kEligible)),
          0)
          << "Expected BucketCount == 0 for " << metric_name << ".kEligible";
    }

    if (enabled) {
      EXPECT_GE(histogram_tester_.GetBucketCount(
                    metric_name, static_cast<int>(FeatureUsageEvent::kEnabled)),
                1)
          << "Expected BucketCount >= 1 for " << metric_name << ".kEnabled";
    } else {
      EXPECT_EQ(histogram_tester_.GetBucketCount(
                    metric_name, static_cast<int>(FeatureUsageEvent::kEnabled)),
                0)
          << "Expected BucketCount == 0 for " << metric_name << ".kEnabled";
    }
  }

#define EXPECT_DYNAMIC_PRESENT(...) \
  OUTERMOST_SCOPED_TRACE((ExpectDynamicPresent(__VA_ARGS__)))

  void ExpectDynamicPresent(bool desired_state) {
    EXPECT_EQ(
        numberpad_metrics_.dynamic_activations_metrics_delegate_.IsEnabled(),
        desired_state);
    EXPECT_EQ(
        numberpad_metrics_.dynamic_activations_metrics_delegate_.IsEligible(),
        desired_state);
    EXPECT_EQ(
        numberpad_metrics_.dynamic_cancellations_metrics_delegate_.IsEligible(),
        desired_state);
    EXPECT_EQ(
        numberpad_metrics_.dynamic_cancellations_metrics_delegate_.IsEnabled(),
        desired_state);
    EXPECT_EQ(numberpad_metrics_.dynamic_enter_keystrokes_metrics_delegate_
                  .IsEligible(),
              desired_state);
    EXPECT_EQ(numberpad_metrics_.dynamic_enter_keystrokes_metrics_delegate_
                  .IsEnabled(),
              desired_state);
    EXPECT_EQ(numberpad_metrics_.dynamic_non_enter_keystrokes_metrics_delegate_
                  .IsEligible(),
              desired_state);
    EXPECT_EQ(numberpad_metrics_.dynamic_non_enter_keystrokes_metrics_delegate_
                  .IsEnabled(),
              desired_state);
  }

#define EXPECT_INTERNAL_PRESENT(...) \
  OUTERMOST_SCOPED_TRACE((ExpectInternalPresent(__VA_ARGS__)))

  void ExpectInternalPresent(bool desired_state) {
    EXPECT_EQ(numberpad_metrics_.internal_enter_keystrokes_metrics_delegate_
                  .IsEligible(),
              desired_state);
    EXPECT_EQ(numberpad_metrics_.internal_enter_keystrokes_metrics_delegate_
                  .IsEnabled(),
              desired_state);
    EXPECT_EQ(numberpad_metrics_.internal_non_enter_keystrokes_metrics_delegate_
                  .IsEligible(),
              desired_state);
    EXPECT_EQ(numberpad_metrics_.internal_non_enter_keystrokes_metrics_delegate_
                  .IsEnabled(),
              desired_state);
  }

#define EXPECT_EXTERNAL_PRESENT(...) \
  OUTERMOST_SCOPED_TRACE((ExpectExternalPresent(__VA_ARGS__)))

  void ExpectExternalPresent(bool desired_state) {
    // Note that external always should be eligible, only enabled varies.
    EXPECT_EQ(numberpad_metrics_.external_enter_keystrokes_metrics_delegate_
                  .IsEligible(),
              true);
    EXPECT_EQ(numberpad_metrics_.external_enter_keystrokes_metrics_delegate_
                  .IsEnabled(),
              desired_state);
    EXPECT_EQ(numberpad_metrics_.external_non_enter_keystrokes_metrics_delegate_
                  .IsEligible(),
              true);
    EXPECT_EQ(numberpad_metrics_.external_non_enter_keystrokes_metrics_delegate_
                  .IsEnabled(),
              desired_state);
  }

#define EXPECT_EXTERNAL_METRICS(...) \
  OUTERMOST_SCOPED_TRACE((ExpectExternalMetrics(__VA_ARGS__)))

  void ExpectExternalMetrics(ExternalMetricsExpectations expect) {
    EXPECT_METRIC(NumberpadMetricsRecorder::kFeatureExternalEnterKeystrokes,
                  expect.enter_keys, 0, expect.eligible, expect.enabled);
    EXPECT_METRIC(NumberpadMetricsRecorder::kFeatureExternalNonEnterKeystrokes,
                  expect.non_enter_keys, 0, expect.eligible, expect.enabled);
  }

#define EXPECT_INTERNAL_METRICS(...) \
  OUTERMOST_SCOPED_TRACE((ExpectInternalMetrics(__VA_ARGS__)))

  void ExpectInternalMetrics(InternalMetricsExpectations expect) {
    EXPECT_METRIC(NumberpadMetricsRecorder::kFeatureInternalEnterKeystrokes,
                  expect.enter_keys, 0, expect.eligible, expect.enabled);
    EXPECT_METRIC(NumberpadMetricsRecorder::kFeatureInternalNonEnterKeystrokes,
                  expect.non_enter_keys, 0, expect.eligible, expect.enabled);
  }

#define EXPECT_DYNAMIC_METRICS(...) \
  OUTERMOST_SCOPED_TRACE((ExpectDynamicMetrics(__VA_ARGS__)))

  void ExpectDynamicMetrics(DynamicMetricsExpectations expect) {
    EXPECT_METRIC(NumberpadMetricsRecorder::kFeatureDynamicEnterKeystrokes,
                  expect.enter_keys, 0, expect.eligible, expect.enabled);
    EXPECT_METRIC(NumberpadMetricsRecorder::kFeatureDynamicNonEnterKeystrokes,
                  expect.non_enter_keys, 0, expect.eligible, expect.enabled);
    EXPECT_METRIC(NumberpadMetricsRecorder::kFeatureDynamicActivations,
                  expect.activations, 0, expect.eligible, expect.enabled);
    EXPECT_METRIC(NumberpadMetricsRecorder::kFeatureDynamicCancellations,
                  expect.cancellations, 0, expect.eligible, expect.enabled);
  }

  // Tell the metrics object about a single key event
  void ProcessKey(const ui::InputDevice& input_device,
                  unsigned int key,
                  bool down) {
    numberpad_metrics_.ProcessKey(key, down, input_device);
  }

  // Press and release a key
  void ProcessKey(const ui::InputDevice& input_device, unsigned int key) {
    numberpad_metrics_.ProcessKey(key, true, input_device);
    numberpad_metrics_.ProcessKey(key, false, input_device);
  }

  // Convenience for pressing and releasing a sequence of keys
  void ProcessKeys(const ui::InputDevice& input_device,
                   const std::vector<unsigned int>& keys) {
    for (auto key : keys)
      ProcessKey(input_device, key);
  }

  // feature_usage_metrics testing utility: step forward time sufficiently to
  // allow feature_usage_metrics to emit at least one set of enabled/eligible
  // markers. Use is coupled with EXPECT_METRIC_*() above.
  void DelayForPeriodicMetrics() {
    task_environment_.AdvanceClock(
        ash::feature_usage::FeatureUsageMetrics::kRepeatedInterval +
        base::Seconds(30));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  int scope_level_ = 0;

  // Counter for generating new devices
  int device_id_ = 0;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::MainThreadType::UI};

  base::RunLoop run_loop_;

  base::HistogramTester histogram_tester_;

  // NumberpadMetricsRecorder uses feature_usage_metrics, which posts tasks
  // during construction, so we need a task environment set up before it is
  // constructed.
  ui::NumberpadMetricsRecorder numberpad_metrics_;
};

// Class used for testing parameterised variations of number-lock presses to
// dynamic number pad
class NumberpadMetricsTestParam
    : public NumberpadMetricsTest,
      public testing::WithParamInterface<std::tuple<int, int, bool, int, int>> {
 public:
};

TEST_F(NumberpadMetricsTest, ObservationBaselineNoDevices) {
  EXPECT_DYNAMIC_PRESENT(false);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(false);
}

TEST_F(NumberpadMetricsTest, ObservationOnlyExternalWideKeyboard) {
  ui::InputDevice external_wide_keyboard =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);
  numberpad_metrics_.AddDevice(external_wide_keyboard);

  EXPECT_DYNAMIC_PRESENT(false);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(true);
}

TEST_F(NumberpadMetricsTest, ObservationOnlyInternalWideKeyboard) {
  ui::InputDevice internal_wide_keyboard =
      InputDeviceFromCapabilities(ui::kWoomaxKeyboard);
  numberpad_metrics_.AddDevice(internal_wide_keyboard);

  EXPECT_DYNAMIC_PRESENT(false);
  EXPECT_INTERNAL_PRESENT(true);
  EXPECT_EXTERNAL_PRESENT(false);
}

TEST_F(NumberpadMetricsTest, ObservationOnlyInternalDynamicNumberpad) {
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);
  numberpad_metrics_.AddDevice(dynamic_numberpad);

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(false);
}

TEST_F(NumberpadMetricsTest, ObservationEverythingAtOnce) {
  ui::InputDevice internal_wide_keyboard =
      InputDeviceFromCapabilities(ui::kWoomaxKeyboard);
  numberpad_metrics_.AddDevice(internal_wide_keyboard);
  ui::InputDevice external_wide_keyboard =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);
  numberpad_metrics_.AddDevice(external_wide_keyboard);
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);
  numberpad_metrics_.AddDevice(dynamic_numberpad);

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(true);
  EXPECT_EXTERNAL_PRESENT(true);
}

TEST_F(NumberpadMetricsTest, ObservationDynamicExternalKeyboards) {
  const ui::InputDevice external_wide_keyboard =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);
  numberpad_metrics_.AddDevice(dynamic_numberpad);

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(false);

  // Plug in external wide keyboard
  numberpad_metrics_.AddDevice(external_wide_keyboard);

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(true);

  // Then unplug the external wide keyboard
  numberpad_metrics_.RemoveDevice(external_wide_keyboard);

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(false);
}

TEST_F(NumberpadMetricsTest, ObservationDynamicMultipleExternalKeyboards) {
  const ui::InputDevice external_wide_keyboard_1 =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);
  const ui::InputDevice external_wide_keyboard_2 =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);
  const ui::InputDevice external_wide_keyboard_3 =
      InputDeviceFromCapabilities(ui::kHpUsbKeyboard);

  EXPECT_DYNAMIC_PRESENT(false);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(false);

  // Regardless of changes, one or more external keyboards should always be
  // recognized.
  numberpad_metrics_.AddDevice(external_wide_keyboard_1);

  EXPECT_EXTERNAL_PRESENT(true);

  numberpad_metrics_.AddDevice(external_wide_keyboard_2);

  EXPECT_EXTERNAL_PRESENT(true);

  numberpad_metrics_.AddDevice(external_wide_keyboard_3);

  EXPECT_EXTERNAL_PRESENT(true);

  numberpad_metrics_.RemoveDevice(external_wide_keyboard_1);

  EXPECT_EXTERNAL_PRESENT(true);

  numberpad_metrics_.RemoveDevice(external_wide_keyboard_2);

  EXPECT_EXTERNAL_PRESENT(true);

  numberpad_metrics_.RemoveDevice(external_wide_keyboard_3);

  EXPECT_EXTERNAL_PRESENT(false);
}

TEST_F(NumberpadMetricsTest, ObservationBluetoothNumericPad) {
  // Try a real number-key only pad (which unfortunately describes an entire
  // keyboard).

  numberpad_metrics_.AddDevice(
      InputDeviceFromCapabilities(ui::kDrobitNumberpad));
  numberpad_metrics_.AddDevice(
      InputDeviceFromCapabilities(ui::kWoomaxKeyboard));
  numberpad_metrics_.AddDevice(
      InputDeviceFromCapabilities(ui::kMicrosoftBluetoothNumberPad));

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(true);
  EXPECT_EXTERNAL_PRESENT(true);
}

TEST_F(NumberpadMetricsTest, ObservationPluggingInBluetoothNumericPad) {
  // Try plugging in the bluetooth pad dynamically.

  numberpad_metrics_.AddDevice(
      InputDeviceFromCapabilities(ui::kDrobitNumberpad));
  numberpad_metrics_.AddDevice(
      InputDeviceFromCapabilities(ui::kWoomaxKeyboard));

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(true);
  EXPECT_EXTERNAL_PRESENT(false);

  numberpad_metrics_.AddDevice(
      InputDeviceFromCapabilities(ui::kMicrosoftBluetoothNumberPad));

  EXPECT_EXTERNAL_PRESENT(true);
}

TEST_F(NumberpadMetricsTest, MetricsBaseline) {
  EXPECT_DYNAMIC_PRESENT(false);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(false);

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.enter_keys = 0,
                          .non_enter_keys = 0,
                          .activations = 0,
                          .cancellations = 0,
                          .eligible = false,
                          .enabled = false});
  EXPECT_INTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 0,
                           .eligible = false,
                           .enabled = false});
  EXPECT_EXTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 0,
                           .eligible = true,
                           .enabled = false});
}

TEST_F(NumberpadMetricsTest, MetricsWideExternalKeyboardBasics) {
  ui::InputDevice external_wide_keyboard =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);

  numberpad_metrics_.AddDevice(external_wide_keyboard);

  EXPECT_DYNAMIC_PRESENT(false);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(true);

  int total_enter = 0;
  int total_non_enter = 0;

  // Check that we start off with zero.

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});
  EXPECT_INTERNAL_METRICS({.eligible = false, .enabled = false});
  EXPECT_DYNAMIC_METRICS({.eligible = false, .enabled = false});

  // Check a number-pad digit.
  ProcessKeys(external_wide_keyboard, {KEY_KP0});
  total_non_enter++;

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});

  // Try the keypad enter
  ProcessKeys(external_wide_keyboard, {KEY_KPENTER});
  total_enter++;

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});

  // Check multiple number pad numeric keys in a row.
  ProcessKeys(external_wide_keyboard, {KEY_KP1, KEY_KP2, KEY_KP2});
  total_non_enter += 3;

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});
}

TEST_F(NumberpadMetricsTest, MetricsWideExternalKeyboardNonKeypad) {
  ui::InputDevice external_wide_keyboard =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);

  numberpad_metrics_.AddDevice(external_wide_keyboard);

  EXPECT_DYNAMIC_PRESENT(false);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(true);

  int total_enter = 0;
  int total_non_enter = 0;

  // Check that we start off with zero.

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});
  EXPECT_INTERNAL_METRICS({.eligible = false, .enabled = false});
  EXPECT_DYNAMIC_METRICS({.eligible = false, .enabled = false});

  // Check a number-pad digit.
  ProcessKeys(external_wide_keyboard, {KEY_KP0});
  total_non_enter++;

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});

  // Check that letter keys don't count.
  ProcessKeys(external_wide_keyboard, {KEY_Q, KEY_A});

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});

  // Check multiple number pad numeric keys in a row.
  ProcessKeys(external_wide_keyboard, {KEY_KP1, KEY_KP2, KEY_KP2});
  total_non_enter += 3;

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});

  // Check that top-row numeric keys don't count.
  ProcessKeys(external_wide_keyboard, {KEY_0, KEY_1, KEY_2});

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});

  // Try the keypad enter
  ProcessKeys(external_wide_keyboard, {KEY_KPENTER});
  total_enter++;

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});

  // Then the core enter key
  ProcessKeys(external_wide_keyboard, {KEY_ENTER});

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = total_enter,
                           .non_enter_keys = total_non_enter,
                           .eligible = true,
                           .enabled = true});
}

TEST_F(NumberpadMetricsTest, MetricsWideExternalKeyboardVsOtherKeyboards) {
  ui::InputDevice external_wide_keyboard =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);
  ui::InputDevice internal_wide_keyboard =
      InputDeviceFromCapabilities(ui::kWoomaxKeyboard);
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);

  numberpad_metrics_.AddDevice(internal_wide_keyboard);
  numberpad_metrics_.AddDevice(external_wide_keyboard);
  numberpad_metrics_.AddDevice(dynamic_numberpad);

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(true);
  EXPECT_EXTERNAL_PRESENT(true);

  // Check that we start off with zero.

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 0,
                           .eligible = true,
                           .enabled = true});
  EXPECT_INTERNAL_METRICS({.eligible = true, .enabled = true});
  EXPECT_DYNAMIC_METRICS({.eligible = true, .enabled = true});

  // Check that keys on the internal keyboard don't count towards external
  // metrics.
  ProcessKeys(internal_wide_keyboard,
              {KEY_A, KEY_0, KEY_KP0, KEY_ENTER, KEY_KPENTER});

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 0,
                           .eligible = true,
                           .enabled = true});

  // Check that number-pad digits on the dynamic numberpad don't count towards
  // external metrics.
  ProcessKeys(dynamic_numberpad, {KEY_KP0, KEY_KPENTER, KEY_5});

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 0,
                           .eligible = true,
                           .enabled = true});
}

TEST_F(NumberpadMetricsTest, MetricsInternalKeyboardVsOtherKeyboards) {
  ui::InputDevice external_wide_keyboard =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);
  ui::InputDevice internal_wide_keyboard =
      InputDeviceFromCapabilities(ui::kWoomaxKeyboard);
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);

  numberpad_metrics_.AddDevice(internal_wide_keyboard);
  numberpad_metrics_.AddDevice(external_wide_keyboard);
  numberpad_metrics_.AddDevice(dynamic_numberpad);

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(true);
  EXPECT_EXTERNAL_PRESENT(true);

  // Check that we start off with zero.

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 0,
                           .eligible = true,
                           .enabled = true});
  EXPECT_INTERNAL_METRICS({.eligible = true, .enabled = true});
  EXPECT_DYNAMIC_METRICS({.eligible = true, .enabled = true});

  // Check that keys on the external keyboard don't count.
  ProcessKeys(external_wide_keyboard,
              {KEY_A, KEY_0, KEY_KP0, KEY_NUMLOCK, KEY_ENTER, KEY_KPENTER});

  DelayForPeriodicMetrics();

  EXPECT_INTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 0,
                           .eligible = true,
                           .enabled = true});

  // Check that number-pad digits on the dynamic numberpad don't count.
  ProcessKeys(dynamic_numberpad, {KEY_KP0, KEY_KPENTER, KEY_5});

  DelayForPeriodicMetrics();

  EXPECT_INTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 0,
                           .eligible = true,
                           .enabled = true});
}

TEST_F(NumberpadMetricsTest, MetricsDynamicNumerpadVsOtherKeyboards) {
  ui::InputDevice external_wide_keyboard =
      InputDeviceFromCapabilities(ui::kLogitechKeyboardK120);
  ui::InputDevice internal_wide_keyboard =
      InputDeviceFromCapabilities(ui::kWoomaxKeyboard);
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);

  numberpad_metrics_.AddDevice(internal_wide_keyboard);
  numberpad_metrics_.AddDevice(external_wide_keyboard);
  numberpad_metrics_.AddDevice(dynamic_numberpad);

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(true);
  EXPECT_EXTERNAL_PRESENT(true);

  // Check that we start off with zero.

  DelayForPeriodicMetrics();

  EXPECT_EXTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 0,
                           .eligible = true,
                           .enabled = true});
  EXPECT_INTERNAL_METRICS({.eligible = true, .enabled = true});
  EXPECT_DYNAMIC_METRICS({.eligible = true, .enabled = true});

  // Check that keys on the external keyboard don't count.
  ProcessKeys(external_wide_keyboard,
              {KEY_A, KEY_0, KEY_KP0, KEY_NUMLOCK, KEY_ENTER, KEY_KPENTER});

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.eligible = true, .enabled = true});

  // Really hammer that numlock
  ProcessKeys(external_wide_keyboard, {KEY_NUMLOCK, KEY_NUMLOCK, KEY_KP0,
                                       KEY_NUMLOCK, KEY_NUMLOCK, KEY_NUMLOCK});

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.eligible = true, .enabled = true});

  // Check that keys on the internal keyboard don't count.
  ProcessKeys(internal_wide_keyboard,
              {KEY_A, KEY_0, KEY_KP0, KEY_ENTER, KEY_KPENTER, KEY_HOME});

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.eligible = true, .enabled = true});

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.eligible = true, .enabled = true});
}

TEST_F(NumberpadMetricsTest, MetricsWideInternalKeyboard) {
  ui::InputDevice internal_wide_keyboard =
      InputDeviceFromCapabilities(ui::kWoomaxKeyboard);

  numberpad_metrics_.AddDevice(internal_wide_keyboard);

  EXPECT_DYNAMIC_PRESENT(false);
  EXPECT_INTERNAL_PRESENT(true);
  EXPECT_EXTERNAL_PRESENT(false);

  ProcessKeys(internal_wide_keyboard, {KEY_KP0});

  DelayForPeriodicMetrics();

  EXPECT_INTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 1,
                           .eligible = true,
                           .enabled = true});
  EXPECT_EXTERNAL_METRICS({.eligible = true, .enabled = false});
  EXPECT_DYNAMIC_METRICS({.eligible = false, .enabled = false});

  ProcessKeys(internal_wide_keyboard, {KEY_KP1, KEY_KP2, KEY_KP2});

  DelayForPeriodicMetrics();

  EXPECT_INTERNAL_METRICS({.enter_keys = 0,
                           .non_enter_keys = 4,
                           .eligible = true,
                           .enabled = true});

  ProcessKeys(internal_wide_keyboard, {KEY_KPENTER});

  DelayForPeriodicMetrics();

  EXPECT_INTERNAL_METRICS({.enter_keys = 1,
                           .non_enter_keys = 4,
                           .eligible = true,
                           .enabled = true});
}

TEST_F(NumberpadMetricsTest, MetricsDynamicNumberpadBasics) {
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);

  numberpad_metrics_.AddDevice(dynamic_numberpad);

  EXPECT_DYNAMIC_PRESENT(true);
  EXPECT_INTERNAL_PRESENT(false);
  EXPECT_EXTERNAL_PRESENT(false);

  // Turn on numlock mode, hit numberpad key

  ProcessKeys(dynamic_numberpad, {KEY_NUMLOCK, KEY_KP0});

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.enter_keys = 0,
                          .non_enter_keys = 1,
                          .activations = 1,
                          .cancellations = 0,
                          .eligible = true,
                          .enabled = true});

  // Turn numlock off, on, off

  ProcessKeys(dynamic_numberpad, {KEY_NUMLOCK, KEY_NUMLOCK, KEY_NUMLOCK});

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.enter_keys = 0,
                          .non_enter_keys = 1,
                          .activations = 2,
                          .cancellations = 1,
                          .eligible = true,
                          .enabled = true});
}

TEST_F(NumberpadMetricsTest, MetricsDynamicNumberpadAutoActivation) {
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);

  numberpad_metrics_.AddDevice(dynamic_numberpad);

  // Hit numberpad key without turning on numlock; should not auto activate.

  ProcessKeys(dynamic_numberpad, {KEY_KP0});

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.enter_keys = 0,
                          .non_enter_keys = 1,
                          .activations = 0,
                          .cancellations = 0,
                          .eligible = true,
                          .enabled = true});

  // Turn numlock off, on, off
  ProcessKeys(dynamic_numberpad, {KEY_NUMLOCK, KEY_NUMLOCK, KEY_NUMLOCK});

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.enter_keys = 0,
                          .non_enter_keys = 1,
                          .activations = 1,
                          .cancellations = 1,
                          .eligible = true,
                          .enabled = true});

  // Then hit numberpad key again without turning on numlock; will auto
  // activate to resolve a transition that we don't ever expect, but can
  // easily define the behaviour for.

  ProcessKeys(dynamic_numberpad, {KEY_KP0});

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.enter_keys = 0,
                          .non_enter_keys = 2,
                          .activations = 2,
                          .cancellations = 1,
                          .eligible = true,
                          .enabled = true});
}

TEST_F(NumberpadMetricsTest, MetricsDynamicNumberpadPressRelease) {
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);

  numberpad_metrics_.AddDevice(dynamic_numberpad);

  // Verify that metrics are triggered on key-press, not release.

  // Start holding them down
  ProcessKey(dynamic_numberpad, KEY_NUMLOCK, true);
  ProcessKey(dynamic_numberpad, KEY_KP0, true);

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.enter_keys = 0,
                          .non_enter_keys = 1,
                          .activations = 1,
                          .cancellations = 0,
                          .eligible = true,
                          .enabled = true});

  // Release keys
  ProcessKey(dynamic_numberpad, KEY_NUMLOCK, false);
  ProcessKey(dynamic_numberpad, KEY_KP0, false);

  DelayForPeriodicMetrics();

  // Should be unchanged
  EXPECT_DYNAMIC_METRICS({.enter_keys = 0,
                          .non_enter_keys = 1,
                          .activations = 1,
                          .cancellations = 0,
                          .eligible = true,
                          .enabled = true});
}

TEST_P(NumberpadMetricsTestParam, CheckDeduction) {
  ui::InputDevice dynamic_numberpad =
      InputDeviceFromCapabilities(ui::kDrobitNumberpad);

  numberpad_metrics_.AddDevice(dynamic_numberpad);

  int numlock_count_prior = std::get<0>(GetParam());
  int numlock_count_post = std::get<1>(GetParam());
  bool enter_key = std::get<2>(GetParam());
  int activations = std::get<3>(GetParam());
  int cancellations = std::get<4>(GetParam());

  // Consecutive numlocks before enter
  for (int i = 0; i < numlock_count_prior; i++)
    ProcessKey(dynamic_numberpad, KEY_NUMLOCK);

  if (enter_key)
    ProcessKey(dynamic_numberpad, KEY_KPENTER);

  // Consecutive numlocks after enter
  for (int i = 0; i < numlock_count_post; i++)
    ProcessKey(dynamic_numberpad, KEY_NUMLOCK);

  DelayForPeriodicMetrics();

  EXPECT_DYNAMIC_METRICS({.enter_keys = enter_key ? 1 : 0,
                          .non_enter_keys = 0,
                          .activations = activations,
                          .cancellations = cancellations,
                          .eligible = true,
                          .enabled = true});
}

// Investigate all significant variations for the dynamic numberpad
// state deduction logic.
INSTANTIATE_TEST_SUITE_P(
    NumberpadMetricsDynamicNumberpadStateDeduction,
    NumberpadMetricsTestParam,
    ::testing::Values(
        // Check the guesses that are made for various number of numlocks
        // starting with default unknown state.

        // Baseline, no events
        std::make_tuple(0, 0, false, 0, 0),
        // One single numlock doesn't demonstrate anything
        std::make_tuple(1, 0, false, 0, 0),
        // Two numlocks prove activation (somewhere)
        std::make_tuple(2, 0, false, 1, 0),
        // Three numlocks prove cancellation (somewhere)
        std::make_tuple(3, 0, false, 1, 1),
        // Verify pattern
        std::make_tuple(4, 0, false, 2, 1),
        std::make_tuple(5, 0, false, 2, 2),
        std::make_tuple(6, 0, false, 3, 2),
        std::make_tuple(7, 0, false, 3, 3),
        // Arbitrary numbers
        std::make_tuple(2 * 7, 0, false, 7, 7 - 1),
        std::make_tuple(2 * 7 + 1, 0, false, 7, 7),
        std::make_tuple(2 * 24, 0, false, 24, 24 - 1),
        std::make_tuple(2 * 24 + 1, 0, false, 24, 24),

        // Now force state with a kp-enter key after the numlocks to see how
        // it resolves the prior guess.

        // 0 numlocks: we don't know whether an activation had already been
        // counted; do not count one here
        std::make_tuple(0, 0, true, 0, 0),
        // One single numlock, now we know we've known we had an activation
        std::make_tuple(1, 0, true, 1, 0),
        // Two numlocks, no more evidence
        std::make_tuple(2, 0, true, 1, 0),
        // Three numlocks, on,off,on, now there must have been a cancellation
        std::make_tuple(3, 0, true, 2, 1),
        // Four numlocks, off,on,off,on
        std::make_tuple(4, 0, true, 2, 1),
        // Five numlocks, on,off,on,off,on
        std::make_tuple(5, 0, true, 3, 2),
        // Verify pattern
        std::make_tuple(6, 0, true, 3, 2),
        std::make_tuple(7, 0, true, 4, 3),
        std::make_tuple(8, 0, true, 4, 3),

        // Test numlock toggle works properly once state is known:
        // this assumes state is permanently known after pressing a key, so
        // we don't have to test from each of the possible guesses.

        std::make_tuple(1, 1, true, 1, 0),
        std::make_tuple(1, 2, true, 2, 0),
        std::make_tuple(1, 3, true, 2, 1),
        std::make_tuple(1, 4, true, 3, 1),
        std::make_tuple(1, 5, true, 3, 2),
        std::make_tuple(1, 6, true, 4, 2),
        std::make_tuple(1, 7, true, 4, 3),
        std::make_tuple(1, 8, true, 5, 3)));

}  // namespace ui
