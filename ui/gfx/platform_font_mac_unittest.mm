// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_mac.h"

#include <Cocoa/Cocoa.h>
#include <CoreText/CoreText.h>
#include <stddef.h>

#include "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"

namespace gfx {

using Weight = Font::Weight;

TEST(PlatformFontMacTest, DeriveFont) {
  // macOS 13.0 bug: For non-system fonts with 0-valued traits,
  // `kCFBooleanFalse` is used instead of a `CFNumberRef` of 0. See
  // https://crbug.com/1372420. Filed as FB11673021, fixed in macOS 13.1.
  auto GetValueFromDictionaryAndWorkAroundMacOS13Bug = [](CFDictionaryRef dict,
                                                          CFStringRef key) {
    NSOperatingSystemVersion version =
        NSProcessInfo.processInfo.operatingSystemVersion;

    if (version.majorVersion == 13 && version.minorVersion == 0) {
      CFTypeRef value = CFDictionaryGetValue(dict, key);
      if (value == kCFBooleanFalse) {
        CGFloat zero = 0;
        return (CFNumberRef)CFAutorelease(
            CFNumberCreate(nullptr, kCFNumberCGFloatType, &zero));
      }
    }

    return base::apple::GetValueFromDictionary<CFNumberRef>(dict, key);
  };

  // |weight_tri| is either -1, 0, or 1 meaning "light", "normal", or "bold".
  auto CheckExpected = [GetValueFromDictionaryAndWorkAroundMacOS13Bug](
                           const Font& font, int weight_tri, bool isItalic) {
    base::apple::ScopedCFTypeRef<CFDictionaryRef> traits(
        CTFontCopyTraits(font.GetCTFont()));
    DCHECK(traits);

    CFNumberRef cf_slant = GetValueFromDictionaryAndWorkAroundMacOS13Bug(
        traits.get(), kCTFontSlantTrait);
    CGFloat slant;
    CFNumberGetValue(cf_slant, kCFNumberCGFloatType, &slant);
    if (isItalic)
      EXPECT_GT(slant, 0);
    else
      EXPECT_EQ(slant, 0);

    CFNumberRef cf_weight = GetValueFromDictionaryAndWorkAroundMacOS13Bug(
        traits.get(), kCTFontWeightTrait);
    CGFloat weight;
    CFNumberGetValue(cf_weight, kCFNumberCGFloatType, &weight);
    if (weight_tri < 0)
      EXPECT_LT(weight, 0);
    else if (weight_tri == 0)
      EXPECT_EQ(weight, 0);
    else
      EXPECT_GT(weight, 0);
  };

  // Use a base font that support all traits.
  Font base_font("Helvetica", 13);
  {
    SCOPED_TRACE("plain font");
    CheckExpected(base_font, 0, false);
  }

  // Italic
  Font italic_font(base_font.Derive(0, Font::ITALIC, Weight::NORMAL));
  {
    SCOPED_TRACE("italic font");
    CheckExpected(italic_font, 0, true);
  }

  // Bold
  Font bold_font(base_font.Derive(0, Font::NORMAL, Weight::BOLD));
  {
    SCOPED_TRACE("bold font");
    CheckExpected(bold_font, 1, false);
  }

  // Bold italic
  Font bold_italic_font(base_font.Derive(0, Font::ITALIC, Weight::BOLD));
  {
    SCOPED_TRACE("bold italic font");
    CheckExpected(bold_italic_font, 1, true);
  }

  // Non-existent thin will return the closest weight, light
  Font thin_font(base_font.Derive(0, Font::NORMAL, Weight::THIN));
  {
    SCOPED_TRACE("thin font");
    CheckExpected(thin_font, -1, false);
  }

  // Non-existent black will return the closest weight, bold
  Font black_font(base_font.Derive(0, Font::NORMAL, Weight::BLACK));
  {
    SCOPED_TRACE("black font");
    CheckExpected(black_font, 1, false);
  }
}

TEST(PlatformFontMacTest, DeriveFontUnderline) {
  // Create a default font.
  Font base_font;

  // Make the font underlined.
  Font derived_font(base_font.Derive(0, base_font.GetStyle() | Font::UNDERLINE,
                                     base_font.GetWeight()));

  // Validate the derived font properties against its native font instance.
  NSFontTraitMask traits = [NSFontManager.sharedFontManager
      traitsOfFont:base::apple::CFToNSPtrCast(derived_font.GetCTFont())];
  Weight actual_weight =
      (traits & NSFontBoldTrait) ? Weight::BOLD : Weight::NORMAL;

  int actual_style = Font::UNDERLINE;
  if (traits & NSFontItalicTrait)
    actual_style |= Font::ITALIC;

  EXPECT_TRUE(derived_font.GetStyle() & Font::UNDERLINE);
  EXPECT_EQ(derived_font.GetStyle(), actual_style);
  EXPECT_EQ(derived_font.GetWeight(), actual_weight);
}

// Tests internal methods for extracting Font properties from the
// underlying CTFont representation.
TEST(PlatformFontMacTest, ConstructFromNativeFont) {
  NSFont* ns_light_font = [NSFont fontWithName:@"Helvetica-Light" size:12];
  Font light_font(base::apple::NSToCFPtrCast(ns_light_font));
  EXPECT_EQ(12, light_font.GetFontSize());
  EXPECT_EQ("Helvetica", light_font.GetFontName());
  EXPECT_EQ(Font::NORMAL, light_font.GetStyle());
  EXPECT_EQ(Weight::LIGHT, light_font.GetWeight());

  NSFont* ns_light_italic_font = [NSFont fontWithName:@"Helvetica-LightOblique"
                                                 size:14];
  Font light_italic_font(base::apple::NSToCFPtrCast(ns_light_italic_font));
  EXPECT_EQ(14, light_italic_font.GetFontSize());
  EXPECT_EQ("Helvetica", light_italic_font.GetFontName());
  EXPECT_EQ(Font::ITALIC, light_italic_font.GetStyle());
  EXPECT_EQ(Weight::LIGHT, light_italic_font.GetWeight());

  NSFont* ns_normal_font = [NSFont fontWithName:@"Helvetica" size:12];
  Font normal_font(base::apple::NSToCFPtrCast(ns_normal_font));
  EXPECT_EQ(12, normal_font.GetFontSize());
  EXPECT_EQ("Helvetica", normal_font.GetFontName());
  EXPECT_EQ(Font::NORMAL, normal_font.GetStyle());
  EXPECT_EQ(Weight::NORMAL, normal_font.GetWeight());

  NSFont* ns_italic_font = [NSFont fontWithName:@"Helvetica-Oblique" size:14];
  Font italic_font(base::apple::NSToCFPtrCast(ns_italic_font));
  EXPECT_EQ(14, italic_font.GetFontSize());
  EXPECT_EQ("Helvetica", italic_font.GetFontName());
  EXPECT_EQ(Font::ITALIC, italic_font.GetStyle());
  EXPECT_EQ(Weight::NORMAL, italic_font.GetWeight());

  NSFont* ns_bold_font = [NSFont fontWithName:@"Helvetica-Bold" size:12];
  Font bold_font(base::apple::NSToCFPtrCast(ns_bold_font));
  EXPECT_EQ(12, bold_font.GetFontSize());
  EXPECT_EQ("Helvetica", bold_font.GetFontName());
  EXPECT_EQ(Font::NORMAL, bold_font.GetStyle());
  EXPECT_EQ(Weight::BOLD, bold_font.GetWeight());

  NSFont* ns_bold_italic_font = [NSFont fontWithName:@"Helvetica-BoldOblique"
                                                size:14];
  Font bold_italic_font(base::apple::NSToCFPtrCast(ns_bold_italic_font));
  EXPECT_EQ(14, bold_italic_font.GetFontSize());
  EXPECT_EQ("Helvetica", bold_italic_font.GetFontName());
  EXPECT_EQ(Font::ITALIC, bold_italic_font.GetStyle());
  EXPECT_EQ(Weight::BOLD, bold_italic_font.GetWeight());
}

// Test font derivation for fine-grained font weights.
TEST(PlatformFontMacTest, DerivedFineGrainedFonts) {
  // The resulting, actual font weight after deriving |weight| from |base|.
  auto DerivedIntWeight = [](Weight weight) {
    Font base;  // The default system font.
    Font derived(base.Derive(0, 0, weight));
    // PlatformFont should always pass the requested weight, not what the OS
    // could provide. This just checks a constructor argument, so not very
    // interesting.
    EXPECT_EQ(static_cast<int>(weight), static_cast<int>(derived.GetWeight()));

    return static_cast<int>(PlatformFontMac::GetFontWeightFromCTFontForTesting(
        derived.GetCTFont()));
  };

  EXPECT_EQ(static_cast<int>(Weight::THIN), DerivedIntWeight(Weight::THIN));
  EXPECT_EQ(static_cast<int>(Weight::EXTRA_LIGHT),
            DerivedIntWeight(Weight::EXTRA_LIGHT));
  EXPECT_EQ(static_cast<int>(Weight::LIGHT), DerivedIntWeight(Weight::LIGHT));
  EXPECT_EQ(static_cast<int>(Weight::NORMAL), DerivedIntWeight(Weight::NORMAL));
  EXPECT_EQ(static_cast<int>(Weight::MEDIUM), DerivedIntWeight(Weight::MEDIUM));
  EXPECT_EQ(static_cast<int>(Weight::SEMIBOLD),
            DerivedIntWeight(Weight::SEMIBOLD));
  EXPECT_EQ(static_cast<int>(Weight::BOLD), DerivedIntWeight(Weight::BOLD));
  EXPECT_EQ(static_cast<int>(Weight::EXTRA_BOLD),
            DerivedIntWeight(Weight::EXTRA_BOLD));
  EXPECT_EQ(static_cast<int>(Weight::BLACK), DerivedIntWeight(Weight::BLACK));
}

// Ensures that the Font's reported height is consistent with the native font's
// ascender and descender metrics.
TEST(PlatformFontMacTest, ValidateFontHeight) {
  // Use the default ResourceBundle system font (i.e. San Francisco).
  Font default_font;
  Font::FontStyle styles[] = {Font::NORMAL, Font::ITALIC, Font::UNDERLINE};

  for (auto& style : styles) {
    SCOPED_TRACE(testing::Message() << "Font::FontStyle: " << style);
    // Include the range of sizes used by ResourceBundle::FontStyle (-1 to +8).
    for (int delta = -1; delta <= 8; ++delta) {
      Font font = default_font.Derive(delta, style, Weight::NORMAL);
      SCOPED_TRACE(testing::Message() << "FontSize(): " << font.GetFontSize());
      NSFont* ns_font = base::apple::CFToNSPtrCast(font.GetCTFont());

      // Font height (an integer) should be the sum of these.
      CGFloat ascender = ns_font.ascender;
      CGFloat descender = ns_font.descender;
      CGFloat leading = ns_font.leading;

      // NSFont always gives a negative value for descender. Others positive.
      EXPECT_GE(0, descender);
      EXPECT_LE(0, ascender);
      EXPECT_LE(0, leading);

      int sum = ceil(ascender - descender + leading);

      // Text layout is performed using an integral baseline offset derived from
      // the ascender. The height needs to be enough to fit the full descender
      // (plus baseline). So the height depends on the rounding of the ascender,
      // and can be as much as 1 greater than the simple sum of floats.
      EXPECT_LE(sum, font.GetHeight());
      EXPECT_GE(sum + 1, font.GetHeight());

      // Recreate the rounding performed for GetBaseLine().
      EXPECT_EQ(ceil(ceil(ascender) - descender + leading), font.GetHeight());
    }
  }
}

// Test to ensure we cater for the AppKit quirk that can make the font italic
// when asking for a fine-grained weight. See http://crbug.com/742261. Note that
// AppKit's bug was detected on macOS 10.10 which uses Helvetica Neue as the
// system font.
TEST(PlatformFontMacTest, DerivedSemiboldFontIsNotItalic) {
  Font base_font;
  NSFontTraitMask base_traits = [NSFontManager.sharedFontManager
      traitsOfFont:base::apple::CFToNSPtrCast(base_font.GetCTFont())];
  ASSERT_FALSE(base_traits & NSItalicFontMask);

  Font semibold_font = base_font.Derive(0, Font::NORMAL, Weight::SEMIBOLD);
  NSFontTraitMask semibold_traits = [NSFontManager.sharedFontManager
      traitsOfFont:base::apple::CFToNSPtrCast(semibold_font.GetCTFont())];
  EXPECT_FALSE(semibold_traits & NSItalicFontMask);
}

}  // namespace gfx
