// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/color_utils_android.h"

#include "base/check.h"
#include "base/numerics/safe_math.h"
#include "ui/gfx/color_utils.h"

namespace ui {

std::string OptionalSkColorToString(const std::optional<SkColor>& color) {
  if (!color)
    return std::string();
  return color_utils::SkColorToRgbaString(*color);
}

int64_t OptionalSkColorToJavaColor(const std::optional<SkColor>& skcolor) {
  if (!skcolor)
    return kInvalidJavaColor;
  return static_cast<int32_t>(*skcolor);
}

std::optional<SkColor> JavaColorToOptionalSkColor(int64_t java_color) {
  if (java_color == kInvalidJavaColor)
    return std::nullopt;
  DCHECK(base::IsValueInRangeForNumericType<int32_t>(java_color));
  return static_cast<SkColor>(java_color);
}

}  // namespace ui
