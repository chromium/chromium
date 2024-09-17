// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_IMPOSTER_CHECKER_EVDEV_STATE_H_
#define UI_EVENTS_OZONE_EVDEV_IMPOSTER_CHECKER_EVDEV_STATE_H_

#include "base/component_export.h"
#include "base/functional/callback.h"

namespace ui {

// The state for imposter checker evdev which helps in disabling/enabling
// keyboard check.
class COMPONENT_EXPORT(EVDEV) ImposterCheckerEvdevState {
 public:
  static ImposterCheckerEvdevState& Get();
  static bool HasInstance();

  ImposterCheckerEvdevState();
  virtual ~ImposterCheckerEvdevState();

  bool IsKeyboardCheckEnabled();
  void SetKeyboardCheckEnabled(bool enabled);

 private:
  // The default state for the imposter check is enabled.
  bool keyboard_check_enabled_ = true;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_IMPOSTER_CHECKER_EVDEV_STATE_H_
