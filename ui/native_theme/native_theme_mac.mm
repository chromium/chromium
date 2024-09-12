// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#import <Cocoa/Cocoa.h>
#include <MediaAccessibility/MediaAccessibility.h>
#include <stddef.h>

#include <vector>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/no_destructor.h"
#include "cc/paint/paint_shader.h"
#include "ui/base/cocoa/defaults_utils.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_features.h"

namespace {

bool IsDarkMode() {
  NSAppearanceName appearance =
      [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua
      ]];
  return [appearance isEqual:NSAppearanceNameDarkAqua];
}

bool PrefersReducedTransparency() {
  return NSWorkspace.sharedWorkspace
      .accessibilityDisplayShouldReduceTransparency;
}

bool IsHighContrast() {
  return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldIncreaseContrast;
}

bool InvertedColors() {
  return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldInvertColors;
}

}  // namespace

// Helper object to respond to light mode/dark mode changeovers.
@interface NativeThemeEffectiveAppearanceObserver : NSObject
@end

@implementation NativeThemeEffectiveAppearanceObserver {
  void (^_handler)() __strong;
}

- (instancetype)initWithHandler:(void (^)())handler {
  self = [super init];
  if (self) {
    _handler = handler;
    [NSApp addObserver:self
            forKeyPath:@"effectiveAppearance"
               options:0
               context:nullptr];
  }
  return self;
}

- (void)dealloc {
  [NSApp removeObserver:self forKeyPath:@"effectiveAppearance"];
}

- (void)observeValueForKeyPath:(NSString*)forKeyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  _handler();
}

@end

namespace {

// Helper to make indexing an array by an enum class easier.
template <class KEY, class VALUE>
struct EnumArray {
  VALUE& operator[](const KEY& key) { return array[static_cast<size_t>(key)]; }
  VALUE array[static_cast<size_t>(KEY::COUNT)];
};

}  // namespace

namespace ui {

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  return NativeThemeMacWeb::instance();
}

// static
NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  return NativeThemeMac::instance();
}

NativeTheme* NativeTheme::GetInstanceForDarkUI() {
  static base::NoDestructor<NativeThemeMac> s_native_theme(
      /*configure_web_instance=*/false, /*should_only_use_dark_colors=*/true);
  return s_native_theme.get();
}

// static
bool NativeTheme::SystemDarkModeSupported() {
  return true;
}

// static
NativeThemeMac* NativeThemeMac::instance() {
  static base::NoDestructor<NativeThemeMac> s_native_theme(
      /*configure_web_instance=*/true, /*should_only_use_dark_colors=*/false);
  return s_native_theme.get();
}

NativeThemeAura::PreferredContrast NativeThemeMac::CalculatePreferredContrast()
    const {
  return IsHighContrast() ? NativeThemeAura::PreferredContrast::kMore
                          : NativeThemeAura::PreferredContrast::kNoPreference;
}

void NativeThemeMac::Paint(cc::PaintCanvas* canvas,
                           const ColorProvider* color_provider,
                           Part part,
                           State state,
                           const gfx::Rect& rect,
                           const ExtraParams& extra,
                           ColorScheme color_scheme,
                           bool in_forced_colors,
                           const std::optional<SkColor>& accent_color) const {
  ColorScheme color_scheme_updated = color_scheme;
  if (color_scheme_updated == ColorScheme::kDefault)
    color_scheme_updated = GetDefaultSystemColorScheme();

  if (rect.IsEmpty())
    return;

  switch (part) {
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      PaintMacScrollbarThumb(canvas, part, state, rect,
                             absl::get<ScrollbarExtraParams>(extra),
                             color_scheme_updated);
      break;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintMacScrollBarTrackOrCorner(canvas, part, state,
                                     absl::get<ScrollbarExtraParams>(extra),
                                     rect, color_scheme_updated, false);
      break;
    case kScrollbarCorner:
      PaintMacScrollBarTrackOrCorner(canvas, part, state,
                                     absl::get<ScrollbarExtraParams>(extra),
                                     rect, color_scheme_updated, true);
      break;
    default:
      NativeThemeBase::Paint(canvas, color_provider, part, state, rect, extra,
                             color_scheme, in_forced_colors, accent_color);
      break;
  }
}

