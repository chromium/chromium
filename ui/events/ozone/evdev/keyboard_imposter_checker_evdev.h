// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_KEYBOARD_IMPOSTER_CHECKER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_KEYBOARD_IMPOSTER_CHECKER_EVDEV_H_

#include <map>

#include "base/component_export.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/events/ozone/evdev/fake_keyboard_heuristic_metrics.h"
#endif

namespace ui {
class COMPONENT_EXPORT(EVDEV) KeyboardImposterCheckerEvdev {
 public:
  // Registers the id of a device on its phys path and returns the ids of all
  // devices on that phys path.
  std::vector<int> OnDeviceAdded(EventConverterEvdev* converter);
  std::vector<int> OnDeviceRemoved(EventConverterEvdev* converter);
  bool FlagIfImposter(EventConverterEvdev* converter);

  KeyboardImposterCheckerEvdev();

  KeyboardImposterCheckerEvdev(const KeyboardImposterCheckerEvdev&) = delete;
  KeyboardImposterCheckerEvdev& operator=(const KeyboardImposterCheckerEvdev&) =
      delete;

  ~KeyboardImposterCheckerEvdev();

 private:
  std::vector<int> GetIdsOnSamePhys(const std::string& phys_path);

  // Number of devices per phys path.
  std::multimap<std::string, int> devices_on_phys_path_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  FakeKeyboardHeuristicMetrics fake_keyboard_heuristic_metrics_;
#endif
};
}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_KEYBOARD_IMPOSTER_CHECKER_EVDEV_H_
