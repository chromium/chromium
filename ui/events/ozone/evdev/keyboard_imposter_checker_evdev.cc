// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/keyboard_imposter_checker_evdev.h"

#include "base/containers/cxx20_erase_map.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (converter->GetKeyboardType() == KeyboardType::IN_BLOCKLIST) {
    fake_keyboard_heuristic_metrics_.RecordUsage(false);
  }
#endif

  if (!converter->HasKeyboard() ||
      (!converter->HasMouse() &&
       devices_on_phys_path_.count(converter->input_device().phys) < 2)) {
    converter->SetSuspectedImposter(false);
    return false;
  }

  converter->SetSuspectedImposter(true);
  VLOG(1) << "Device Name: " << converter->input_device().name << " Vendor ID: "
          << base::StringPrintf("%#06x", converter->input_device().vendor_id)
          << " Product ID: "
          << base::StringPrintf("%#06x", converter->input_device().product_id)
          << " has been flagged as a suspected imposter keyboard";
#if BUILDFLAG(IS_CHROMEOS_ASH)
  fake_keyboard_heuristic_metrics_.RecordUsage(true);
#endif
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
