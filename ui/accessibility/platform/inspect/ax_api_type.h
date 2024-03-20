// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_API_TYPE_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_API_TYPE_H_

#include <string>
#include <string_view>

#include "base/component_export.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) AXApiType {
 public:
  // Inspect types for all platforms.
  enum TypeConstant {
    kNone,
    kAndroid,
    kAndroidExternal,  // For the Java-side "external" Android tree.
    kBlink,
    kFuchsia,
    kMac,
    kLinux,
    kWinIA2,
    kWinUIA,
  };

  // Type represents a platform-specific accessibility API.
  class COMPONENT_EXPORT(AX_PLATFORM) Type final {
   public:
    Type(TypeConstant type) : type_(type) {}

    ~Type() = default;

    Type(const Type&) = default;
    Type& operator=(const Type&) = default;

    explicit operator std::string_view() const;
    explicit operator std::string() const;
    operator TypeConstant() const { return type_; }

   private:
    TypeConstant type_;
  };

  // Conversion from string to AXApiType::Type.
  static Type From(const std::string& type_str);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_API_TYPE_H_
