// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_api_type.h"

#include <cstring>

namespace ui {

namespace {

struct TypeStr {
  const char* type_str;
  AXApiType::TypeConstant type;
};

constexpr TypeStr kTypeStringMap[] = {
    {"android", AXApiType::kAndroid},
    {"android_external", AXApiType::kAndroidExternal},
    {"blink", AXApiType::kBlink},
    {"fuchsia", AXApiType::kFuchsia},
    {"mac", AXApiType::kMac},
    {"linux", AXApiType::kLinux},
    {"ia2", AXApiType::kWinIA2},
    {"uia", AXApiType::kWinUIA},
};

}  // Namespace

AXApiType::Type::operator std::string() const {
  for (const auto& info : kTypeStringMap) {
    if (info.type == type_) {
      return info.type_str;
    }
  }
  return "unknown";
}

// static
AXApiType::Type AXApiType::From(std::string& type_str) {
  const char* c_type_str = type_str.c_str();
  for (const auto& info : kTypeStringMap) {
    if (std::strcmp(info.type_str, c_type_str) == 0) {
      return info.type;
    }
  }
  return AXApiType::kNone;
}

}  // namespace ui
