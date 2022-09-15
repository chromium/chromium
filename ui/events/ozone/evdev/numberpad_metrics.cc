// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/numberpad_metrics.h"

#include "base/logging.h"
#include "base/no_destructor.h"

#include <linux/input.h>

namespace ui {

namespace {

// Constants for identifying Elan Dynamic Numberpad
const int kDynamicNumberpadVendor = 0x04F3;
const int kDynamicNumberpadProduct = 0x31C6;

bool IsDeviceDynamicNumberpad(const InputDevice& input_device) {
  bool result = (input_device.vendor_id == kDynamicNumberpadVendor &&
                 input_device.product_id == kDynamicNumberpadProduct);
  return result;
}

bool IsNumpadEnterKey(unsigned int keycode) {
  return (keycode == KEY_KPENTER);
}

// Recognize the majority of keys (other than Enter) that could
// be found on a numberpad, and only a number pad; this does not
// cover additional keys that are often on external stand-alone
// numeric pads (backspace, tab, 'Calculator'), or cover every
// defined KEY_KP* symbol.
bool IsNumpadOtherKey(unsigned int keycode) {
  switch (keycode) {
    case KEY_KP0:
    case KEY_KP1:
    case KEY_KP2:
    case KEY_KP3:
    case KEY_KP4:
    case KEY_KP5:
    case KEY_KP6:
    case KEY_KP7:
    case KEY_KP8:
    case KEY_KP9:
    case KEY_KPMINUS:
    case KEY_KPPLUS:
    case KEY_KPDOT:
    case KEY_KPSLASH:
    case KEY_KPASTERISK:
    case KEY_KPEQUAL:
      return true;
    default:
      return false;
  }
}

bool IsNumpadKey(unsigned int keycode) {
  return (IsNumpadEnterKey(keycode) || IsNumpadOtherKey(keycode));
}

bool IsNumlockKey(unsigned int keycode) {
  return (keycode == KEY_NUMLOCK);
}

// The Elan Dynamic Numberpad includes a few extra keys,
// which we want to recognize; they include backspace, a
// non-kp equals key, and it can emit a '%' via Shift-5.
bool IsDynamicNumpadKey(unsigned int keycode) {
  if (IsNumpadKey(keycode) || IsNumpadEnterKey(keycode) ||
      IsNumpadOtherKey(keycode) || IsNumlockKey(keycode))
    return true;

  switch (keycode) {
    case KEY_5:
    case KEY_BACKSPACE:
    case KEY_EQUAL:
      return true;
    default:
      return false;
  }
}

bool IsInternalDevice(const InputDevice& input_device) {
  bool result = (input_device.type == INPUT_DEVICE_INTERNAL ||
                 input_device.type == INPUT_DEVICE_UNKNOWN);
  return result;
}

}  // namespace

// static
constexpr char NumberpadMetricsRecorder::kFeatureDynamicActivations[];
// static
constexpr char NumberpadMetricsRecorder::kFeatureDynamicCancellations[];
// static
constexpr char NumberpadMetricsRecorder::kFeatureDynamicEnterKeystrokes[];
// static
constexpr char NumberpadMetricsRecorder::kFeatureDynamicNonEnterKeystrokes[];
// static
constexpr char NumberpadMetricsRecorder::kFeatureInternalEnterKeystrokes[];
// static
constexpr char NumberpadMetricsRecorder::kFeatureInternalNonEnterKeystrokes[];
// static
constexpr char NumberpadMetricsRecorder::kFeatureExternalEnterKeystrokes[];
// static
constexpr char NumberpadMetricsRecorder::kFeatureExternalNonEnterKeystrokes[];

NumberpadMetricsDelegate::NumberpadMetricsDelegate(
    const std::string& feature_name)
    : metrics_(feature_name, this) {}

NumberpadMetricsDelegate::~NumberpadMetricsDelegate() = default;

bool NumberpadMetricsDelegate::IsEligible() const {
  return eligible_;
}

bool NumberpadMetricsDelegate::IsEnabled() const {
  return enabled_;
}

void NumberpadMetricsDelegate::SetState(bool now_eligible, bool now_enabled) {
  // enabled must only be set if eligible is set
  DCHECK(now_enabled ? now_eligible : true);
  eligible_ = now_eligible;
  enabled_ = now_enabled;
}

void NumberpadMetricsDelegate::RecordUsage(bool success) {
  // If we are out of sync for some reason, just ignore events to
  // preserve feature_usage_metric's assumptions.
  if (!eligible_ || !enabled_)
    return;
  metrics_.RecordUsage(success);
}

// static
NumberpadMetricsRecorder* NumberpadMetricsRecorder::GetInstance() {
  // For now, we leak this in normal operation; our invokers,
  // EventConverterEvdevImpls, are not currently destroyed during
  // a clean shut-down.
  static base::NoDestructor<NumberpadMetricsRecorder> instance;
  return instance.get();
}

void NumberpadMetricsRecorder::AddDevice(const ui::InputDevice& input_device) {
  if (IsInternalDevice(input_device)) {
    if (IsDeviceDynamicNumberpad(input_device)) {
      dynamic_numpad_present_++;
    } else {
      internal_numpad_present_++;
    }
  } else {
    external_numpad_present_++;
  }
  UpdateDeviceState();
}

void NumberpadMetricsRecorder::RemoveDevice(
    const ui::InputDevice& input_device) {
  if (IsInternalDevice(input_device)) {
    if (IsDeviceDynamicNumberpad(input_device)) {
      dynamic_numpad_present_--;
    } else {
      internal_numpad_present_--;
    }
  } else {
    external_numpad_present_--;
  }
  UpdateDeviceState();
}

void NumberpadMetricsRecorder::UpdateDeviceState() {
  if (dynamic_numpad_present_ > 1)
    LOG(ERROR) << "Unexpected multiple dynamic numberpads seen.";

  dynamic_activations_metrics_delegate_.SetState(dynamic_numpad_present_ > 0);
  dynamic_cancellations_metrics_delegate_.SetState(dynamic_numpad_present_ > 0);
  dynamic_enter_keystrokes_metrics_delegate_.SetState(dynamic_numpad_present_ >
                                                      0);
  dynamic_non_enter_keystrokes_metrics_delegate_.SetState(
      dynamic_numpad_present_ > 0);

  internal_enter_keystrokes_metrics_delegate_.SetState(
      internal_numpad_present_ > 0);
  internal_non_enter_keystrokes_metrics_delegate_.SetState(
      internal_numpad_present_ > 0);

  external_enter_keystrokes_metrics_delegate_.SetState(
      true, external_numpad_present_ > 0);
  external_non_enter_keystrokes_metrics_delegate_.SetState(
      true, external_numpad_present_ > 0);
}

NumberpadMetricsRecorder::NumberpadMetricsRecorder() {
  UpdateDeviceState();
}

NumberpadMetricsRecorder::~NumberpadMetricsRecorder() = default;

// Background: the dynamic numberpad lets us know when its state is changed
// via a numlock keypress, however we cannot retrieve the current state. On
// boot it is reasonable to assume it is off, however on chromium restart (for
// crash or any other reason) we don't know.
//
// Using the assumption that a numberpad key can only come through when it
// is turned on, we can fix the state at that point; prior to that, we can
// only guess. We are conservative and only push events that must be true,
// and accept we might lose an activation or cancellation in combination
// with a crash/restart.

void NumberpadMetricsRecorder::ToggleDynamicNumlockState() {
  if (dynamic_numlock_state_known_) {
    dynamic_numlock_state_ = !dynamic_numlock_state_;
    if (dynamic_numlock_state_) {
      // Activated numlock
      dynamic_activations_metrics_delegate_.RecordUsage(true);
    } else {
      // Deactivated numlock; if we have not used a numberpad key since it was
      // activated, consider it a cancelled use of the numberpad.
      if (!dynamic_numpad_used_)
        dynamic_cancellations_metrics_delegate_.RecordUsage(true);
    }
    dynamic_numpad_used_ = false;
  } else {
    // We have an unknown original state: after two flips, it must have been
    // turned on, after three flips (without seeing a numpad key) it must have
    // been turned back off without being used. (If it was used, we would no
    // longer be in this branch of code.)

    if (!any_dynamic_numlock_state_flips_) {
      // Discard first flip
      any_dynamic_numlock_state_flips_ = true;
    } else {
      dynamic_numlock_state_guess_ = !dynamic_numlock_state_guess_;
      if (dynamic_numlock_state_guess_) {
        // 2nd, 4th, etc. flip
        dynamic_activations_metrics_delegate_.RecordUsage(true);
      } else {
        // 3rd, 5th, etc. flip
        dynamic_cancellations_metrics_delegate_.RecordUsage(true);
      }
    }
  }
}

// Used when a key has been pressed on a dynamic numberpad,
// proving the numlock state is on.
void NumberpadMetricsRecorder::SetDynamicNumlockStateOn() {
  if (dynamic_numlock_state_known_) {
    // If we have known numlock state, then we should be accurately changing
    // the state every time a numlock key comes in; if we get a numberpad
    // key from the dynamic numberpad when we believe numlock is off, something
    // unexpected has happened; just log an error, fix our state, and move on.
    if (dynamic_numlock_state_ == false) {
      LOG(ERROR)
          << "Dynamic numberpad in an unexpected state; numlock is on now.";
      ToggleDynamicNumlockState();
    }
    return;
  }

  // Now we know the state, we can stop guessing; sync up our previous
  // guess with the known state.

  dynamic_numlock_state_known_ = true;
  dynamic_numlock_state_ = true;

  // If we're sure we're off by one, issue an additional metric.
  // This won't issue a metric if numberlock isn't pressed before the
  // first key, as it could be excess, and we're being conservative.
  if (any_dynamic_numlock_state_flips_ && !dynamic_numlock_state_guess_)
    dynamic_activations_metrics_delegate_.RecordUsage(true);
}

// This method is only intended to be called for input devices where
// HasNumberpad() is true. We do not have enough information
// (from the input_device) to actually check this.
void NumberpadMetricsRecorder::ProcessKey(unsigned int key,
                                          bool down,
                                          const InputDevice& input_device) {
  // Only process key presses, not releases
  if (!down)
    return;

  if (IsInternalDevice(input_device)) {
    if (IsDeviceDynamicNumberpad(input_device) && IsDynamicNumpadKey(key)) {
      // Logic specific to particular dynamic numberpad.
      if (IsNumlockKey(key)) {
        ToggleDynamicNumlockState();
      } else {
        // Saw a numpad key, therefore numlock must be enabled
        SetDynamicNumlockStateOn();
        dynamic_numpad_used_ = true;
        if (IsNumpadEnterKey(key)) {
          dynamic_enter_keystrokes_metrics_delegate_.RecordUsage(true);
        } else {
          dynamic_non_enter_keystrokes_metrics_delegate_.RecordUsage(true);
        }
      }
    } else if (IsNumpadKey(key)) {
      if (IsNumpadEnterKey(key)) {
        internal_enter_keystrokes_metrics_delegate_.RecordUsage(true);
      } else {
        internal_non_enter_keystrokes_metrics_delegate_.RecordUsage(true);
      }
    }
  } else {  // External device
    if (IsNumpadKey(key)) {
      if (IsNumpadEnterKey(key)) {
        external_enter_keystrokes_metrics_delegate_.RecordUsage(true);
      } else {
        external_non_enter_keystrokes_metrics_delegate_.RecordUsage(true);
      }
    }
  }
}

}  // namespace ui
