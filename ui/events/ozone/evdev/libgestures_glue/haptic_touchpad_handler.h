// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_HAPTIC_TOUCHPAD_HANDLER_H_
#define UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_HAPTIC_TOUCHPAD_HANDLER_H_

#include <map>
#include <memory>

#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/ozone/evdev/event_device_info.h"

namespace ui {

// Handles communication with haptic touchpads via the evdev force feedback
// interface.
//
// Checks that the device is capable of performing all of the required haptic
// effects, then takes control over haptic effects from the kernel.
//
// Performs haptic effects requested by the UI, and also provides a callback to
// perform the appropriate haptic effects when the gesture library determines
// that a physical button click (press or release) has occurred.
class HapticTouchpadHandler {
 public:
  explicit HapticTouchpadHandler(int fd);
  HapticTouchpadHandler(const HapticTouchpadHandler&) = delete;
  HapticTouchpadHandler& operator=(const HapticTouchpadHandler&) = delete;

  virtual ~HapticTouchpadHandler();

  // Will return a nullptr if the device does not support all the required
  // haptic features.
  static std::unique_ptr<HapticTouchpadHandler> Create(
      int fd,
      const EventDeviceInfo& devinfo);

  virtual void Initialize();

  // Was haptics setup successful?
  bool IsValid();

  // Set the effect to use on the next button release. Will only have an effect
  // if the button is currently pressed.
  void SetEffectForNextButtonRelease(HapticTouchpadEffect effect_type,
                                     HapticTouchpadEffectStrength strength);

  // Set the strength to use for clicks.
  void SetClickStrength(HapticTouchpadEffectStrength strength);

  // Handler for physical clicks as determined by libgestures.
  void OnGestureClick(bool press);

  // Play the given effect at the given strength.
  void PlayEffect(HapticTouchpadEffect effect,
                  HapticTouchpadEffectStrength strength);

 private:
  // Set up the force feedback interface by taking control of click haptics and
  // uploading required effects. Returns true if successful.
  bool SetupFf();

  // Destroy all force feedback effects that have been uploaded to the kernel.
  void DestroyAllFfEffects();

  // Destroy the force feedback effect with the given effect ID.
  virtual void DestroyFfEffect(int effect_id);

  // Play the force feedback effect with the given effect ID
  virtual void PlayFfEffect(int effect_id);

  // The next three methods (UploadFfEffect, TakeControlOfClickEffects, and
  // ReleaseControlOfClickEffects) depend on new changes to the linux userspace
  // API. However, they will only be called if the touchpad is marked as a
  // haptic touchpad by EventDeviceInfo, which also depends on the new changes
  // to the linux UAPI. When a HapticTouchpadHandler is initialized without the
  // required linux userspace API, haptics setup will fail before these methods
  // are called and the device will act as a normal non-haptic touchpad, with
  // click haptics handled by the touchpad controller.

  // Upload an effect to the touchpad with the given HID waveform usage and
  // intensity percentage. Returns the newly assigned effect ID if successful,
  // or -1 if unsuccessful.
  virtual int UploadFfEffect(uint16_t hid_usage, uint8_t intensity);

  // Take control of click effects from the kernel. After we take control, the
  // kernel will no longer perform haptics to emulate button clicks, but it will
  // continue to send button change events. Return true if successful.
  virtual bool TakeControlOfClickEffects();

  // Release control of click effects back to the kernel.
  void ReleaseControlOfClickEffects();

  // File descriptor for device.
  const int fd_;

  // Which effect and strength the touchpad should play next time the physical
  // button is released. If next_click_release_effect_ is set to kRelease, use
  // click_strength_ instead of next_click_release_strength_.
  HapticTouchpadEffect next_click_release_effect_;
  HapticTouchpadEffectStrength next_click_release_strength_;

  // What strength should be used for physical button press and release haptics.
  HapticTouchpadEffectStrength click_strength_;

  // Tells which force feedback effect ID is associated with each
  // effect-strength pair.
  std::map<HapticTouchpadEffect, std::map<HapticTouchpadEffectStrength, int>>
      ff_effect_id_;

  // Is the physical button currently pressed, as determined by the gesture
  // library?
  bool button_pressed_;

  // Are non-autonomous haptics enabled at all? This will be false if the
  // touchpad doesn't support all the required effects. In that case, the
  // touchpad acts like a normal touchpad: UI feedback is disabled and click
  // sensitivity cannot be customized.
  bool haptics_enabled_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_HAPTIC_TOUCHPAD_HANDLER_H_
