// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_OPTIONAL_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_OPTIONAL_H_

#include <string>

#include "build/build_config.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

// Implements stateful value_s. Similar to absl::optional, but multi-state
// allowing nullable value_s.
template <typename ValueType>
class AX_EXPORT AXOptional final {
 public:
  static constexpr AXOptional Unsupported() { return AXOptional(kUnsupported); }
  static constexpr AXOptional Error() { return AXOptional(kError); }
  static constexpr AXOptional NotApplicable() {
    return AXOptional(kNotApplicable);
  }
  static constexpr AXOptional NotNullOrError(ValueType other_value_) {
    return AXOptional(other_value_, other_value_ != nullptr ? kValue : kError);
  }
  static constexpr AXOptional NotNullOrNotApplicable(ValueType other_value_) {
    return AXOptional(other_value_,
                      other_value_ != nullptr ? kValue : kNotApplicable);
  }

  explicit constexpr AXOptional(ValueType value_)
      : value_(value_), flag_(kValue) {}

  bool constexpr IsUnsupported() const { return flag_ == kUnsupported; }
  bool constexpr IsNotApplicable() const { return flag_ == kNotApplicable; }
  bool constexpr IsError() const { return flag_ == kError; }

  template <typename T = ValueType>
  bool constexpr IsNotNull(
      typename std::enable_if<std::is_pointer<T>::value>::type* = 0) const {
    return value_ != nullptr;
  }

  template <typename T = ValueType>
  bool constexpr IsNotNull(
      typename std::enable_if<!std::is_pointer<T>::value>::type* = 0) const {
    return true;
  }

  bool constexpr HasValue() { return flag_ == kValue; }
  constexpr const ValueType& operator*() const { return value_; }

  std::string ToString() const {
    if (IsNotNull())
      return "<value>";
    return StateToString();
  }

  std::string StateToString() const {
    if (IsNotApplicable())
      return "<n/a>";
    if (IsUnsupported())
      return "<unsupported>";
    if (IsError())
      return "<error>";
    if (!IsNotNull())
      return "<null>";
    return "";
  }

 private:
  enum State {
    // Indicates a valid value_; can be null.
    kValue,

    // Indicates an error, such as call or parser errors.
    kError,

    // Indicates a called property is not applicable to the object.
    kNotApplicable,

    // Indicates the property can't have an associated object.
    kUnsupported,
  };

  explicit constexpr AXOptional(State flag_) : value_(nullptr), flag_(flag_) {}
  explicit constexpr AXOptional(ValueType value_, State flag_)
      : value_(value_), flag_(flag_) {}

  ValueType value_;
  State flag_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_OPTIONAL_H_
