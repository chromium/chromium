// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_target_win.h"

#include "base/strings/string_number_conversions.h"

namespace ui {

AXTargetWin::AXTargetWin() = default;
AXTargetWin::AXTargetWin(std::nullptr_t) : value_(absl::monostate()) {}
AXTargetWin::AXTargetWin(const AXTargetWin&) = default;
AXTargetWin::AXTargetWin(AXTargetWin&&) = default;

AXTargetWin::~AXTargetWin() = default;

std::string AXTargetWin::ToString() const {
  if (absl::holds_alternative<IAccessibleComPtr>(value_))
    return "IAccessible";

  if (absl::holds_alternative<IA2ComPtr>(value_))
    return "IAccessible2Interface";

  if (absl::holds_alternative<IA2HypertextComPtr>(value_))
    return "IAccessible2HyperlinkInferface";

  if (absl::holds_alternative<IA2TableComPtr>(value_))
    return "IAccessible2TableInterface";

  if (absl::holds_alternative<IA2TableCellComPtr>(value_))
    return "IAccessible2TableCellInterface";

  if (absl::holds_alternative<IA2TextComPtr>(value_))
    return "IAccessible2TextInterface";

  if (absl::holds_alternative<IA2ValueComPtr>(value_))
    return "IAccessible2ValueInterface";

  if (absl::holds_alternative<std::string>(value_))
    return "\"" + absl::get<std::string>(value_) + "\"";

  if (absl::holds_alternative<int>(value_))
    return base::NumberToString(absl::get<int>(value_));

  return "Unsupported";
}

}  // namespace ui
