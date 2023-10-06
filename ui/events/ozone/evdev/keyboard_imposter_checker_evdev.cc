// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/keyboard_imposter_checker_evdev.h"

#include "base/containers/cxx20_erase_map.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/re2/src/re2/re2.h"
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

std::string KeyboardImposterCheckerEvdev::StandardizedPhys(
    const std::string& phys_path) {
  // For input devices on USB, remove the final digits in the phys_path. This
  // means devices with the same USB topology will have identical phys_paths.
  std::string standard_phys = phys_path;
  static constexpr re2::LazyRE2 usb_input_re("^(usb[-:.0-9]*/input)[0-9]*$");
  re2::RE2::Replace(&standard_phys, *usb_input_re, "\\1");
  return standard_phys;
}

std::vector<int> KeyboardImposterCheckerEvdev::OnDeviceAdded(
    EventConverterEvdev* converter) {
  if (!base::FeatureList::IsEnabled(kEnableFakeKeyboardHeuristic))
    return std::vector<int>();

  std::string standard_phys = StandardizedPhys(converter->input_device().phys);
  devices_on_phys_path_.emplace(standard_phys, converter->id());
  return GetIdsOnSamePhys(standard_phys);
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

  bool shared_phys = devices_on_phys_path_.count(
                         StandardizedPhys(converter->input_device().phys)) > 1;
  if (!converter->HasKeyboard() || (!converter->HasMouse() && !shared_phys)) {
    converter->SetSuspectedImposter(false);
    return false;
  }

  converter->SetSuspectedImposter(true);
  LOG(WARNING) << "Device Name: " << converter->input_device().name
               << " Vendor ID: "
               << base::StringPrintf("0x%04x",
                                     converter->input_device().vendor_id)
               << " Product ID: "
               << base::StringPrintf("0x%04x",
                                     converter->input_device().product_id)
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
  return GetIdsOnSamePhys(StandardizedPhys(converter->input_device().phys));
}

KeyboardImposterCheckerEvdev::KeyboardImposterCheckerEvdev() = default;

KeyboardImposterCheckerEvdev::~KeyboardImposterCheckerEvdev() = default;

}  // namespace ui
