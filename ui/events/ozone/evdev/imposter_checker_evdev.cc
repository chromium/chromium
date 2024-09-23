// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/imposter_checker_evdev.h"

#include <map>

#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/events/ozone/evdev/imposter_checker_evdev_state.h"
#include "ui/events/ozone/features.h"

namespace ui {

namespace {

std::string StandardizedPhys(const std::string& phys_path) {
  // For input devices on USB, remove the final digits in the phys_path. This
  // means devices with the same USB topology will have identical phys_paths.
  std::string standard_phys = phys_path;
  static constexpr re2::LazyRE2 usb_input_re("^(usb[-:.0-9]*/input)[0-9]*$");
  re2::RE2::Replace(&standard_phys, *usb_input_re, "\\1");
  return standard_phys;
}

}  // namespace

std::vector<int> ImposterCheckerEvdev::GetIdsOnSamePhys(
    const std::string& phys_path) {
  std::vector<int> ids_on_same_phys;
  auto iterators = devices_on_phys_path_.equal_range(phys_path);
  for (auto& it = iterators.first; it != iterators.second; ++it) {
    ids_on_same_phys.push_back(it->second);
  }
  return ids_on_same_phys;
}

bool ImposterCheckerEvdev::IsSuspectedKeyboardImposter(
    EventConverterEvdev* converter,
    bool shared_phys) {
  if (!imposter_checker_evdev_state_->IsKeyboardCheckEnabled()) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (converter->GetKeyboardType() == KeyboardType::IN_BLOCKLIST) {
    fake_keyboard_heuristic_metrics_.RecordUsage(false);
  }
#endif
  if (!converter->HasKeyboard() || (!converter->HasMouse() && !shared_phys) ||
      converter->type() == InputDeviceType::INPUT_DEVICE_INTERNAL) {
    return false;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  fake_keyboard_heuristic_metrics_.RecordUsage(true);
#endif
  return true;
}

bool ImposterCheckerEvdev::IsSuspectedMouseImposter(
    EventConverterEvdev* converter,
    bool shared_phys) {
  if (!base::FeatureList::IsEnabled(kEnableFakeMouseHeuristic)) {
    return false;
  }

  if (!converter->HasMouse() || (!converter->HasKeyboard() && !shared_phys)) {
    return false;
  }
  return true;
}

std::vector<int> ImposterCheckerEvdev::OnDeviceAdded(
    EventConverterEvdev* converter) {
  std::string standard_phys = StandardizedPhys(converter->input_device().phys);
  devices_on_phys_path_.emplace(standard_phys, converter->id());
  return GetIdsOnSamePhys(standard_phys);
}

bool ImposterCheckerEvdev::FlagSuspectedImposter(
    EventConverterEvdev* converter) {
  bool shared_phys = devices_on_phys_path_.count(
                         StandardizedPhys(converter->input_device().phys)) > 1;

  bool is_keyboard_imposter =
      IsSuspectedKeyboardImposter(converter, shared_phys);
  converter->SetSuspectedKeyboardImposter(is_keyboard_imposter);
  bool is_mouse_imposter = IsSuspectedMouseImposter(converter, shared_phys);
  converter->SetSuspectedMouseImposter(is_mouse_imposter);

  if (is_keyboard_imposter) {
    LOG(WARNING) << "Device Name: " << converter->input_device().name
                 << " Vendor ID: "
                 << base::StringPrintf("0x%04x",
                                       converter->input_device().vendor_id)
                 << " Product ID: "
                 << base::StringPrintf("0x%04x",
                                       converter->input_device().product_id)
                 << " has been flagged as a suspected imposter keyboard";
  }

  if (is_mouse_imposter) {
    LOG(WARNING) << "Device Name: " << converter->input_device().name
                 << " Vendor ID: "
                 << base::StringPrintf("0x%04x",
                                       converter->input_device().vendor_id)
                 << " Product ID: "
                 << base::StringPrintf("0x%04x",
                                       converter->input_device().product_id)
                 << " has been flagged as a suspected imposter mouse";
  }

  return is_keyboard_imposter || is_mouse_imposter;
}

std::vector<int> ImposterCheckerEvdev::OnDeviceRemoved(
    EventConverterEvdev* converter) {
  std::erase_if(devices_on_phys_path_,
                [&](const auto& devices_on_phys_path_entry) {
                  return devices_on_phys_path_entry.second == converter->id();
                });
  return GetIdsOnSamePhys(StandardizedPhys(converter->input_device().phys));
}

ImposterCheckerEvdev::ImposterCheckerEvdev()
    : imposter_checker_evdev_state_(
          std::make_unique<ImposterCheckerEvdevState>()) {}

ImposterCheckerEvdev::~ImposterCheckerEvdev() = default;

}  // namespace ui
