// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_mac.h"

#include <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/mac/mac_util.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"

// TODO(tapted): Remove gfx:: prefixes.
namespace gfx {

TEST(PlatformFontMacTest, DeriveFont) {
  // Use a base font that support all traits.
  gfx::Font base_font("Helvetica", 13);

  // Bold
  gfx::Font bold_font(
      base_font.Derive(0, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
  NSFontTraitMask traits = [[NSFontManager sharedFontManager]
      traitsOfFont:bold_font.GetNativeFont()];
  EXPECT_EQ(NSBoldFontMask, traits);

  // Italic
  gfx::Font italic_font(
      base_font.Derive(0, gfx::Font::ITALIC, gfx::Font::Weight::NORMAL));
  traits = [[NSFontManager sharedFontManager]
      traitsOfFont:italic_font.GetNativeFont()];
  EXPECT_EQ(NSItalicFontMask, traits);

  // Bold italic
  gfx::Font bold_italic_font(
      base_font.Derive(0, gfx::Font::ITALIC, gfx::Font::Weight::BOLD));
  traits = [[NSFontManager sharedFontManager]
      traitsOfFont:bold_italic_font.GetNativeFont()];
  EXPECT_EQ(static_cast<NSFontTraitMask>(NSBoldFontMask | NSItalicFontMask),
            traits);
}

TEST(PlatformFontMacTest, DeriveFontUnderline) {
  // Create a default font.
  gfx::Font base_font;

  // Make the font underlined.
  gfx::Font derived_font(base_font.Derive(
      0, base_font.GetStyle() | gfx::Font::UNDERLINE, base_font.GetWeight()));

  // Validate the derived font properties against its native font instance.
  NSFontTraitMask traits = [[NSFontManager sharedFontManager]
      traitsOfFont:derived_font.GetNativeFont()];
  gfx::Font::Weight actual_weight = (traits & NSFontBoldTrait)
                                        ? gfx::Font::Weight::BOLD
                                        : gfx::Font::Weight::NORMAL;

  int actual_style = gfx::Font::UNDERLINE;
  if (traits & NSFontItalicTrait)
    actual_style |= gfx::Font::ITALIC;

  EXPECT_TRUE(derived_font.GetStyle() & gfx::Font::UNDERLINE);
  EXPECT_EQ(derived_font.GetStyle(), actual_style);
  EXPECT_EQ(derived_font.GetWeight(), actual_weight);
}

// Tests internal methods for extracting gfx::Font properties from the
// underlying CTFont representation.
TEST(PlatformFontMacTest, ConstructFromNativeFont) {
  Font normal_font([NSFont fontWithName:@"Helvetica" size:12]);
  EXPECT_EQ(12, normal_font.GetFontSize());
  EXPECT_EQ("Helvetica", normal_font.GetFontName());
  EXPECT_EQ(Font::NORMAL, normal_font.GetStyle());
  EXPECT_EQ(Font::Weight::NORMAL, normal_font.GetWeight());

  Font bold_font([NSFont fontWithName:@"Helvetica-Bold" size:14]);
  EXPECT_EQ(14, bold_font.GetFontSize());
  EXPECT_EQ("Helvetica", bold_font.GetFontName());
  EXPECT_EQ(Font::NORMAL, bold_font.GetStyle());
  EXPECT_EQ(Font::Weight::BOLD, bold_font.GetWeight());

  Font italic_font([NSFont fontWithName:@"Helvetica-Oblique" size:14]);
  EXPECT_EQ(14, italic_font.GetFontSize());
  EXPECT_EQ("Helvetica", italic_font.GetFontName());
  EXPECT_EQ(Font::ITALIC, italic_font.GetStyle());
  EXPECT_EQ(Font::Weight::NORMAL, italic_font.GetWeight());

  Font bold_italic_font([NSFont fontWithName:@"Helvetica-BoldOblique" size:14]);
  EXPECT_EQ(14, bold_italic_font.GetFontSize());
  EXPECT_EQ("Helvetica", bold_italic_font.GetFontName());
  EXPECT_EQ(Font::ITALIC, bold_italic_font.GetStyle());
  EXPECT_EQ(Font::Weight::BOLD, bold_italic_font.GetWeight());
}

// Specific test for the mapping from the NSFont weight API to gfx::Font::Weight
// values.
TEST(PlatformFontMacTest, FontWeightAPIConsistency) {
  // Vanilla Helvetica only has bold and normal, so use a system font.
  NSFont* ns_font = [NSFont systemFontOfSize:13];
  NSFontManager* manager = [NSFontManager sharedFontManager];

  // -[NSFontManager convertWeight:ofFont] supposedly steps the font up and down
  // in weight values according to a table at
  // https://developer.apple.com/reference/appkit/nsfontmanager/1462321-convertweight
  // Apple Terminology                 | ISO Equivalent
  // 1. ultralight                     | none
  // 2. thin                           | W1. ultralight
  // 3. light, extralight              | W2. extralight
  // 4. book                           | W3. light
  // 5. regular, plain, display, roman | W4. semilight
  // 6. medium                         | W5. medium
  // 7. demi, demibold                 | none
  // 8. semi, semibold                 | W6. semibold
  // 9. bold                           | W7. bold
  // 10. extra, extrabold              | W8. extrabold
  // 11. heavy, heavyface              | none
  // 12. black, super                  | W9. ultrabold
  // 13. ultra, ultrablack, fat        | none
  // 14. extrablack, obese, nord       | none
  EXPECT_EQ(Font::Weight::NORMAL, Font(ns_font).GetWeight());  // Row 5.

  // Ensure the Bold "symbolic" trait from the NSFont traits API maps correctly
  // to the weight (non-symbolic) trait from the CTFont API.
  NSFont* bold_ns_font =
      [manager convertFont:ns_font toHaveTrait:NSFontBoldTrait];
  Font bold_font(bold_ns_font);
  EXPECT_EQ(Font::Weight::BOLD, bold_font.GetWeight());

  // No thin fonts on the lower rows of the table for San Francisco or earlier
  // system fonts.
  BOOL down = NO;
  ns_font = [NSFont systemFontOfSize:13];
  for (int row = 4; row > 0; --row) {
    SCOPED_TRACE(testing::Message() << "Row: " << row);
    ns_font = [manager convertWeight:down ofFont:ns_font];
    EXPECT_EQ(Font::Weight::NORMAL, Font(ns_font).GetWeight());
  }

  BOOL up = YES;
  // That is... unless we first go up by one and then down. A LIGHT and a THIN
  // font reveal themselves somehow. Only tested on 10.12.
  if (base::mac::IsAtLeastOS10_12()) {
    ns_font = [NSFont systemFontOfSize:13];
    ns_font = [manager convertWeight:up ofFont:ns_font];
    ns_font = [manager convertWeight:down ofFont:ns_font];
    EXPECT_EQ(Font::Weight::LIGHT, Font(ns_font).GetWeight());
    ns_font = [manager convertWeight:down ofFont:ns_font];
    EXPECT_EQ(Font::Weight::THIN, Font(ns_font).GetWeight());
  }

  ns_font = [NSFont systemFontOfSize:13];

  if (base::mac::IsOS10_11()) {
    // On 10.11 the API jumps to BOLD, but has heavier weights as well.
    ns_font = [manager convertWeight:up ofFont:ns_font];
    EXPECT_EQ(Font::Weight::BOLD, Font(ns_font).GetWeight());
    ns_font = [manager convertWeight:up ofFont:ns_font];
    EXPECT_EQ(Font::Weight::EXTRA_BOLD, Font(ns_font).GetWeight());
    ns_font = [manager convertWeight:up ofFont:ns_font];
    EXPECT_EQ(Font::Weight::BLACK, Font(ns_font).GetWeight());
    return;
  }

  // Each typeface maps weight notches differently, and the weight is actually a
  // floating point value that may not map directly to a gfx::Font::Weight. For
  // example San Francisco on macOS 10.12 goes up from 0 in the sequence: [0.23,
  // 0.23, 0.3, 0.4, 0.56, 0.62, 0.62, ...] and has no "thin" weights. But also
  // iterating over weights does weird stuff sometimes - before macOS 10.15,
  // occasionally the font goes italic, but going up another step goes back to
  // non-italic, at a heavier weight.

  // NSCTFontUIUsageAttribute = CTFontMediumUsage.
  ns_font = [manager convertWeight:up ofFont:ns_font];         // 0.23.
  EXPECT_EQ(Font::Weight::MEDIUM, Font(ns_font).GetWeight());  // Row 6.

  // 10.15 fixed the bug where the step up from medium created a medium italic.
  if (base::mac::IsAtMostOS10_14()) {
    // Goes italic: NSCTFontUIUsageAttribute = CTFontMediumItalicUsage.
    ns_font = [manager convertWeight:up ofFont:ns_font];         // 0.23.
    EXPECT_EQ(Font::Weight::MEDIUM, Font(ns_font).GetWeight());  // Row 7.
  }

  // NSCTFontUIUsageAttribute = CTFontDemiUsage.
  ns_font = [manager convertWeight:up ofFont:ns_font];  // 0.3.
  if (base::mac::IsOS10_10()) {
    // 10.10 is Helvetica Neue. It only has NORMAL, MEDIUM, BOLD and BLACK.
    EXPECT_EQ(Font::Weight::BOLD, Font(ns_font).GetWeight());  // Row 8.
  } else {
    EXPECT_EQ(Font::Weight::SEMIBOLD, Font(ns_font).GetWeight());  // Row 8.
  }

  // NSCTFontUIUsageAttribute = CTFontEmphasizedUsage.
  ns_font = [manager convertWeight:up ofFont:ns_font];  // 0.4 on 10.11+.

  if (base::mac::IsOS10_10()) {
    // Remaining rows are all BLACK on 10.10.
    for (int row = 9; row <= 14; ++row) {
      SCOPED_TRACE(testing::Message() << "Row: " << row);
      ns_font = [manager convertWeight:up ofFont:ns_font];
      EXPECT_EQ(Font::Weight::BLACK, Font(ns_font).GetWeight());
    }
    return;
  }
  EXPECT_EQ(Font::Weight::BOLD, Font(ns_font).GetWeight());  // Row 9.

  // NSCTFontUIUsageAttribute = CTFontHeavyUsage.
  ns_font = [manager convertWeight:up ofFont:ns_font];             // 0.56.
  EXPECT_EQ(Font::Weight::EXTRA_BOLD, Font(ns_font).GetWeight());  // Row 10.

  // NSCTFontUIUsageAttribute = CTFontBlackUsage.
  ns_font = [manager convertWeight:up ofFont:ns_font];        // 0.62.
  EXPECT_EQ(Font::Weight::BLACK, Font(ns_font).GetWeight());  // Row 11.
  ns_font = [manager convertWeight:up ofFont:ns_font];        // 0.62.
  EXPECT_EQ(Font::Weight::BLACK, Font(ns_font).GetWeight());  // Row 12.
  ns_font = [manager convertWeight:up ofFont:ns_font];        // 0.62.
  EXPECT_EQ(Font::Weight::BLACK, Font(ns_font).GetWeight());  // Row 13.
  ns_font = [manager convertWeight:up ofFont:ns_font];        // 0.62.
  EXPECT_EQ(Font::Weight::BLACK, Font(ns_font).GetWeight());  // Row 14.
}

// Test font derivation for fine-grained font weights.
TEST(PlatformFontMacTest, DerivedFineGrainedFonts) {
  // Only test where San Francisco is available.
  if (base::mac::IsAtMostOS10_10())
    return;

  using Weight = Font::Weight;
  Font base([NSFont systemFontOfSize:13]);

  // The resulting, actual font weight after deriving |weight| from |base|.
  auto DerivedWeight = [&](Weight weight) {
    Font derived(base.Derive(0, 0, weight));
    // PlatformFont should always pass the requested weight, not what the OS
    // could provide. This just checks a constructor argument, so not very
    // interesting.
    EXPECT_EQ(weight, derived.GetWeight());

    // Return the weight enum value that PlatformFontMac internally derives from
    // the floating point weight given by the kCTFontWeightTrait of |font|. Do
    // this by creating a new font based only off the NSFont in |derived|.
    return Font(derived.GetNativeFont()).GetWeight();
  };

  // Only use NORMAL or BOLD as a base font. Mac font APIs go whacky otherwise.
  // See comments in PlatformFontMac::DeriveFont().
  for (Weight base_weight : {Weight::NORMAL, Weight::BOLD}) {
    SCOPED_TRACE(testing::Message()
                 << "BaseWeight: " << static_cast<int>(base_weight));
    if (base_weight != Weight::NORMAL) {
      base = base.Derive(0, 0, base_weight);
      EXPECT_EQ(base_weight, base.GetWeight());
    }

    // Normal and heavy weights map correctly on 10.11 and 10.12.
    EXPECT_EQ(Weight::NORMAL, DerivedWeight(Weight::NORMAL));
    EXPECT_EQ(Weight::BOLD, DerivedWeight(Weight::BOLD));
    EXPECT_EQ(Weight::EXTRA_BOLD, DerivedWeight(Weight::EXTRA_BOLD));
    EXPECT_EQ(Weight::BLACK, DerivedWeight(Weight::BLACK));

    if (base::mac::IsAtMostOS10_11()) {
      // The fine-grained font weights on 10.11 are incomplete.
      EXPECT_EQ(Weight::NORMAL, DerivedWeight(Weight::EXTRA_LIGHT));
      EXPECT_EQ(Weight::NORMAL, DerivedWeight(Weight::THIN));
      EXPECT_EQ(Weight::NORMAL, DerivedWeight(Weight::LIGHT));
      EXPECT_EQ(Weight::BOLD, DerivedWeight(Weight::MEDIUM));
      EXPECT_EQ(Weight::BOLD, DerivedWeight(Weight::SEMIBOLD));
      continue;
    }

    // San Francisco doesn't offer anything between THIN and LIGHT.
    EXPECT_EQ(Weight::THIN, DerivedWeight(Weight::EXTRA_LIGHT));

    // All the rest should map correctly.
    EXPECT_EQ(Weight::THIN, DerivedWeight(Weight::THIN));
    EXPECT_EQ(Weight::LIGHT, DerivedWeight(Weight::LIGHT));
    EXPECT_EQ(Weight::MEDIUM, DerivedWeight(Weight::MEDIUM));
    EXPECT_EQ(Weight::SEMIBOLD, DerivedWeight(Weight::SEMIBOLD));
  }
}

// Ensures that the Font's reported height is consistent with the native font's
// ascender and descender metrics.
TEST(PlatformFontMacTest, ValidateFontHeight) {
  // Use the default ResourceBundle system font. E.g. Helvetica Neue in 10.10,
  // Lucida Grande before that, and San Francisco after.
  gfx::Font default_font;
  gfx::Font::FontStyle styles[] = {gfx::Font::NORMAL, gfx::Font::ITALIC,
                                   gfx::Font::UNDERLINE};

  for (size_t i = 0; i < base::size(styles); ++i) {
    SCOPED_TRACE(testing::Message() << "Font::FontStyle: " << styles[i]);
    // Include the range of sizes used by ResourceBundle::FontStyle (-1 to +8).
    for (int delta = -1; delta <= 8; ++delta) {
      gfx::Font font =
          default_font.Derive(delta, styles[i], gfx::Font::Weight::NORMAL);
      SCOPED_TRACE(testing::Message() << "FontSize(): " << font.GetFontSize());
      NSFont* native_font = font.GetNativeFont();

      // Font height (an integer) should be the sum of these.
      CGFloat ascender = [native_font ascender];
      CGFloat descender = [native_font descender];
      CGFloat leading = [native_font leading];

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
// Appkit's bug was detected on macOS 10.10 which uses Helvetica Neue as the
// system font.
TEST(PlatformFontMacTest, DerivedSemiboldFontIsNotItalic) {
  gfx::Font base_font;
  {
    NSFontTraitMask traits = [[NSFontManager sharedFontManager]
        traitsOfFont:base_font.GetNativeFont()];
    ASSERT_FALSE(traits & NSItalicFontMask);
  }

  gfx::Font semibold_font(
      base_font.Derive(0, gfx::Font::NORMAL, gfx::Font::Weight::SEMIBOLD));
  NSFontTraitMask traits = [[NSFontManager sharedFontManager]
      traitsOfFont:semibold_font.GetNativeFont()];
  EXPECT_FALSE(traits & NSItalicFontMask);
}

}  // namespace gfx
