// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/text_utils.h"

#import <CoreText/CoreText.h>
#import <UIKit/UIKit.h>

#include <cmath>
#include <string_view>

#include "base/apple/bridging.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/gfx/font_list.h"

namespace gfx {

int GetStringWidth(std::u16string_view text, const FontList& font_list) {
  return std::ceil(GetStringWidthF(text, font_list));
}

float GetStringWidthF(std::u16string_view text, const FontList& font_list) {
  NSString* ns_text = base::SysUTF16ToNSString(text);
  CTFontRef font = font_list.GetPrimaryFont().GetCTFont();
  NSDictionary* attributes =
      @{NSFontAttributeName : base::apple::CFToNSPtrCast(font)};
  return [ns_text sizeWithAttributes:attributes].width;
}

}  // namespace gfx
