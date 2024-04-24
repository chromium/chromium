// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include <CoreText/CoreText.h>
#import <Foundation/Foundation.h>

#include <string_view>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/i18n/char_iterator.h"
#import "base/mac/mac_util.h"
#import "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback_skia_impl.h"
#include "ui/gfx/platform_font.h"

namespace gfx {

namespace {

bool TextSequenceHasEmoji(std::u16string_view text) {
  for (base::i18n::UTF16CharIterator iter(text); !iter.end(); iter.Advance()) {
    const UChar32 codepoint = iter.get();
    if (u_hasBinaryProperty(codepoint, UCHAR_EMOJI))
      return true;
  }
  return false;
}

}  // namespace

std::vector<Font> GetFallbackFonts(const Font& font) {
  DCHECK(font.GetCTFont());
  // On Mac "There is a system default cascade list (which is polymorphic, based
  // on the user's language setting and current font)" - CoreText Programming
  // Guide.
  NSArray* languages =
      [NSUserDefaults.standardUserDefaults stringArrayForKey:@"AppleLanguages"];
  CFArrayRef languages_cf = base::apple::NSToCFPtrCast(languages);
  base::apple::ScopedCFTypeRef<CFArrayRef> cascade_list(
      CTFontCopyDefaultCascadeListForLanguages(font.GetCTFont(), languages_cf));

  std::vector<Font> fallback_fonts;
  if (!cascade_list)
    return fallback_fonts;  // This should only happen for an invalid |font|.

  const CFIndex fallback_count = CFArrayGetCount(cascade_list.get());
  for (CFIndex i = 0; i < fallback_count; ++i) {
    CTFontDescriptorRef descriptor =
        base::apple::CFCastStrict<CTFontDescriptorRef>(
            CFArrayGetValueAtIndex(cascade_list.get(), i));
    base::apple::ScopedCFTypeRef<CTFontRef> fallback_font(
        CTFontCreateWithFontDescriptor(descriptor, 0.0, nullptr));
    if (fallback_font.get()) {
      fallback_fonts.emplace_back(fallback_font.get());
    }
  }

  if (fallback_fonts.empty())
    return std::vector<Font>(1, font);

  return fallback_fonts;
}

bool GetFallbackFont(const Font& font,
                     const std::string& locale,
                     std::u16string_view text,
                     Font* result) {
  TRACE_EVENT0("fonts", "gfx::GetFallbackFont");

  if (TextSequenceHasEmoji(text)) {
    *result = Font("Apple Color Emoji", font.GetFontSize());
    return true;
  }

  sk_sp<SkTypeface> fallback_typeface =
      GetSkiaFallbackTypeface(font, locale, text);

  if (!fallback_typeface)
    return false;

  // Fallback needs to keep the exact SkTypeface, as re-matching the font using
  // family name and styling information loses access to the underlying platform
  // font handles and is not guaranteed to result in the correct typeface, see
  // https://crbug.com/1003829
  *result = Font(PlatformFont::CreateFromSkTypeface(
      std::move(fallback_typeface), font.GetFontSize(), std::nullopt));
  return true;
}

}  // namespace gfx