void ConstrainInsets(int old_width, int min_width, int* left, int* right) {
  int requested_total_inset = *left + *right;
  if (requested_total_inset == 0)
    return;
  int max_total_inset = old_width - min_width;
  if (requested_total_inset < max_total_inset)
    return;
  if (max_total_inset < 0) {
    *left = *right = 0;
    return;
  }
  // Multiply the right/bottom inset by the ratio by which we need to shrink the
  // total inset. This has the effect of rounding down the right/bottom inset,
  // if the two sides are to be affected unevenly.
  // This is done instead of using inset scale functions to maintain expected
  // behavior and to map to how it looks like other scrollbars work on MacOS.
  *right *= max_total_inset * 1.0f / requested_total_inset;
  *left = max_total_inset - *right;
}

void ConstrainedInset(gfx::Rect* rect,
                      gfx::Size min_size,
                      gfx::Insets initial_insets) {
  int inset_left = initial_insets.left();
  int inset_right = initial_insets.right();
  int inset_top = initial_insets.top();
  int inset_bottom = initial_insets.bottom();

  ConstrainInsets(rect->width(), min_size.width(), &inset_left, &inset_right);
  ConstrainInsets(rect->height(), min_size.height(), &inset_top, &inset_bottom);
  rect->Inset(
      gfx::Insets::TLBR(inset_top, inset_left, inset_bottom, inset_right));
}

void NativeThemeMac::PaintMacScrollBarTrackOrCorner(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme,
    bool is_corner) const {
  if (is_corner && extra_params.is_overlay)
    return;
  PaintScrollBarTrackGradient(canvas, rect, extra_params, is_corner,
                              color_scheme);
  PaintScrollbarTrackInnerBorder(canvas, rect, extra_params, is_corner,
                                 color_scheme);
  PaintScrollbarTrackOuterBorder(canvas, rect, extra_params, is_corner,
                                 color_scheme);
}

void NativeThemeMac::PaintScrollBarTrackGradient(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool is_corner,
    ColorScheme color_scheme) const {
  gfx::Canvas paint_canvas(canvas, 1.0f);
  // Select colors.
  std::vector<SkColor4f> gradient_colors;
  bool dark_mode = color_scheme == ColorScheme::kDark;
  if (extra_params.is_overlay) {
    if (dark_mode) {
      gradient_colors = {SkColor4f{0.847f, 0.847f, 0.847f, 0.157f},
                         SkColor4f{0.8f, 0.8f, 0.8f, 0.149f},
                         SkColor4f{0.8f, 0.8f, 0.8f, 0.149f},
                         SkColor4f{0.8f, 0.8f, 0.8f, 0.149f}};
    } else {
      gradient_colors = {SkColor4f{0.973f, 0.973f, 0.973f, 0.776f},
                         SkColor4f{0.973f, 0.973f, 0.973f, 0.761f},
                         SkColor4f{0.973f, 0.973f, 0.973f, 0.761f},
                         SkColor4f{0.973f, 0.973f, 0.973f, 0.761f}};
    }
  } else {
    // Non-overlay scroller track colors are not transparent. On Safari, they
    // are, but on all other macOS applications they are not.
    if (dark_mode) {
      gradient_colors = {SkColor4f{0.176f, 0.176f, 0.176f, 1.0f},
                         SkColor4f{0.169f, 0.169f, 0.169f, 1.0f}};
    } else {
      gradient_colors = {SkColor4f{0.98f, 0.98f, 0.98f, 1.0f},
                         SkColor4f{0.98f, 0.98f, 0.98f, 1.0f}};
    }
  }

  // Set the gradient direction.
  std::vector<SkPoint> gradient_bounds;
  if (is_corner) {
    if (extra_params.orientation == ScrollbarOrientation::kVerticalOnRight) {
      gradient_bounds = {gfx::PointToSkPoint(rect.origin()),
                         gfx::PointToSkPoint(rect.bottom_right())};
    } else {
      gradient_bounds = {gfx::PointToSkPoint(rect.top_right()),
                         gfx::PointToSkPoint(rect.bottom_left())};
    }
  } else {
    if (extra_params.orientation == ScrollbarOrientation::kHorizontal) {
      gradient_bounds = {gfx::PointToSkPoint(rect.origin()),
                         gfx::PointToSkPoint(rect.top_right())};
    } else {
      gradient_bounds = {gfx::PointToSkPoint(rect.origin()),
                         gfx::PointToSkPoint(rect.bottom_left())};
    }
  }

  // And draw.
  cc::PaintFlags flags;
  std::optional<SkColor> track_color =
      GetScrollbarColor(ScrollbarPart::kTrack, color_scheme, extra_params);
  if (track_color.has_value()) {
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(track_color.value());
  } else {
    flags.setShader(cc::PaintShader::MakeLinearGradient(
        gradient_bounds.data(), gradient_colors.data(), nullptr,
        gradient_colors.size(), SkTileMode::kClamp));
  }
  paint_canvas.DrawRect(rect, flags);
}

