// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/devicetype_utils.h"

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/constants/devicetype.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace ui {

std::u16string SubstituteChromeOSDeviceType(int resource_id) {
  return l10n_util::GetStringFUTF16(resource_id, GetChromeOSDeviceName());
}

std::u16string GetChromeOSDeviceName() {
  return l10n_util::GetStringUTF16(GetChromeOSDeviceTypeResourceId());
}

int GetChromeOSDeviceTypeResourceId() {
#if BUILDFLAG(IS_REVEN)
  return IDS_REVEN_DEVICE_NAME;
#else
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebase:
      return IDS_CHROMEBASE_DEVICE_NAME;
    case chromeos::DeviceType::kChromebook:
      return IDS_CHROMEBOOK_DEVICE_NAME;
    case chromeos::DeviceType::kChromebox:
      return IDS_CHROMEBOX_DEVICE_NAME;
    case chromeos::DeviceType::kChromebit:
      return IDS_CHROMEBIT_DEVICE_NAME;
    case chromeos::DeviceType::kUnknown:
      return IDS_GENERIC_CHROMEOS_DEVICE_NAME;
  }

  NOTREACHED_IN_MIGRATION();
  return IDS_GENERIC_CHROMEOS_DEVICE_NAME;
#endif
}

std::u16string GetChromeOSDeviceNameInPlural() {
  return l10n_util::GetStringUTF16(GetChromeOSDeviceTypeInPluralResourceId());
}

int GetChromeOSDeviceTypeInPluralResourceId() {
#if BUILDFLAG(IS_REVEN)
  return IDS_REVEN_DEVICE_NAME_IN_PLURAL;
#else
  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebase:
      return IDS_CHROMEBASE_DEVICE_NAME_IN_PLURAL;
    case chromeos::DeviceType::kChromebook:
      return IDS_CHROMEBOOK_DEVICE_NAME_IN_PLURAL;
    case chromeos::DeviceType::kChromebox:
      return IDS_CHROMEBOX_DEVICE_NAME_IN_PLURAL;
    case chromeos::DeviceType::kChromebit:
      return IDS_CHROMEBIT_DEVICE_NAME_IN_PLURAL;
    case chromeos::DeviceType::kUnknown:
      return IDS_GENERIC_CHROMEOS_DEVICE_NAME_IN_PLURAL;
  }

  NOTREACHED_IN_MIGRATION();
  return IDS_GENERIC_CHROMEOS_DEVICE_NAME_IN_PLURAL;
#endif
}

}  // namespace ui
