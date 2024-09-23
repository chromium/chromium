// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_api_type.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"

namespace ui {

namespace {

// These strings are stored in prefs for chrome://accessibility, do not rename.
// When adding a new type, add a new entry here and then update the methods that
// convert to/from string.
static constexpr std::string_view kAndroidString{"android"};
static constexpr std::string_view kAndroidExternalString{"android_external"};
static constexpr std::string_view kBlinkString{"blink"};
static constexpr std::string_view kFuchsiaString{"fuchsia"};
static constexpr std::string_view kMacString{"mac"};
static constexpr std::string_view kLinuxString{"linux"};
static constexpr std::string_view kWinIA2String{"ia2"};
static constexpr std::string_view kWinUIAString{"uia"};

}  // Namespace

AXApiType::Type::operator std::string_view() const {
  switch (type_) {
    case kNone:
      NOTREACHED();
    case kAndroid:
      return kAndroidString;
    case kAndroidExternal:
      return kAndroidExternalString;
    case kBlink:
      return kBlinkString;
    case kFuchsia:
      return kFuchsiaString;
    case kMac:
      return kMacString;
    case kLinux:
      return kLinuxString;
    case kWinIA2:
      return kWinIA2String;
    case kWinUIA:
      return kWinUIAString;
  }
}

AXApiType::Type::operator std::string() const {
  return std::string(std::string_view(*this));
}

// static
AXApiType::Type AXApiType::From(const std::string& type_str) {
  static constexpr auto kTypeToString =
      base::MakeFixedFlatMap<std::string_view, TypeConstant>(
          {{kAndroidString, kAndroid},
           {kAndroidExternalString, kAndroidExternal},
           {kBlinkString, kBlink},
           {kFuchsiaString, kFuchsia},
           {kMacString, kMac},
           {kLinuxString, kLinux},
           {kWinIA2String, kWinIA2},
           {kWinUIAString, kWinUIA}});
  auto it = kTypeToString.find(type_str);
  if (it == kTypeToString.end()) {
    NOTREACHED();
  }
  return it->second;
}

}  // namespace ui