void NativeThemeMac::PaintScrollbarTrackInnerBorder(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool is_corner,
    ColorScheme color_scheme) const {
  gfx::Canvas paint_canvas(canvas, 1.0f);

  // Compute the rect for the border.
  gfx::Rect inner_border(rect);
  if (extra_params.orientation == ScrollbarOrientation::kVerticalOnLeft)
    inner_border.set_x(rect.right() -
                       ScrollbarTrackBorderWidth(extra_params.scale_from_dip));
  if (is_corner ||
      extra_params.orientation == ScrollbarOrientation::kHorizontal)
    inner_border.set_height(
        ScrollbarTrackBorderWidth(extra_params.scale_from_dip));
  if (is_corner ||
      extra_params.orientation != ScrollbarOrientation::kHorizontal)
    inner_border.set_width(
        ScrollbarTrackBorderWidth(extra_params.scale_from_dip));

  // And draw.
  cc::PaintFlags flags;
  SkColor inner_border_color =
      GetScrollbarColor(ScrollbarPart::kTrackInnerBorder, color_scheme,
                        extra_params)
          .value();
  flags.setColor(inner_border_color);
  paint_canvas.DrawRect(inner_border, flags);
}

void NativeThemeMac::PaintScrollbarTrackOuterBorder(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool is_corner,
    ColorScheme color_scheme) const {
  gfx::Canvas paint_canvas(canvas, 1.0f);
  cc::PaintFlags flags;
  SkColor outer_border_color =
      GetScrollbarColor(ScrollbarPart::kTrackOuterBorder, color_scheme,
                        extra_params)
          .value();
  flags.setColor(outer_border_color);

  // Draw the horizontal outer border.
  if (is_corner ||
      extra_params.orientation == ScrollbarOrientation::kHorizontal) {
    gfx::Rect outer_border(rect);
    outer_border.set_height(
        ScrollbarTrackBorderWidth(extra_params.scale_from_dip));
    outer_border.set_y(rect.bottom() -
                       ScrollbarTrackBorderWidth(extra_params.scale_from_dip));
    paint_canvas.DrawRect(outer_border, flags);
  }

  // Draw the vertical outer border.
  if (is_corner ||
      extra_params.orientation != ScrollbarOrientation::kHorizontal) {
    gfx::Rect outer_border(rect);
    outer_border.set_width(
        ScrollbarTrackBorderWidth(extra_params.scale_from_dip));
    if (extra_params.orientation == ScrollbarOrientation::kVerticalOnRight)
      outer_border.set_x(rect.right() - ScrollbarTrackBorderWidth(
                                            extra_params.scale_from_dip));
    paint_canvas.DrawRect(outer_border, flags);
  }
}

gfx::Size NativeThemeMac::GetThumbMinSize(bool vertical, float scale) {
  const int kLength = 18 * scale;
  const int kGirth = 6 * scale;

  return vertical ? gfx::Size(kGirth, kLength) : gfx::Size(kLength, kGirth);
}

