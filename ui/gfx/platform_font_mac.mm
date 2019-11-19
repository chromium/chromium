// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_mac.h"

#include <cmath>

#include <Cocoa/Cocoa.h>

#import "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params.h"

namespace gfx {

namespace {

// How to get from NORMAL weight to a fine-grained font weight using calls to
// -[NSFontManager convertWeight:(BOOL)upFlag ofFont:(NSFont)].
struct WeightSolver {
  int steps_up;    // Times to call with upFlag:YES.
  int steps_down;  // Times to call with upFlag:NO.
  // Either NORMAL or BOLD: whether to set the NSBoldFontMask symbolic trait.
  Font::Weight nearest;
};

// Solve changes to the font weight according to the following table, from
// https://developer.apple.com/reference/appkit/nsfontmanager/1462321-convertweight
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
WeightSolver WeightChangeFromNormal(Font::Weight desired) {
  using Weight = Font::Weight;
  switch (desired) {
    case Weight::THIN:
      // It's weird, but to get LIGHT and THIN fonts, first go up a step.
      // Without this, the font stays stuck at NORMAL. See
      // PlatformFontMacTest, FontWeightAPIConsistency.
      return {1, 3, Weight::NORMAL};
    case Weight::EXTRA_LIGHT:
      return {1, 2, Weight::NORMAL};
    case Weight::LIGHT:
      return {1, 1, Weight::NORMAL};
    case Weight::NORMAL:
      return {0, 0, Weight::NORMAL};
    case Weight::MEDIUM:
      return {1, 0, Weight::NORMAL};
    case Weight::SEMIBOLD:
      return {0, 1, Weight::BOLD};
    case Weight::BOLD:
      return {0, 0, Weight::BOLD};
    case Weight::EXTRA_BOLD:
      return {1, 0, Weight::BOLD};
    case Weight::BLACK:
      return {3, 0, Weight::BOLD};  // Skip row 12.
    case Weight::INVALID:
      return {0, 0, Weight::NORMAL};
  }
}

// Returns the font style for |font|. Disregards Font::UNDERLINE, since NSFont
// does not support it as a trait.
int GetFontStyleFromNSFont(NSFont* font) {
  int font_style = Font::NORMAL;
  NSFontSymbolicTraits traits = [[font fontDescriptor] symbolicTraits];
  if (traits & NSFontItalicTrait)
    font_style |= Font::ITALIC;
  return font_style;
}

// Returns the Font weight for |font|.
Font::Weight GetFontWeightFromNSFont(NSFont* font) {
  DCHECK(font);

  // Map CoreText weights in a manner similar to ct_weight_to_fontstyle() from
  // SkFontHost_mac.cpp, but adjust for MEDIUM so that the San Francisco's
  // custom MEDIUM weight can be picked out. San Francisco has weights:
  // [0.23, 0.23, 0.3, 0.4, 0.56, 0.62, 0.62, ...] (no thin weights).
  // See PlatformFontMacTest.FontWeightAPIConsistency for details.
  // Note that the table Skia uses is also determined by experiment.
  constexpr struct {
    CGFloat ct_weight;
    Font::Weight gfx_weight;
  } weight_map[] = {
      // Missing: Apple "ultralight".
      {-0.70, Font::Weight::THIN},
      {-0.50, Font::Weight::EXTRA_LIGHT},
      {-0.23, Font::Weight::LIGHT},
      {0.00, Font::Weight::NORMAL},
      {0.23, Font::Weight::MEDIUM},  // Note: adjusted from 0.20 vs Skia.
      // Missing: Apple "demibold".
      {0.30, Font::Weight::SEMIBOLD},
      {0.40, Font::Weight::BOLD},
      {0.60, Font::Weight::EXTRA_BOLD},
      // Missing: Apple "heavyface".
      // Values will be capped to BLACK (this entry is here for consistency).
      {0.80, Font::Weight::BLACK},
      // Missing: Apple "ultrablack".
      // Missing: Apple "extrablack".
  };
  base::ScopedCFTypeRef<CFDictionaryRef> traits(
      CTFontCopyTraits(base::mac::NSToCFCast(font)));
  DCHECK(traits);
  CFNumberRef cf_weight = base::mac::GetValueFromDictionary<CFNumberRef>(
      traits, kCTFontWeightTrait);
  // A missing weight attribute just means 0 -> NORMAL.
  if (!cf_weight)
    return Font::Weight::NORMAL;

  // Documentation is vague about what sized floating point type should be used.
  // However, numeric_limits::epsilon() for 64-bit types is too small to match
  // the above table, so use 32-bit float. Do not check for the success of
  // CFNumberGetValue(). CFNumberGetValue() returns false for *any* loss of
  // value, and a float is used here deliberately to coarsen the accuracy.
  // There's no guarantee that any particular value for the kCTFontWeightTrait
  // will be able to be accurately represented with a float.
  float weight_value;
  CFNumberGetValue(cf_weight, kCFNumberFloatType, &weight_value);
  for (const auto& item : weight_map) {
    if (weight_value - item.ct_weight <= std::numeric_limits<float>::epsilon())
      return item.gfx_weight;
  }
  return Font::Weight::BLACK;
}

// Returns an autoreleased NSFont created with the passed-in specifications.
NSFont* NSFontWithSpec(const std::string& font_name,
                       int font_size,
                       int font_style,
                       Font::Weight font_weight) {
  NSFontSymbolicTraits trait_bits = 0;
  // TODO(mboc): Add support for other weights as well.
  if (font_weight >= Font::Weight::BOLD)
    trait_bits |= NSFontBoldTrait;
  if (font_style & Font::ITALIC)
    trait_bits |= NSFontItalicTrait;
  // The Mac doesn't support underline as a font trait, so just drop it.
  // (Underlines must be added as an attribute on an NSAttributedString.)
  NSDictionary* traits = @{ NSFontSymbolicTrait : @(trait_bits) };

  NSDictionary* attrs = @{
    NSFontFamilyAttribute : base::SysUTF8ToNSString(font_name),
    NSFontTraitsAttribute : traits
  };
  NSFontDescriptor* descriptor =
      [NSFontDescriptor fontDescriptorWithFontAttributes:attrs];
  NSFont* font = [NSFont fontWithDescriptor:descriptor size:font_size];
  if (font)
    return font;

  // Make one fallback attempt by looking up via font name rather than font
  // family name.
  attrs = @{
    NSFontNameAttribute : base::SysUTF8ToNSString(font_name),
    NSFontTraitsAttribute : traits
  };
  descriptor = [NSFontDescriptor fontDescriptorWithFontAttributes:attrs];
  return [NSFont fontWithDescriptor:descriptor size:font_size];
}

// Returns |font| or a default font if |font| is nil.
NSFont* ValidateFont(NSFont* font) {
  return font ? font : [NSFont systemFontOfSize:[NSFont systemFontSize]];
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, public:

PlatformFontMac::PlatformFontMac()
    : PlatformFontMac([NSFont systemFontOfSize:[NSFont systemFontSize]]) {
}

PlatformFontMac::PlatformFontMac(NativeFont native_font)
    : PlatformFontMac(native_font,
                      base::SysNSStringToUTF8([native_font familyName]),
                      [native_font pointSize],
                      GetFontStyleFromNSFont(native_font),
                      GetFontWeightFromNSFont(native_font)) {
  DCHECK(native_font);  // Null should not be passed to this constructor.
}

PlatformFontMac::PlatformFontMac(const std::string& font_name, int font_size)
    : PlatformFontMac(font_name,
                      font_size,
                      Font::NORMAL,
                      Font::Weight::NORMAL) {}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, PlatformFont implementation:

Font PlatformFontMac::DeriveFont(int size_delta,
                                 int style,
                                 Font::Weight weight) const {
  // For some reason, creating fonts using the NSFontDescriptor API's seem to be
  // unreliable. Hence use the NSFontManager.
  NSFont* derived = native_font_;
  NSFontManager* font_manager = [NSFontManager sharedFontManager];

  if (weight != font_weight_) {
    // Find a font without any bold traits. Ideally, all bold traits are
    // removed here, but non-symbolic traits are read-only properties of a
    // particular set of glyphs. And attempting to "reset" the attribute with a
    // new font descriptor will lose internal properties that Mac decorates its
    // UI fonts with. E.g., solving the plans below from NORMAL result in a
    // CTFontDescriptor attribute entry of NSCTFontUIUsageAttribute in
    // CTFont{Regular,Medium,Demi,Emphasized,Heavy,Black}Usage. Attempting to
    // "solve" weights starting at other than NORMAL has unpredictable results.
    if (font_weight_ != Font::Weight::NORMAL)
      derived = [font_manager convertFont:derived toHaveTrait:NSUnboldFontMask];
    DLOG_IF(WARNING, GetFontWeightFromNSFont(derived) != Font::Weight::NORMAL)
        << "Deriving from a font with an internal unmodifiable weight.";

    WeightSolver plan = WeightChangeFromNormal(weight);
    if (plan.nearest == Font::Weight::BOLD)
      derived = [font_manager convertFont:derived toHaveTrait:NSBoldFontMask];
    for (int i = 0; i < plan.steps_up; ++i)
      derived = [font_manager convertWeight:YES ofFont:derived];
    for (int i = 0; i < plan.steps_down; ++i)
      derived = [font_manager convertWeight:NO ofFont:derived];
  }

  // Always apply the italic trait, even if the italic trait is not changing.
  // it's possible for a change in the weight to trigger the font to go italic.
  // This is due to an AppKit bug. See http://crbug.com/742261.
  if (style != font_style_ || weight != font_weight_) {
    NSFontTraitMask italic_trait_mask =
        (style & Font::ITALIC) ? NSItalicFontMask : NSUnitalicFontMask;
    derived = [font_manager convertFont:derived toHaveTrait:italic_trait_mask];
  }

  if (size_delta != 0)
    derived = [font_manager convertFont:derived toSize:font_size_ + size_delta];

  return Font(new PlatformFontMac(derived, font_name_, font_size_ + size_delta,
                                  style, weight));
}

int PlatformFontMac::GetHeight() {
  return height_;
}

int PlatformFontMac::GetBaseline() {
  return ascent_;
}

int PlatformFontMac::GetCapHeight() {
  return cap_height_;
}

int PlatformFontMac::GetExpectedTextWidth(int length) {
  if (!average_width_) {
    // -[NSFont boundingRectForGlyph:] seems to always return the largest
    // bounding rect that could be needed, which produces very wide expected
    // widths for strings. Instead, compute the actual width of a string
    // containing all the lowercase characters to find a reasonable guess at the
    // average.
    base::scoped_nsobject<NSAttributedString> attr_string(
        [[NSAttributedString alloc]
            initWithString:@"abcdefghijklmnopqrstuvwxyz"
                attributes:@{NSFontAttributeName : native_font_.get()}]);
    average_width_ = [attr_string size].width / [attr_string length];
    DCHECK_NE(0, average_width_);
  }
  return ceil(length * average_width_);
}

int PlatformFontMac::GetStyle() const {
  return font_style_;
}

Font::Weight PlatformFontMac::GetWeight() const {
  return font_weight_;
}

const std::string& PlatformFontMac::GetFontName() const {
  return font_name_;
}

std::string PlatformFontMac::GetActualFontName() const {
  return base::SysNSStringToUTF8([native_font_ familyName]);
}

int PlatformFontMac::GetFontSize() const {
  return font_size_;
}

const FontRenderParams& PlatformFontMac::GetFontRenderParams() {
  return render_params_;
}

NativeFont PlatformFontMac::GetNativeFont() const {
  return [[native_font_.get() retain] autorelease];
}

sk_sp<SkTypeface> PlatformFontMac::GetNativeSkTypefaceIfAvailable() const {
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, private:

PlatformFontMac::PlatformFontMac(const std::string& font_name,
                                 int font_size,
                                 int font_style,
                                 Font::Weight font_weight)
    : PlatformFontMac(
          NSFontWithSpec(font_name, font_size, font_style, font_weight),
          font_name,
          font_size,
          font_style,
          font_weight) {}

PlatformFontMac::PlatformFontMac(NativeFont font,
                                 const std::string& font_name,
                                 int font_size,
                                 int font_style,
                                 Font::Weight font_weight)
    : native_font_([ValidateFont(font) retain]),
      font_name_(font_name),
      font_size_(font_size),
      font_style_(font_style),
      font_weight_(font_weight) {
  CalculateMetricsAndInitRenderParams();
}

PlatformFontMac::~PlatformFontMac() {
}

void PlatformFontMac::CalculateMetricsAndInitRenderParams() {
  NSFont* font = native_font_.get();
  DCHECK(font);
  ascent_ = ceil([font ascender]);
  cap_height_ = ceil([font capHeight]);

  // PlatformFontMac once used -[NSLayoutManager defaultLineHeightForFont:] to
  // initialize |height_|. However, it has a silly rounding bug. Essentially, it
  // gives round(ascent) + round(descent). E.g. Helvetica Neue at size 16 gives
  // ascent=15.4634, descent=3.38208 -> 15 + 3 = 18. When the height should be
  // at least 19. According to the OpenType specification, these values should
  // simply be added, so do that. Note this uses the already-rounded |ascent_|
  // to ensure GetBaseline() + descender fits within GetHeight() during layout.
  height_ = ceil(ascent_ + std::abs([font descender]) + [font leading]);

  FontRenderParamsQuery query;
  query.families.push_back(font_name_);
  query.pixel_size = font_size_;
  query.style = font_style_;
  query.weight = font_weight_;
  render_params_ = gfx::GetFontRenderParams(query, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontMac;
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontMac(ValidateFont(native_font));
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  return new PlatformFontMac(font_name, font_size);
}

}  // namespace gfx
