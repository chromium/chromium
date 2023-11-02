// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/text_utils.h"

#import <UIKit/UIKit.h>

#include <cmath>

#include "base/strings/sys_string_conversions.h"
#include "ui/gfx/font_list.h"

namespace gfx {

int GetStringWidth(const std::u16string& text, const FontList& font_list) {
  return std::ceil(GetStringWidthF(text, font_list));
}

float GetStringWidthF(const std::u16string& text, const FontList& font_list) {
  NSString* ns_text = base::SysUTF16ToNSString(text);
  NativeFont native_font = font_list.GetPrimaryFont().GetNativeFont();
  NSDictionary* attributes = @{NSFontAttributeName : native_font};
  return [ns_text sizeWithAttributes:attributes].width;
}

}  // namespace gfx