void NativeThemeMac::PaintMacScrollbarThumb(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& scroll_thumb,
    ColorScheme color_scheme) const {
  gfx::Canvas paint_canvas(canvas, 1.0f);

  // Compute the bounds for the rounded rect for the thumb from the bounds of
  // the thumb.
  gfx::Rect bounds(rect);
  {
    // Shrink the thumb evenly in length and girth to fit within the track.
    gfx::Insets thumb_insets(GetScrollbarThumbInset(
        scroll_thumb.is_overlay, scroll_thumb.scale_from_dip));

    // Also shrink the thumb in girth to not touch the border.
    if (scroll_thumb.orientation == ScrollbarOrientation::kHorizontal) {
      thumb_insets.set_top(
          thumb_insets.top() +
          ScrollbarTrackBorderWidth(scroll_thumb.scale_from_dip));
      ConstrainedInset(&bounds,
                       GetThumbMinSize(false, scroll_thumb.scale_from_dip),
                       thumb_insets);
    } else {
      thumb_insets.set_left(
          thumb_insets.left() +
          ScrollbarTrackBorderWidth(scroll_thumb.scale_from_dip));
      ConstrainedInset(&bounds,
                       GetThumbMinSize(true, scroll_thumb.scale_from_dip),
                       thumb_insets);
    }
  }

  // Draw.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  SkColor thumb_color =
      GetScrollbarColor(ScrollbarPart::kThumb, color_scheme, scroll_thumb)
          .value();
  flags.setColor(thumb_color);
  const SkScalar radius = std::min(bounds.width(), bounds.height());
  paint_canvas.DrawRoundRect(bounds, radius, flags);
}

std::optional<SkColor> NativeThemeMac::GetScrollbarColor(
    ScrollbarPart part,
    ColorScheme color_scheme,
    const ScrollbarExtraParams& extra_params) const {
  // This function is called from the renderer process through the scrollbar
  // drawing functions. Due to this, it cannot use any of the dynamic NS system
  // colors.
  bool dark_mode = color_scheme == ColorScheme::kDark;
  if (part == ScrollbarPart::kThumb) {
    if (extra_params.thumb_color.has_value()) {
      return extra_params.thumb_color.value();
    }
    if (extra_params.is_overlay)
      return dark_mode ? SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF)
                       : SkColorSetARGB(0x80, 0, 0, 0);

    if (dark_mode)
      return extra_params.is_hovering ? SkColorSetRGB(0x93, 0x93, 0x93)
                                      : SkColorSetRGB(0x6B, 0x6B, 0x6B);

    return extra_params.is_hovering ? SkColorSetARGB(0x80, 0, 0, 0)
                                    : SkColorSetARGB(0x3A, 0, 0, 0);
  } else if (part == ScrollbarPart::kTrackInnerBorder) {
    if (extra_params.track_color.has_value()) {
      return extra_params.track_color.value();
    }

    if (extra_params.is_overlay)
      return dark_mode ? SkColorSetARGB(0x33, 0xE5, 0xE5, 0xE5)
                       : SkColorSetARGB(0xF9, 0xDF, 0xDF, 0xDF);

    return dark_mode ? SkColorSetRGB(0x3D, 0x3D, 0x3D)
                     : SkColorSetRGB(0xE8, 0xE8, 0xE8);
  } else if (part == ScrollbarPart::kTrackOuterBorder) {
    if (extra_params.track_color.has_value()) {
      return extra_params.track_color.value();
    }
    if (extra_params.is_overlay)
      return dark_mode ? SkColorSetARGB(0x28, 0xD8, 0xD8, 0xD8)
                       : SkColorSetARGB(0xC6, 0xE8, 0xE8, 0xE8);

    return dark_mode ? SkColorSetRGB(0x51, 0x51, 0x51)
                     : SkColorSetRGB(0xED, 0xED, 0xED);
  } else if (part == ScrollbarPart::kTrack) {
    if (extra_params.track_color.has_value()) {
      return extra_params.track_color.value();
    }
  }

  return std::nullopt;
}

SkColor NativeThemeMac::GetSystemButtonPressedColor(SkColor base_color) const {
  // TODO crbug.com/1003612: This should probably be replaced with a color
  // transform.
  // Mac has a different "pressed button" styling because it doesn't use
  // ripples.
  return color_utils::GetResultingPaintColor(SkColorSetA(SK_ColorBLACK, 0x10),
                                             base_color);
}

void NativeThemeMac::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  DCHECK(color_provider);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color_provider->GetColor(kColorMenuBackground));
  const SkScalar radius = SkIntToScalar(menu_background.corner_radius);
  SkRect rect = gfx::RectToSkRect(gfx::Rect(size));
  canvas->drawRoundRect(rect, radius, radius, flags);
}

