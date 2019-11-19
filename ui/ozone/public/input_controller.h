// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_INPUT_CONTROLLER_H_
#define UI_OZONE_PUBLIC_INPUT_CONTROLLER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/ozone/public/mojom/gesture_properties_service.mojom.h"

namespace base {
class TimeDelta;
}

namespace ui {

enum class DomCode;

// Platform-specific interface for controlling input devices.
//
// The object provides methods for the preference page to configure input
// devices w.r.t. the user setting. On ChromeOS, this replaces the inputcontrol
// script that is originally located at /opt/google/chrome/.
class COMPONENT_EXPORT(OZONE_BASE) InputController {
 public:
  using GetTouchDeviceStatusReply =
      base::OnceCallback<void(const std::string&)>;
  // TODO(sky): convert this to value once mojo supports move for vectors.
  using GetTouchEventLogReply =
      base::OnceCallback<void(const std::vector<base::FilePath>&)>;

  InputController() {}
  virtual ~InputController() {}

  // Functions for checking devices existence.
  virtual bool HasMouse() = 0;
  virtual bool HasTouchpad() = 0;

  // Keyboard settings.
  virtual bool IsCapsLockEnabled() = 0;
  virtual void SetCapsLockEnabled(bool enabled) = 0;
  virtual void SetNumLockEnabled(bool enabled) = 0;
  virtual bool IsAutoRepeatEnabled() = 0;
  virtual void SetAutoRepeatEnabled(bool enabled) = 0;
  virtual void SetAutoRepeatRate(const base::TimeDelta& delay,
                                 const base::TimeDelta& interval) = 0;
  virtual void GetAutoRepeatRate(base::TimeDelta* delay,
                                 base::TimeDelta* interval) = 0;
  virtual void SetCurrentLayoutByName(const std::string& layout_name) = 0;

  // Touchpad settings.
  virtual void SetTouchpadSensitivity(int value) = 0;
  virtual void SetTapToClick(bool enabled) = 0;
  virtual void SetThreeFingerClick(bool enabled) = 0;
  virtual void SetTapDragging(bool enabled) = 0;
  virtual void SetNaturalScroll(bool enabled) = 0;
  virtual void SetTouchpadAcceleration(bool enabled) = 0;

  // Mouse settings.
  virtual void SetMouseSensitivity(int value) = 0;
  virtual void SetPrimaryButtonRight(bool right) = 0;
  virtual void SetMouseReverseScroll(bool enabled) = 0;
  virtual void SetMouseAcceleration(bool enabled) = 0;

  // Touch log collection.
  virtual void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) = 0;
  virtual void GetTouchEventLog(const base::FilePath& out_dir,
                                GetTouchEventLogReply reply) = 0;
  // Touchscreen log settings.
  virtual void SetTouchEventLoggingEnabled(bool enabled) = 0;

  // Temporarily enable/disable Tap-to-click. Used to enhance the user
  // experience in some use cases (e.g., typing, watching video).
  virtual void SetTapToClickPaused(bool state) = 0;

  virtual void SetInternalTouchpadEnabled(bool enabled) = 0;
  virtual bool IsInternalTouchpadEnabled() const = 0;

  virtual void SetTouchscreensEnabled(bool enabled) = 0;

  // If |enable_filter| is true, all keys on the internal keyboard except
  // |allowed_keys| are disabled.
  virtual void SetInternalKeyboardFilter(bool enable_filter,
                                         std::vector<DomCode> allowed_keys) = 0;

  virtual void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputController);
};

// Create an input controller that does nothing.
COMPONENT_EXPORT(OZONE_BASE)
std::unique_ptr<InputController> CreateStubInputController();

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_INPUT_CONTROLLER_H_
