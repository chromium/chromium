// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_DEVICETYPE_UTILS_H_
#define UI_CHROMEOS_DEVICETYPE_UTILS_H_

#include "base/strings/string16.h"
#include "ui/chromeos/ui_chromeos_export.h"

namespace ui {

// Assuming the given localization resources takes a device type parameter, this
// will substitute the appropriate device in (e.g. Chromebook, Chromebox).
UI_CHROMEOS_EXPORT base::string16 SubstituteChromeOSDeviceType(int resource_id);

// Returns the name of the Chrome device type (e.g. Chromebook, Chromebox).
UI_CHROMEOS_EXPORT base::string16 GetChromeOSDeviceName();

// Returns the resource ID for the current Chrome device type (e.g. Chromebook,
// Chromebox).
UI_CHROMEOS_EXPORT int GetChromeOSDeviceTypeResourceId();

// Returns the name (plural forms) of the Chrome device type (e.g. Chromebooks,
// Chromeboxes).
UI_CHROMEOS_EXPORT base::string16 GetChromeOSDeviceNameInPlural();

// Returns the resource ID for the current Chrome device type in plural forms
// (e.g. Chromebooks, Chromeboxes).
UI_CHROMEOS_EXPORT int GetChromeOSDeviceTypeInPluralResourceId();

}  // namespace ui

#endif  // UI_CHROMEOS_DEVICETYPE_UTILS_H_
