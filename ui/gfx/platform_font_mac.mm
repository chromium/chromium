// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_mac.h"

#include <cmath>
#include <set>

#include <Cocoa/Cocoa.h>
#include <CoreText/CoreText.h>

#include "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/memory/scoped_policy.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/ports/SkTypeface_mac.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params.h"

namespace gfx {

using Weight = Font::Weight;

extern "C" {
bool CTFontDescriptorIsSystemUIFont(CTFontDescriptorRef);
}

namespace {

// Returns the font style for |font|. Disregards Font::UNDERLINE, since NSFont
// does not support it as a trait.
int GetFontStyleFromCTFont(CTFontRef font) {
  int font_style = Font::NORMAL;
  NSFont* ns_font = base::apple::CFToNSPtrCast(font);
  NSFontSymbolicTraits traits = ns_font.fontDescriptor.symbolicTraits;
  if (traits & NSFontItalicTrait) {
    font_style |= Font::ITALIC;
  }
  return font_style;
}

// Returns the Font::Weight for |font|.
Weight GetFontWeightFromCTFont(CTFontRef font) {
  DCHECK(font);

  // Map CoreText weights in a manner similar to ct_weight_to_fontstyle() from
  // SkFontHost_mac.cpp, but adjusted for the weights actually used by the
  // system fonts. See PlatformFontMacTest.FontWeightAPIConsistency for details.
  // macOS uses specific float values in its constants, but individual fonts can
  // and do specify arbitrary values in the -1.0 to 1.0 range. Therefore, to
  // accommodate that, and to avoid float comparison issues, use ranges.
  constexpr struct {
    // A range of CoreText weights.
    CGFloat weight_lower;
    CGFloat weight_upper;
    Weight gfx_weight;
  } weight_map[] = {
      // NSFontWeight constants:
      //   NSFontWeightUltraLight: -0.80
      //   NSFontWeightThin: -0.60
      //   NSFontWeightLight: -0.40
      //   NSFontWeightRegular: 0.0
      //   NSFontWeightMedium: 0.23
      //   NSFontWeightSemibold: 0.30
      //   NSFontWeightBold: 0.40
      //   NSFontWeightHeavy: 0.56
      //   NSFontWeightBlack: 0.62
      //
      // Actual system font weights:
      //   .AppleSystemUIFontUltraLight: -0.80
      //   .AppleSystemUIFontThin: -0.60
      //   .AppleSystemUIFontLight: -0.40
      //   .AppleSystemUIFont: 0.0
      //   .AppleSystemUIFontMedium: 0.23
      //   .AppleSystemUIFontDemi: 0.30
      //   .AppleSystemUIFontEmphasized: 0.40
      //   .AppleSystemUIFontHeavy: 0.56
      //   .AppleSystemUIFontBlack: 0.62
      {-1.0, -0.70, Weight::THIN},          // NSFontWeightUltraLight
      {-0.70, -0.45, Weight::EXTRA_LIGHT},  // NSFontWeightThin
      {-0.45, -0.10, Weight::LIGHT},        // NSFontWeightLight
      {-0.10, 0.10, Weight::NORMAL},        // NSFontWeightRegular
      {0.10, 0.27, Weight::MEDIUM},         // NSFontWeightMedium
      {0.27, 0.35, Weight::SEMIBOLD},       // NSFontWeightSemibold
      {0.35, 0.50, Weight::BOLD},           // NSFontWeightBold
      {0.50, 0.60, Weight::EXTRA_BOLD},     // NSFontWeightHeavy
      {0.60, 1.0, Weight::BLACK},           // NSFontWeightBlack
  };

  base::apple::ScopedCFTypeRef<CFDictionaryRef> traits(CTFontCopyTraits(font));
  DCHECK(traits);
  CFNumberRef cf_weight = base::apple::GetValueFromDictionary<CFNumberRef>(
      traits.get(), kCTFontWeightTrait);
  // A missing weight attribute just means 0 -> NORMAL.
  if (!cf_weight)
    return Weight::NORMAL;

  // macOS 13.0 bug: For non-system fonts with 0-valued traits,
  // `kCFBooleanFalse` is used instead of a `CFNumberRef` of 0. See
  // https://crbug.com/1372420. Filed as FB11673021, fixed in macOS 13.1. In
  // this code path, the `base::apple::GetValueFromDictionary` call above will
  // DLOG for this case and return a null `CFNumberRef`, which will cause this
  // function to return `Weight::NORMAL`, which happens to be the correct thing
  // to do for a trait with value 0.

  // The value of kCTFontWeightTrait empirically is a kCFNumberFloat64Type
  // (double) on all tested versions of macOS. However, that doesn't really
  // matter as only the first two decimal digits need to be tested. Do not check
  // for the success of CFNumberGetValue() as it returns false for any loss of
  // value and all that is needed here is two digits of accuracy.
  CGFloat weight;
  CFNumberGetValue(cf_weight, kCFNumberCGFloatType, &weight);
  for (const auto& item : weight_map) {
    if (item.weight_lower <= weight && weight <= item.weight_upper)
      return item.gfx_weight;
  }
  return Weight::INVALID;
}

// Converts a Font::Weight value to the corresponding NSFontWeight value.
NSFontWeight ToNSFontWeight(Weight weight) {
  switch (weight) {
    case Weight::THIN:
      return NSFontWeightUltraLight;
    case Weight::EXTRA_LIGHT:
      return NSFontWeightThin;
    case Weight::LIGHT:
      return NSFontWeightLight;
    case Weight::INVALID:
    case Weight::NORMAL:
      return NSFontWeightRegular;
    case Weight::MEDIUM:
      return NSFontWeightMedium;
    case Weight::SEMIBOLD:
      return NSFontWeightSemibold;
    case Weight::BOLD:
      return NSFontWeightBold;
    case Weight::EXTRA_BOLD:
      return NSFontWeightHeavy;
    case Weight::BLACK:
      return NSFontWeightBlack;
  }
}

// Chromium uses the ISO-style, 9-value ladder of font weights (THIN-BLACK). The
// new font API in macOS also uses these weights, though they are constants
// defined in terms of CGFloat with values from -1.0 to 1.0.
//
// However, the old API used by the NSFontManager uses integer values on a
// "scale of 0 to 15". These values are used in:
//
//   -[NSFontManager availableMembersOfFontFamily:]
//   -[NSFontManager convertWeight:ofFont:]
//   -[NSFontManager fontWithFamily:traits:weight:size:]
//   -[NSFontManager weightOfFont:]
//
// Apple provides a chart of how the ISO values correspond:
// https://developer.apple.com/reference/appkit/nsfontmanager/1462321-convertweight
// However, it's more complicated than that. A survey of fonts yields the
// correspondence in this function, but the outliers imply that the ISO-style
// weight is more along the lines of "weight role within the font family" vs
// this number which is more like "how weighty is this font compared to all
// other fonts".
//
// These numbers can't really be forced to line up; different fonts disagree on
// how to map them. This function mostly follows the documented chart as
// inspired by actual fonts, and should be good enough.
NSInteger ToNSFontManagerWeight(Weight weight) {
  switch (weight) {
    case Weight::THIN:
      return 2;
    case Weight::EXTRA_LIGHT:
      return 3;
    case Weight::LIGHT:
      return 4;
    case Weight::INVALID:
    case Weight::NORMAL:
      return 5;
    case Weight::MEDIUM:
      return 6;
    case Weight::SEMIBOLD:
      return 8;
    case Weight::BOLD:
      return 9;
    case Weight::EXTRA_BOLD:
      return 10;
    case Weight::BLACK:
      return 11;
  }
}

std::string GetFamilyNameFromTypeface(sk_sp<SkTypeface> typeface) {
  SkString family;
  typeface->getFamilyName(&family);
  return family.c_str();
}

// Returns an autoreleased font of a specified system font type.
CTFontRef SystemFontForConstructorOfType(
    PlatformFontMac::SystemFontType type,
    int font_size = PlatformFontMac::kDefaultFontSize) {
  NSFont* font = nil;
  switch (type) {
    case PlatformFontMac::SystemFontType::kGeneral: {
      if (font_size == PlatformFontMac::kDefaultFontSize) {
        font_size = NSFont.systemFontSize;
      }
      font = [NSFont systemFontOfSize:font_size];
      break;
    }
    case PlatformFontMac::SystemFontType::kMenu: {
      font = [NSFont menuFontOfSize:font_size];
      break;
    }
    case PlatformFontMac::SystemFontType::kToolTip: {
      font = [NSFont toolTipsFontOfSize:font_size];
      break;
    }
    default: {
      NOTREACHED();
    }
  }

  return base::apple::NSToCFPtrCast(font);
}

std::optional<PlatformFontMac::SystemFontType>
SystemFontTypeFromUndocumentedCTFontRefInternals(CTFontRef font) {
  // The macOS APIs can't reliably derive one font from another. That's why for
  // non-system fonts PlatformFontMac::DeriveFont() uses the family name of the
  // font to find look up new fonts from scratch, and why, for system fonts, it
  // uses the system font APIs to generate new system fonts.
  //
  // Skia's font handling assumes that given a font object, new fonts can be
  // derived from it. That's absolutely not true on the Mac. However, this needs
  // to be fixed, and a rewrite of how Skia handles fonts is not on the table.
  //
  // Therefore this sad hack. If Skia provides an SkTypeface, dig into the
  // undocumented bowels of CoreText and magically determine if the font is a
  // system font. This allows PlatformFontMac to correctly derive variants of
  // the provided font.
  //
  // TODO(avi, etienneb): Figure out this font stuff.
  base::apple::ScopedCFTypeRef<CTFontDescriptorRef> descriptor(
      CTFontCopyFontDescriptor(font));
  if (CTFontDescriptorIsSystemUIFont(descriptor.get())) {
    // Assume it's the standard system font. The fact that this much is known is
    // enough.
    return PlatformFontMac::SystemFontType::kGeneral;
  } else {
    return std::nullopt;
  }
}

const std::set<std::string>& SystemFontNames() {
  static const base::NoDestructor<std::set<std::string>> names([] {
    std::set<std::string> names;
    names.insert(base::SysNSStringToUTF8(
        [NSFont systemFontOfSize:NSFont.systemFontSize].familyName));
    names.insert(base::SysNSStringToUTF8([NSFont menuFontOfSize:0].familyName));
    names.insert(
        base::SysNSStringToUTF8([NSFont toolTipsFontOfSize:0].familyName));
    return names;
  }());

  return *names;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, public:

PlatformFontMac::PlatformFontMac(SystemFontType system_font_type, int font_size)
    : PlatformFontMac(
          SystemFontForConstructorOfType(system_font_type, font_size),
          system_font_type) {}

PlatformFontMac::PlatformFontMac(CTFontRef ct_font)
    : PlatformFontMac(ct_font, std::nullopt) {
  DCHECK(ct_font);  // nil must not be passed to this constructor.
}

PlatformFontMac::PlatformFontMac(const std::string& font_name, int font_size)
    : PlatformFontMac(
          CTFontWithSpec({font_name, font_size, Font::NORMAL, Weight::NORMAL})
              .get(),
          std::nullopt,
          {font_name, font_size, Font::NORMAL, Weight::NORMAL}) {}

PlatformFontMac::PlatformFontMac(sk_sp<SkTypeface> typeface,
                                 int font_size_pixels,
                                 const std::optional<FontRenderParams>& params)
    : PlatformFontMac(SkTypeface_GetCTFontRef(typeface.get()),
                      SystemFontTypeFromUndocumentedCTFontRefInternals(
                          SkTypeface_GetCTFontRef(typeface.get())),
                      {GetFamilyNameFromTypeface(typeface), font_size_pixels,
                       (typeface->isItalic() ? Font::ITALIC : Font::NORMAL),
                       FontWeightFromInt(typeface->fontStyle().weight())}) {}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, PlatformFont implementation:

Font PlatformFontMac::DeriveFont(int size_delta,
                                 int style,
                                 Weight weight) const {
  // What doesn't work?
  //
  // For all fonts, -[NSFontManager convertWeight:ofFont:] will reliably
  // misbehave, skipping over particular weights of fonts, refusing to go
  // lighter than regular unless you go heavier first, and in earlier versions
  // of the system would accidentally introduce italic fonts when changing
  // weights from a non-italic instance.
  //
  // For system fonts, -[NSFontManager convertFont:to(Not)HaveTrait:], if used
  // to change weight, will sometimes switch to a compatibility system font that
  // does not have all the weights available.
  //
  // For system fonts, the most reliable call to use is +[NSFont
  // systemFontOfSize:weight:]. This uses the new-style NSFontWeight which maps
  // perfectly to the ISO weights that Chromium uses. For non-system fonts,
  // -[NSFontManager fontWithFamily:traits:weight:size:] is the only reasonable
  // way to query fonts with more granularity than bold/non-bold short of
  // walking the font family and querying their kCTFontWeightTrait values. Font
  // descriptors hold promise but querying using them often fails to find fonts
  // that match; hopefully their matching abilities will improve in future
  // versions of the macOS.

  if (system_font_type_ == SystemFontType::kGeneral) {
    NSFont* derived = [NSFont systemFontOfSize:font_spec_.size + size_delta
                                        weight:ToNSFontWeight(weight)];
    NSFontTraitMask italic_trait_mask =
        (style & Font::ITALIC) ? NSItalicFontMask : NSUnitalicFontMask;
    derived = [NSFontManager.sharedFontManager convertFont:derived
                                               toHaveTrait:italic_trait_mask];

    return Font(new PlatformFontMac(
        base::apple::NSToCFPtrCast(derived), SystemFontType::kGeneral,
        {font_spec_.name, font_spec_.size + size_delta, style, weight}));
  } else if (system_font_type_ == SystemFontType::kMenu) {
    NSFont* derived = [NSFont menuFontOfSize:font_spec_.size + size_delta];
    return Font(new PlatformFontMac(
        base::apple::NSToCFPtrCast(derived), SystemFontType::kMenu,
        {font_spec_.name, font_spec_.size + size_delta, style, weight}));
  } else if (system_font_type_ == SystemFontType::kToolTip) {
    NSFont* derived = [NSFont toolTipsFontOfSize:font_spec_.size + size_delta];
    return Font(new PlatformFontMac(
        base::apple::NSToCFPtrCast(derived), SystemFontType::kToolTip,
        {font_spec_.name, font_spec_.size + size_delta, style, weight}));
  } else {
    base::apple::ScopedCFTypeRef<CTFontRef> derived = CTFontWithSpec(
        {font_spec_.name, font_spec_.size + size_delta, style, weight});
    return Font(new PlatformFontMac(
        derived.get(), std::nullopt,
        {font_spec_.name, font_spec_.size + size_delta, style, weight}));
  }
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
    NSAttributedString* attr_string = [[NSAttributedString alloc]
        initWithString:@"abcdefghijklmnopqrstuvwxyz"
            attributes:@{
              NSFontAttributeName : base::apple::CFToNSPtrCast(ct_font_.get())
            }];
    average_width_ = attr_string.size.width / attr_string.length;
    DCHECK_NE(0, average_width_);
  }
  return ceil(length * average_width_);
}

int PlatformFontMac::GetStyle() const {
  return font_spec_.style;
}

Weight PlatformFontMac::GetWeight() const {
  return font_spec_.weight;
}

const std::string& PlatformFontMac::GetFontName() const {
  return font_spec_.name;
}

std::string PlatformFontMac::GetActualFontName() const {
  return base::SysNSStringToUTF8(
      base::apple::CFToNSPtrCast(ct_font_.get()).familyName);
}

int PlatformFontMac::GetFontSize() const {
  return font_spec_.size;
}

const FontRenderParams& PlatformFontMac::GetFontRenderParams() {
  return render_params_;
}

CTFontRef PlatformFontMac::GetCTFont() const {
  return ct_font_.get();
}

sk_sp<SkTypeface> PlatformFontMac::GetNativeSkTypeface() const {
  return SkMakeTypefaceFromCTFont(GetCTFont());
}

// static
Weight PlatformFontMac::GetFontWeightFromCTFontForTesting(CTFontRef font) {
  return GetFontWeightFromCTFont(font);
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontMac, private:

PlatformFontMac::PlatformFontMac(CTFontRef ct_font,
                                 std::optional<SystemFontType> system_font_type)
    : PlatformFontMac(ct_font,
                      system_font_type,
                      {base::SysNSStringToUTF8(
                           base::apple::CFToNSPtrCast(ct_font).familyName),
                       base::ClampRound(CTFontGetSize(ct_font)),
                       GetFontStyleFromCTFont(ct_font),
                       GetFontWeightFromCTFont(ct_font)}) {}

PlatformFontMac::PlatformFontMac(CTFontRef ct_font,
                                 std::optional<SystemFontType> system_font_type,
                                 FontSpec spec)
    : ct_font_(ct_font, base::scoped_policy::RETAIN),
      system_font_type_(system_font_type),
      font_spec_(spec) {
#if DCHECK_IS_ON()
  // If system_font_type.has_value(), `ct_font` is a system font that was
  // passed in from the correct constructor. If system_font_type.has_value() is
  // false, we expect that `ct_font` is *not* a system font. We enforce this
  // expectation by checking for spec.name in the list of system font names.
  // So, if system_font_type.has_value(), or `spec.name` does not appear in the
  // list of system font names, we're in good shape.
  DCHECK(system_font_type.has_value() ||
         SystemFontNames().count(spec.name) == 0)
      << "Do not pass a system font (" << spec.name << ") to PlatformFontMac; "
      << "use the SystemFontType constructor. Extend the SystemFontType enum "
      << "if necessary.";
#endif  // DCHECK_IS_ON()
  CalculateMetricsAndInitRenderParams();
}

PlatformFontMac::~PlatformFontMac() = default;

void PlatformFontMac::CalculateMetricsAndInitRenderParams() {
  NSFont* font = base::apple::CFToNSPtrCast(ct_font_.get());
  DCHECK(font);
  ascent_ = ceil(font.ascender);
  cap_height_ = ceil(font.capHeight);

  // PlatformFontMac once used -[NSLayoutManager defaultLineHeightForFont:] to
  // initialize |height_|. However, it has a silly rounding bug. Essentially, it
  // gives round(ascent) + round(descent). E.g. Helvetica Neue at size 16 gives
  // ascent=15.4634, descent=3.38208 -> 15 + 3 = 18. When the height should be
  // at least 19. According to the OpenType specification, these values should
  // simply be added, so do that. Note this uses the already-rounded |ascent_|
  // to ensure GetBaseline() + descender fits within GetHeight() during layout.
  height_ = ceil(ascent_ + std::abs(font.descender) + font.leading);

  FontRenderParamsQuery query;
  query.families.push_back(font_spec_.name);
  query.pixel_size = font_spec_.size;
  query.style = font_spec_.style;
  query.weight = font_spec_.weight;
  render_params_ = gfx::GetFontRenderParams(query, nullptr);
}

// static
base::apple::ScopedCFTypeRef<CTFontRef> PlatformFontMac::CTFontWithSpec(
    FontSpec font_spec) {
  // One might think that a font descriptor with the NSFontWeightTrait/
  // kCTFontWeightTrait trait could be used to look up a font with a specific
  // weight. That doesn't work, though. You can ask a font for its weight, but
  // you can't use weight to query for the font.
  //
  // The way that does work is to use the old-style integer weight API.

  NSFontManager* font_manager = NSFontManager.sharedFontManager;

  NSFontTraitMask traits = 0;
  if (font_spec.style & Font::ITALIC)
    traits |= NSItalicFontMask;
  // The Mac doesn't support underline as a font trait, so just drop it.
  // (Underlines must be added as an attribute on an NSAttributedString.) Do not
  // add NSBoldFontMask here; if it is added then the weight parameter below
  // will be ignored.

  NSFont* font =
      [font_manager fontWithFamily:base::SysUTF8ToNSString(font_spec.name)
                            traits:traits
                            weight:ToNSFontManagerWeight(font_spec.weight)
                              size:font_spec.size];
  if (font) {
    return base::apple::ScopedCFTypeRef<CTFontRef>(
        base::apple::NSToCFOwnershipCast(font));
  }

  // Make one fallback attempt by looking up via font name rather than font
  // family name. With this API, the available granularity of font weight is
  // bold/not-bold, but that's what's available.
  NSFontSymbolicTraits trait_bits = 0;
  if (font_spec.weight >= Weight::BOLD)
    trait_bits |= NSFontBoldTrait;
  if (font_spec.style & Font::ITALIC)
    trait_bits |= NSFontItalicTrait;

  NSDictionary* attrs = @{
    NSFontNameAttribute : base::SysUTF8ToNSString(font_spec.name),
    NSFontTraitsAttribute : @{NSFontSymbolicTrait : @(trait_bits)},
  };
  NSFontDescriptor* descriptor =
      [NSFontDescriptor fontDescriptorWithFontAttributes:attrs];

  font = [NSFont fontWithDescriptor:descriptor size:font_spec.size];
  if (font) {
    return base::apple::ScopedCFTypeRef<CTFontRef>(
        base::apple::NSToCFOwnershipCast(font));
  }

  // If that doesn't find a font, whip up a system font to stand in for the
  // specified font.
  font = [NSFont systemFontOfSize:font_spec.size
                           weight:ToNSFontWeight(font_spec.weight)];
  font = [font_manager convertFont:font toHaveTrait:traits];
  return base::apple::ScopedCFTypeRef<CTFontRef>(
      base::apple::NSToCFOwnershipCast(font));
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontMac(PlatformFontMac::SystemFontType::kGeneral);
}

// static
PlatformFont* PlatformFont::CreateFromCTFont(CTFontRef ct_font) {
  return new PlatformFontMac(ct_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::string& font_name,
                                                  int font_size) {
  if (!SystemFontNames().count(font_name)) {
    return new PlatformFontMac(font_name, font_size);
  }

  // `font_name` is the name of a system font. Use the APIs Apple dictates to
  // construct a system font instance. At least as of macOS 15, the family
  // names for menuFontOfSize: and toolTipsFontOfSize: are the same as the
  // system font. If font_name matches the system font name, make sure we
  // construct it using SystemFontType::kGeneral.
  if (font_name ==
      base::SysNSStringToUTF8(
          [NSFont systemFontOfSize:NSFont.systemFontSize].familyName)) {
    return new PlatformFontMac(PlatformFontMac::SystemFontType::kGeneral,
                               font_size);
  }

  if (font_name ==
      base::SysNSStringToUTF8([NSFont menuFontOfSize:0].familyName)) {
    return new PlatformFontMac(PlatformFontMac::SystemFontType::kMenu,
                               font_size);
  }

  return new PlatformFontMac(PlatformFontMac::SystemFontType::kToolTip,
                             font_size);
}

// static
PlatformFont* PlatformFont::CreateFromSkTypeface(
    sk_sp<SkTypeface> typeface,
    int font_size_pixels,
    const std::optional<FontRenderParams>& params) {
  return new PlatformFontMac(typeface, font_size_pixels, params);
}

}  // namespace gfx
