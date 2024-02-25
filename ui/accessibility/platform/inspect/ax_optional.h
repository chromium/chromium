// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_OPTIONAL_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_OPTIONAL_H_

#include <string>

#include "base/component_export.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

// Used for template specialization.
template <typename T>
struct is_variant : std::false_type {};
template <typename... Args>
struct is_variant<absl::variant<Args...>> : std::true_type {};
template <typename T>
inline constexpr bool is_variant_v = is_variant<T>::value;

namespace ui {

// Implements stateful value_s. Similar to std::optional, but multi-state
// allowing nullable value_s.
template <typename ValueType>
class COMPONENT_EXPORT(AX_PLATFORM) AXOptional final {
 public:
  static constexpr AXOptional Unsupported() { return AXOptional(kUnsupported); }
  static constexpr AXOptional Error(const char* error_text = nullptr) {
    return error_text ? AXOptional(kError, error_text) : AXOptional(kError);
  }
  static constexpr AXOptional Error(const std::string& error_text) {
    return AXOptional(kError, error_text);
  }
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

  explicit constexpr AXOptional(const ValueType& value_)
      : value_(value_), state_(kValue) {}
  explicit constexpr AXOptional(ValueType&& value_)
      : value_(std::forward<ValueType>(value_)), state_(kValue) {}

  bool constexpr IsUnsupported() const { return state_ == kUnsupported; }
  bool constexpr IsNotApplicable() const { return state_ == kNotApplicable; }
  bool constexpr IsError() const { return state_ == kError; }

  template <typename T = ValueType>
  bool constexpr IsNotNull(
      typename std::enable_if<!is_variant_v<T>>::type* = 0) const {
    return value_ != nullptr;
  }

  template <typename T = ValueType>
  bool constexpr IsNotNull(
      typename std::enable_if<is_variant_v<T>>::type* = 0) const {
    return true;
  }

  bool constexpr HasValue() const { return state_ == kValue; }
  constexpr const ValueType& operator*() const { return value_; }
  constexpr const ValueType* operator->() const { return &value_; }

  bool HasStateText() const { return !state_text_.empty(); }
  std::string StateText() const { return state_text_; }

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

  template <typename T = ValueType>
  explicit constexpr AXOptional(
      State state,
      const std::string& state_text = {},
      typename std::enable_if<!is_variant_v<T>>::type* = 0)
      : value_(nullptr), state_(state), state_text_(state_text) {}

  template <typename T = ValueType>
  explicit constexpr AXOptional(
      State state,
      const std::string& state_text = {},
      typename std::enable_if<is_variant_v<T>>::type* = 0)
      : value_(absl::monostate()), state_(state), state_text_(state_text) {}

  explicit constexpr AXOptional(ValueType value, State state)
      : value_(value), state_(state) {}

  ValueType value_;
  State state_;
  std::string state_text_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_OPTIONAL_H_