void NativeThemeMac::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item,
    ColorScheme color_scheme) const {
  switch (state) {
    case NativeTheme::kNormal:
    case NativeTheme::kDisabled:
      // Draw nothing over the regular background.
      break;
    case NativeTheme::kHovered:
      PaintSelectedMenuItem(canvas, color_provider, rect, menu_item);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

// static
static void CaptionSettingsChangedNotificationCallback(CFNotificationCenterRef,
                                                       void*,
                                                       CFStringRef,
                                                       const void*,
                                                       CFDictionaryRef) {
  NativeTheme::GetInstanceForWeb()->NotifyOnCaptionStyleUpdated();
}

NativeThemeMac::NativeThemeMac(bool configure_web_instance,
                               bool should_only_use_dark_colors)
    : NativeThemeBase(should_only_use_dark_colors) {
  if (!should_only_use_dark_colors)
    InitializeDarkModeStateAndObserver();

  set_prefers_reduced_transparency(PrefersReducedTransparency());
  set_inverted_colors(InvertedColors());
  if (!IsForcedHighContrast()) {
    SetPreferredContrast(CalculatePreferredContrast());
  }
  __block auto theme = this;
  display_accessibility_notification_token_ =
      [NSWorkspace.sharedWorkspace.notificationCenter
          addObserverForName:
              NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    if (!IsForcedHighContrast()) {
                      theme->SetPreferredContrast(CalculatePreferredContrast());
                    }
                    theme->set_prefers_reduced_transparency(
                        PrefersReducedTransparency());
                    theme->set_inverted_colors(InvertedColors());
                    theme->NotifyOnNativeThemeUpdated();
                  }];

  if (configure_web_instance)
    ConfigureWebInstance();
}

NativeThemeMac::~NativeThemeMac() {
  [NSNotificationCenter.defaultCenter
      removeObserver:display_accessibility_notification_token_];
}

std::optional<base::TimeDelta> NativeThemeMac::GetPlatformCaretBlinkInterval()
    const {
  // If there's insertion point flash rate info in NSUserDefaults, use the
  // blink period derived from that.
  return ui::TextInsertionCaretBlinkPeriodFromDefaults();
}

void NativeThemeMac::PaintSelectedMenuItem(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Rect& rect,
    const MenuItemExtraParams& extra_params) const {
  DCHECK(color_provider);
  // Draw the background.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color_provider->GetColor(kColorMenuItemBackgroundSelected));
  const SkScalar radius = SkIntToScalar(extra_params.corner_radius);
  canvas->drawRoundRect(gfx::RectToSkRect(rect), radius, radius, flags);
}

void NativeThemeMac::InitializeDarkModeStateAndObserver() {
  __block auto theme = this;
  set_use_dark_colors(IsDarkMode());
  set_preferred_color_scheme(CalculatePreferredColorScheme());
  appearance_observer_ =
      [[NativeThemeEffectiveAppearanceObserver alloc] initWithHandler:^{
        theme->set_use_dark_colors(IsDarkMode());
        theme->set_preferred_color_scheme(CalculatePreferredColorScheme());
        theme->NotifyOnNativeThemeUpdated();
      }];
}

void NativeThemeMac::ConfigureWebInstance() {
  // NativeThemeAura is used as web instance so we need to initialize its state.
  NativeTheme* web_instance = NativeTheme::GetInstanceForWeb();
  web_instance->set_use_dark_colors(IsDarkMode());
  web_instance->set_preferred_color_scheme(CalculatePreferredColorScheme());
  web_instance->SetPreferredContrast(CalculatePreferredContrast());
  web_instance->set_prefers_reduced_transparency(PrefersReducedTransparency());
  web_instance->set_inverted_colors(InvertedColors());

  // Add the web native theme as an observer to stay in sync with color scheme
  // changes.
  color_scheme_observer_ =
      std::make_unique<NativeTheme::ColorSchemeNativeThemeObserver>(
          NativeTheme::GetInstanceForWeb());
  AddObserver(color_scheme_observer_.get());

  // Observe caption style changes.
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetLocalCenter(), this,
      CaptionSettingsChangedNotificationCallback,
      kMACaptionAppearanceSettingsChangedNotification, nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

NativeThemeMacWeb::NativeThemeMacWeb()
    : NativeThemeAura(/*use_overlay_scrollbars=*/IsOverlayScrollbarEnabled(),
                      /*should_only_use_dark_colors=*/false) {}

// static
NativeThemeMacWeb* NativeThemeMacWeb::instance() {
  static base::NoDestructor<NativeThemeMacWeb> s_native_theme;
  return s_native_theme.get();
}

}  // namespace ui
