// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DEVICE_FORM_FACTOR_H_
#define UI_BASE_DEVICE_FORM_FACTOR_H_

#include "base/component_export.h"

namespace ui {

enum DeviceFormFactor {
  DEVICE_FORM_FACTOR_DESKTOP = 0,
  DEVICE_FORM_FACTOR_PHONE = 1,
  DEVICE_FORM_FACTOR_TABLET = 2,
  DEVICE_FORM_FACTOR_TV = 3,
  DEVICE_FORM_FACTOR_AUTOMOTIVE = 4,
  DEVICE_FORM_FACTOR_FOLDABLE = 5,
};

// Returns the form factor of current device. For platforms other than Android
// and iOS, DEVICE_FORM_FACTOR_DESKTOP is always returned.
COMPONENT_EXPORT(UI_BASE) DeviceFormFactor GetDeviceFormFactor();

}  // namespace ui

#endif  // UI_BASE_DEVICE_FORM_FACTOR_H_
