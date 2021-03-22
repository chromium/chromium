// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/test/color_utils.h"

#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "ui/native_theme/native_theme_color_id.h"

namespace ui {
namespace test {

namespace {

constexpr const char* kColorIdStringName[] = {
#define OP(enum_name) #enum_name
    NATIVE_THEME_COLOR_IDS
#undef OP
};

}  // namespace

std::ostream& operator<<(std::ostream& os, PrintableSkColor printable_color) {
  SkColor color = printable_color.color;
  return os << base::StringPrintf("SkColorARGB(0x%02x, 0x%02x, 0x%02x, 0x%02x)",
                                  SkColorGetA(color), SkColorGetR(color),
                                  SkColorGetG(color), SkColorGetB(color));
}

std::string ColorIdToString(NativeTheme::ColorId id) {
  if (id >= NativeTheme::ColorId::kColorId_NumColors) {
    NOTREACHED() << "Invalid color value " << id;
    return "InvalidColorId";
  }
  return kColorIdStringName[id];
}

}  // namespace test
}  // namespace ui
