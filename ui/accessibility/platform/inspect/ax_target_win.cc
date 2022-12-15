// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_target_win.h"

#include "base/strings/string_number_conversions.h"

namespace ui {

AXTargetWin::AXTargetWin() = default;
AXTargetWin::AXTargetWin(std::nullptr_t) : value_(nullptr) {}
AXTargetWin::AXTargetWin(const AXTargetWin&) = default;
AXTargetWin::AXTargetWin(AXTargetWin&&) = default;

AXTargetWin::~AXTargetWin() = default;

std::string AXTargetWin::ToString() const {
  if (!value_)
    return "NULL";

  if (Is<IAccessibleComPtr>())
    return "IAccessible";

  if (Is<IA2ComPtr>())
    return "IAccessible2Interface";

  if (Is<IA2HypertextComPtr>())
    return "IAccessible2HyperlinkInferface";

  if (Is<IA2TableComPtr>())
    return "IAccessible2TableInterface";

  if (Is<IA2TableCellComPtr>())
    return "IAccessible2TableCellInterface";

  if (Is<IA2TextComPtr>())
    return "IAccessible2TextInterface";

  if (Is<IA2ValueComPtr>())
    return "IAccessible2ValueInterface";

  if (Is<std::string>())
    return "\"" + As<std::string>() + "\"";

  if (Is<int>())
    return base::NumberToString(As<int>());

  if (Is<ScopedCoMemArray<LONG>>()) {
    std::string str;
    for (LONG value : As<ScopedCoMemArray<LONG>>()) {
      if (!str.empty()) {
        str += ", ";
      }
      str += base::NumberToString(value);
    }
    return '[' + str + ']';
  }

  return "Unsupported";
}

}  // namespace ui
