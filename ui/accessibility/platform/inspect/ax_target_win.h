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
#include "ui/accessibility/platform/iaccessible2/scoped_co_mem_array.h"

namespace ui {

using IAccessibleComPtr = Microsoft::WRL::ComPtr<IAccessible>;
using IA2ComPtr = Microsoft::WRL::ComPtr<IAccessible2>;
using IA2HypertextComPtr = Microsoft::WRL::ComPtr<IAccessibleHypertext>;
using IA2TableComPtr = Microsoft::WRL::ComPtr<IAccessibleTable>;
using IA2TableCellComPtr = Microsoft::WRL::ComPtr<IAccessibleTableCell>;
using IA2TextComPtr = Microsoft::WRL::ComPtr<IAccessibleText>;
using IA2TextSelectionContainerComPtr =
    Microsoft::WRL::ComPtr<IAccessibleTextSelectionContainer>;
using IA2ValueComPtr = Microsoft::WRL::ComPtr<IAccessibleValue>;

class COMPONENT_EXPORT(AX_PLATFORM) AXTargetWin final {
 public:
  AXTargetWin();
  AXTargetWin(std::nullptr_t);
  AXTargetWin(const AXTargetWin&);
  AXTargetWin(AXTargetWin&&);

  template <typename Type>
  constexpr AXTargetWin(Type&& v)
      : value_(std::make_shared<VariantType>(std::move(v))) {}

  ~AXTargetWin();

  template <typename Type>
  bool Is() const {
    return value_ && absl::holds_alternative<Type>(*value_);
  }

  template <typename Type>
  const Type& As() const {
    return absl::get<Type>(*value_);
  }

  std::string ToString() const;

  AXTargetWin& operator=(const AXTargetWin&) = default;
  AXTargetWin& operator=(AXTargetWin&&) = default;
  constexpr bool operator!() const { return value_ == nullptr; }

  friend bool operator!=(const AXTargetWin& lhs, const AXTargetWin& rhs) {
    return !(lhs.value_ == rhs.value_);
  }

 private:
  using VariantType = absl::variant<std::string,
                                    int,
                                    IAccessibleComPtr,
                                    IA2ComPtr,
                                    IA2HypertextComPtr,
                                    IA2TableComPtr,
                                    IA2TableCellComPtr,
                                    IA2TextComPtr,
                                    IA2TextSelectionContainerComPtr,
                                    IA2ValueComPtr,
                                    ScopedCoMemArray<LONG>,
                                    ScopedCoMemArray<IA2TextSelection>>;

  // Keep the value const to prevent accidental change of the value shared
  // between multiple instances of AXTargetWin.
  std::shared_ptr<const VariantType> value_;  // nocheck
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TARGET_WIN_H_
