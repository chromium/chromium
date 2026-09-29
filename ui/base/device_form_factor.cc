// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/device_form_factor.h"

#include <string_view>

namespace ui {

std::string_view DeviceFormFactorToString(DeviceFormFactor device_form_factor) {
  // The following values are transmitted / retained in telemetry; avoid
  // changing existing mappings.
  switch (device_form_factor) {
    case DEVICE_FORM_FACTOR_DESKTOP:
      return "desktop";
    case DEVICE_FORM_FACTOR_PHONE:
      return "phone";
    case DEVICE_FORM_FACTOR_TABLET:
      return "tablet";
    case DEVICE_FORM_FACTOR_TV:
      return "tv";
    case DEVICE_FORM_FACTOR_AUTOMOTIVE:
      return "automotive";
    case DEVICE_FORM_FACTOR_FOLDABLE:
      return "foldable";
    case DEVICE_FORM_FACTOR_XR:
      return "xr";
  }
}

}  // namespace ui
