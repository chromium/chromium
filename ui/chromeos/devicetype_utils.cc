// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/devicetype_utils.h"

#include "ash/constants/devicetype.h"
#include "base/notreached.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace ui {

base::string16 SubstituteChromeOSDeviceType(int resource_id) {
  return l10n_util::GetStringFUTF16(resource_id, GetChromeOSDeviceName());
}

base::string16 GetChromeOSDeviceName() {
  return l10n_util::GetStringUTF16(GetChromeOSDeviceTypeResourceId());
}

int GetChromeOSDeviceTypeResourceId() {
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

  NOTREACHED();
  return IDS_GENERIC_CHROMEOS_DEVICE_NAME;
}

base::string16 GetChromeOSDeviceNameInPlural() {
  return l10n_util::GetStringUTF16(GetChromeOSDeviceTypeInPluralResourceId());
}

int GetChromeOSDeviceTypeInPluralResourceId() {
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

  NOTREACHED();
  return IDS_GENERIC_CHROMEOS_DEVICE_NAME_IN_PLURAL;
}

}  // namespace ui
