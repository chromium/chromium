// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/keyboard_imposter_checker_evdev.h"

#include "base/containers/cxx20_erase_map.h"
#include "ui/events/ozone/features.h"

namespace ui {

std::vector<int> KeyboardImposterCheckerEvdev::GetIdsOnSamePhys(
    const std::string& phys_path) {
  std::vector<int> ids_on_same_phys;
  std::pair<std::multimap<std::string, int>::iterator,
            std::multimap<std::string, int>::iterator>
      iterators = devices_on_phys_path_.equal_range(phys_path);
  for (auto& it = iterators.first; it != iterators.second; ++it) {
    ids_on_same_phys.push_back(it->second);
  }
  return ids_on_same_phys;
}

std::vector<int> KeyboardImposterCheckerEvdev::OnDeviceAdded(
    EventConverterEvdev* converter) {
  if (!base::FeatureList::IsEnabled(kEnableFakeKeyboardHeuristic))
    return std::vector<int>();

  devices_on_phys_path_.emplace(converter->input_device().phys,
                                converter->id());
  return GetIdsOnSamePhys(converter->input_device().phys);
}

bool KeyboardImposterCheckerEvdev::FlagIfImposter(
    EventConverterEvdev* converter) {
  if (!base::FeatureList::IsEnabled(kEnableFakeKeyboardHeuristic))
    return false;

  if (!converter->HasKeyboard() ||
      (!converter->HasMouse() &&
       devices_on_phys_path_.count(converter->input_device().phys) < 2)) {
    converter->SetSuspectedImposter(false);
    return false;
  }

  converter->SetSuspectedImposter(true);
  return true;
}

std::vector<int> KeyboardImposterCheckerEvdev::OnDeviceRemoved(
    EventConverterEvdev* converter) {
  if (!base::FeatureList::IsEnabled(kEnableFakeKeyboardHeuristic))
    return std::vector<int>();

  base::EraseIf(devices_on_phys_path_,
                [&](const auto& devices_on_phys_path_entry) {
                  return devices_on_phys_path_entry.second == converter->id();
                });
  return GetIdsOnSamePhys(converter->input_device().phys);
}

KeyboardImposterCheckerEvdev::KeyboardImposterCheckerEvdev() = default;

KeyboardImposterCheckerEvdev::~KeyboardImposterCheckerEvdev() = default;

}  // namespace ui
