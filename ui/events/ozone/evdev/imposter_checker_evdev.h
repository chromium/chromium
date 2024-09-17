// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_IMPOSTER_CHECKER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_IMPOSTER_CHECKER_EVDEV_H_

#include <map>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/imposter_checker_evdev_state.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/events/ozone/evdev/fake_keyboard_heuristic_metrics.h"
#endif

namespace ui {
class COMPONENT_EXPORT(EVDEV) ImposterCheckerEvdev {
 public:
  // Registers the id of a device on its phys path and returns the ids of all
  // devices on that phys path.
  std::vector<int> OnDeviceAdded(EventConverterEvdev* converter);
  std::vector<int> OnDeviceRemoved(EventConverterEvdev* converter);
  bool FlagSuspectedImposter(EventConverterEvdev* converter);

  ImposterCheckerEvdev();

  ImposterCheckerEvdev(const ImposterCheckerEvdev&) = delete;
  ImposterCheckerEvdev& operator=(const ImposterCheckerEvdev&) = delete;

  ~ImposterCheckerEvdev();

 private:
  std::vector<int> GetIdsOnSamePhys(const std::string& phys_path);
  bool IsSuspectedKeyboardImposter(EventConverterEvdev* converter,
                                   bool shared_phys);
  bool IsSuspectedMouseImposter(EventConverterEvdev* converter,
                                bool shared_phys);

  // Number of devices per phys path.
  std::multimap<std::string, int> devices_on_phys_path_;
  std::unique_ptr<ImposterCheckerEvdevState> imposter_checker_evdev_state_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  FakeKeyboardHeuristicMetrics fake_keyboard_heuristic_metrics_;
#endif
};
}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_IMPOSTER_CHECKER_EVDEV_H_
