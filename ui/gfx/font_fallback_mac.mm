// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include <CoreText/CoreText.h>
#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/strings/sys_string_conversions.h"
#include "ui/gfx/font.h"

namespace gfx {
namespace {

// CTFontCreateForString() sometimes re-wraps its result in a new CTFontRef with
// identical attributes. This wastes time shaping the text run and confounds
// Skia's internal typeface cache.
bool FontsEqual(CTFontRef lhs, CTFontRef rhs) {
  if (lhs == rhs)
    return true;

  // Compare ATSFontRef typeface IDs. These are typedef uint32_t. Typically if
  // RenderText decided to hunt for a fallback in the first place, this check
  // fails and FontsEqual returns here.
  if (CTFontGetPlatformFont(lhs, nil) != CTFontGetPlatformFont(rhs, nil))
    return false;

  // Comparing addresses of descriptors seems to be sufficient for other cases.
  base::ScopedCFTypeRef<CTFontDescriptorRef> lhs_descriptor(
      CTFontCopyFontDescriptor(lhs));
  base::ScopedCFTypeRef<CTFontDescriptorRef> rhs_descriptor(
      CTFontCopyFontDescriptor(rhs));
  return lhs_descriptor.get() == rhs_descriptor.get();
}

}  // namespace

std::vector<Font> GetFallbackFonts(const Font& font) {
  DCHECK(font.GetNativeFont());
  // On Mac "There is a system default cascade list (which is polymorphic, based
  // on the user's language setting and current font)" - CoreText Programming
  // Guide.
  NSArray* languages = [[NSUserDefaults standardUserDefaults]
      stringArrayForKey:@"AppleLanguages"];
  CFArrayRef languages_cf = base::mac::NSToCFCast(languages);
  base::ScopedCFTypeRef<CFArrayRef> cascade_list(
      CTFontCopyDefaultCascadeListForLanguages(
          static_cast<CTFontRef>(font.GetNativeFont()), languages_cf));

  std::vector<Font> fallback_fonts;
  if (!cascade_list)
    return fallback_fonts;  // This should only happen for an invalid |font|.

  const CFIndex fallback_count = CFArrayGetCount(cascade_list);
  for (CFIndex i = 0; i < fallback_count; ++i) {
    CTFontDescriptorRef descriptor =
        base::mac::CFCastStrict<CTFontDescriptorRef>(
            CFArrayGetValueAtIndex(cascade_list, i));
    base::ScopedCFTypeRef<CTFontRef> fallback_font(
        CTFontCreateWithFontDescriptor(descriptor, 0.0, nullptr));
    if (fallback_font.get())
      fallback_fonts.push_back(Font(static_cast<NSFont*>(fallback_font.get())));
  }

  if (fallback_fonts.empty())
    return std::vector<Font>(1, font);

  return fallback_fonts;
}

bool GetFallbackFont(const Font& font,
                     const std::string& locale,
                     base::StringPiece16 text,
                     Font* result) {
  base::ScopedCFTypeRef<CFStringRef> cf_string(CFStringCreateWithCharacters(
      kCFAllocatorDefault, text.data(), text.length()));
  CTFontRef ct_font = base::mac::NSToCFCast(font.GetNativeFont());
  base::ScopedCFTypeRef<CTFontRef> ct_result(
      CTFontCreateForString(ct_font, cf_string, {0, text.length()}));
  if (FontsEqual(ct_font, ct_result))
    return false;

  *result = Font(base::mac::CFToNSCast(ct_result.get()));
  return true;
}

}  // namespace gfx
