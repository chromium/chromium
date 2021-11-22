// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_api_type.h"

namespace ui {

AXApiType::Type::operator std::string() const {
  switch (type_) {
    case kAndroid:
      return "android";
    case kAndroidExternal:
      return "android_external";
    case kBlink:
      return "blink";
    case kFuchsia:
      return "fuchsia";
    case kMac:
      return "mac";
    case kLinux:
      return "linux";
    case kWinIA2:
      return "ia2";
    case kWinUIA:
      return "uia";
    default:
      return "unknown";
  }
}

}  // namespace ui
