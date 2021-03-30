// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_TEST_COLOR_UTILS_H_
#define UI_NATIVE_THEME_TEST_COLOR_UTILS_H_

#include <ostream>
#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/native_theme/native_theme.h"

namespace ui {
namespace test {

// Struct to distinguish SkColor (aliased to uint32_t) for printing.
struct PrintableSkColor {
  bool operator==(const PrintableSkColor& other) const {
    return color == other.color;
  }

  bool operator!=(const PrintableSkColor& other) const {
    return !operator==(other);
  }

  const SkColor color;
};

// Outputs a text representation of `printable_color` to `os`.
std::ostream& operator<<(std::ostream& os, PrintableSkColor printable_color);

// Returns `id` as a readable string.
std::string ColorIdToString(NativeTheme::ColorId id);

}  // namespace test
}  // namespace ui

#endif  // UI_NATIVE_THEME_TEST_COLOR_UTILS_H_
