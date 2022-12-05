// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TARGET_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TARGET_WIN_H_

#include <string>

#include <wrl/client.h>

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/iaccessible2/ia2_api_all.h"

namespace ui {

using IAccessibleComPtr = Microsoft::WRL::ComPtr<IAccessible>;
using IA2ComPtr = Microsoft::WRL::ComPtr<IAccessible2>;
using IA2HypertextComPtr = Microsoft::WRL::ComPtr<IAccessibleHypertext>;
using IA2TableComPtr = Microsoft::WRL::ComPtr<IAccessibleTable>;
using IA2TableCellComPtr = Microsoft::WRL::ComPtr<IAccessibleTableCell>;
using IA2TextComPtr = Microsoft::WRL::ComPtr<IAccessibleText>;
using IA2ValueComPtr = Microsoft::WRL::ComPtr<IAccessibleValue>;

class COMPONENT_EXPORT(AX_PLATFORM) AXTargetWin final {
 public:
  AXTargetWin();
  AXTargetWin(std::nullptr_t);
  AXTargetWin(const AXTargetWin&);
  AXTargetWin(AXTargetWin&&);

  template <typename Type>
  constexpr AXTargetWin(Type&& v) : value_(std::forward<Type>(v)) {}

  ~AXTargetWin();

  template <typename Type>
  bool Is() const {
    return absl::holds_alternative<Type>(value_);
  }

  template <typename Type>
  const Type& As() const {
    return absl::get<Type>(value_);
  }

  std::string ToString() const;

  AXTargetWin& operator=(const AXTargetWin&) = default;
  AXTargetWin& operator=(AXTargetWin&&) = default;
  constexpr bool operator!() const { return value_.index() == 0; }

  friend bool operator!=(const AXTargetWin& lhs, const AXTargetWin& rhs) {
    return !(lhs.value_ == rhs.value_);
  }

 private:
  absl::variant<absl::monostate,
                std::string,
                int,
                IAccessibleComPtr,
                IA2ComPtr,
                IA2HypertextComPtr,
                IA2TableComPtr,
                IA2TableCellComPtr,
                IA2TextComPtr,
                IA2ValueComPtr>
      value_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TARGET_WIN_H_
