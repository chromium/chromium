// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <MediaAccessibility/MediaAccessibility.h>
#include <stddef.h>

#include <algorithm>
#include <optional>
#include <variant>
#include <vector>

#include "base/no_destructor.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/native_theme.h"

namespace {

void ConstrainInsets(int old_width, int min_width, int* left, int* right) {
  const int requested_total_inset = *left + *right;
  if (requested_total_inset == 0) {
    return;
  }
  const int max_total_inset = old_width - min_width;
  if (requested_total_inset < max_total_inset) {
    return;
  }
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

ui::NativeTheme::PreferredColorScheme GetPreferredColorScheme() {
  NSAppearanceName appearance =
      [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua
      ]];
  return [appearance isEqual:NSAppearanceNameDarkAqua]
             ? ui::NativeTheme::PreferredColorScheme::kDark
             : ui::NativeTheme::PreferredColorScheme::kLight;
}

bool IsHighContrast() {
  return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldIncreaseContrast;
}

void CaptionSettingsChangedNotificationCallback(CFNotificationCenterRef,
                                                void*,
                                                CFStringRef,
                                                const void*,
                                                CFDictionaryRef) {
  ui::NativeTheme::GetInstanceForWeb()->NotifyOnCaptionStyleUpdated();
}

}  // namespace

// Helper object to respond to light mode/dark mode changeovers.
@interface EffectiveAppearanceObserver : NSObject
@end

@implementation EffectiveAppearanceObserver {
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

namespace ui {

struct NativeThemeMac::ObjCMembers {
  id __strong display_accessibility_notification_token;
  EffectiveAppearanceObserver* __strong appearance_observer;
};

gfx::Size NativeThemeMac::GetThumbMinSize(bool horizontal, float scale) {
  gfx::Size size = gfx::ScaleToRoundedSize({6, 18}, scale);
  if (horizontal) {
    size.Transpose();
  }
  return size;
}

SkColor NativeThemeMac::GetSystemButtonPressedColor(SkColor base_color) const {
  // Mac has a different "pressed button" styling because it doesn't use
  // ripples.
  // TODO(crbug.com/40098660): This should probably be replaced with a color
  // transform.
  return color_utils::GetResultingPaintColor(SkColorSetA(SK_ColorBLACK, 0x10),
                                             base_color);
}

void NativeThemeMac::PaintImpl(cc::PaintCanvas* canvas,
                               const ColorProvider* color_provider,
                               Part part,
                               State state,
                               const gfx::Rect& rect,
                               const ExtraParams& extra_params,
                               bool forced_colors,
                               bool dark_mode,
                               PreferredContrast contrast,
                               std::optional<SkColor> accent_color) const {
  // Mac uses bespoke scrollbar painting methods (instead of simply overriding
  // the parent ones) in order to pass `ScrollbarExtraParams`, which doesn't
  // exist on other platforms.
  if (part == kScrollbarHorizontalThumb || part == kScrollbarVerticalThumb) {
    PaintMacScrollbarThumb(canvas, part, state, rect,
                           std::get<ScrollbarExtraParams>(extra_params),
                           dark_mode);
    return;
  }
  if (part == kScrollbarHorizontalTrack || part == kScrollbarVerticalTrack ||
      part == kScrollbarCorner) {
    PaintMacScrollBarTrackOrCorner(canvas, part, state,
                                   std::get<ScrollbarExtraParams>(extra_params),
                                   rect, dark_mode, part == kScrollbarCorner);
    return;
  }

  NativeThemeBase::PaintImpl(canvas, color_provider, part, state, rect,
                             extra_params, forced_colors, dark_mode, contrast,
                             accent_color);
}

NativeTheme::PreferredContrast NativeThemeMac::CalculatePreferredContrast()
    const {
  return IsHighContrast() ? NativeTheme::PreferredContrast::kMore
                          : NativeTheme::PreferredContrast::kNoPreference;
}

NativeThemeMac::NativeThemeMac() {
  objc_members_ = std::make_unique<ObjCMembers>();

  InitializeDarkModeStateAndObserver();

  if (!IsForcedHighContrast()) {
    SetPreferredContrast(CalculatePreferredContrast());
    __block auto theme = this;
    objc_members_->display_accessibility_notification_token =
        [NSWorkspace.sharedWorkspace.notificationCenter
            addObserverForName:
                NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification* notification) {
                      theme->SetPreferredContrast(CalculatePreferredContrast());
                      theme->NotifyOnNativeThemeUpdated();
                    }];
  }

  if (static bool initialized = false; !initialized) {
    // Observe caption style changes. Technically these notify the web instance
    // rather than `this`, but there's a 1:1 relationship between the two, and
    // putting this code here allows simpler cross-platform
    // `GetInstanceFor...()` implementations.
    CFNotificationCenterAddObserver(
        CFNotificationCenterGetLocalCenter(), nullptr,
        CaptionSettingsChangedNotificationCallback,
        kMACaptionAppearanceSettingsChangedNotification, nullptr,
        CFNotificationSuspensionBehaviorDeliverImmediately);
    initialized = true;
  }
}

NativeThemeMac::~NativeThemeMac() {
  [NSNotificationCenter.defaultCenter
      removeObserver:objc_members_->display_accessibility_notification_token];
}

void NativeThemeMac::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& extra_params) const {
  if (state != kHovered) {
    return;
  }

  CHECK(color_provider);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color_provider->GetColor(kColorMenuItemBackgroundSelected));
  const SkScalar radius = SkIntToScalar(extra_params.corner_radius);
  canvas->drawRoundRect(gfx::RectToSkRect(rect), radius, radius, flags);
}

void NativeThemeMac::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const ColorProvider* color_provider,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& extra_params) const {
  CHECK(color_provider);
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color_provider->GetColor(kColorMenuBackground));
  const SkScalar radius = SkIntToScalar(extra_params.corner_radius);
  canvas->drawRoundRect(gfx::RectToSkRect(gfx::Rect(size)), radius, radius,
                        flags);
}

void NativeThemeMac::InitializeDarkModeStateAndObserver() {
  __block auto theme = this;
  set_preferred_color_scheme(GetPreferredColorScheme());
  objc_members_->appearance_observer =
      [[EffectiveAppearanceObserver alloc] initWithHandler:^{
        theme->set_preferred_color_scheme(GetPreferredColorScheme());
        theme->NotifyOnNativeThemeUpdated();
      }];
}

// This function is called from the renderer process through the scrollbar
// drawing functions. Due to this, it cannot use any of the dynamic NS system
// colors.
// TODO(pkasting): Consider whether these colors should instead go in a
// Mac-specific color mixer, which would mean scrollbars in web content would
// get these colors instead of Aura defaults.
std::optional<SkColor> NativeThemeMac::GetScrollbarColor(
    ScrollbarPart part,
    bool dark_mode,
    const ScrollbarExtraParams& extra_params) const {
  if (part == ScrollbarPart::kThumb) {
    if (extra_params.thumb_color.has_value()) {
      return extra_params.thumb_color.value();
    }
    if (extra_params.is_overlay) {
      return dark_mode ? SkColorSetARGB(0x80, 0xFF, 0xFF, 0xFF)
                       : SkColorSetARGB(0x80, 0, 0, 0);
    }

    if (dark_mode) {
      return extra_params.is_hovering ? SkColorSetRGB(0x93, 0x93, 0x93)
                                      : SkColorSetRGB(0x6B, 0x6B, 0x6B);
    }

    return extra_params.is_hovering ? SkColorSetARGB(0x80, 0, 0, 0)
                                    : SkColorSetARGB(0x3A, 0, 0, 0);
  } else if (part == ScrollbarPart::kTrackInnerBorder) {
    if (extra_params.track_color.has_value()) {
      return extra_params.track_color.value();
    }

    if (extra_params.is_overlay) {
      return dark_mode ? SkColorSetARGB(0x33, 0xE5, 0xE5, 0xE5)
                       : SkColorSetARGB(0xF9, 0xDF, 0xDF, 0xDF);
    }

    return dark_mode ? SkColorSetRGB(0x3D, 0x3D, 0x3D)
                     : SkColorSetRGB(0xE8, 0xE8, 0xE8);
  } else if (part == ScrollbarPart::kTrackOuterBorder) {
    if (extra_params.track_color.has_value()) {
      return extra_params.track_color.value();
    }
    if (extra_params.is_overlay) {
      return dark_mode ? SkColorSetARGB(0x28, 0xD8, 0xD8, 0xD8)
                       : SkColorSetARGB(0xC6, 0xE8, 0xE8, 0xE8);
    }

    return dark_mode ? SkColorSetRGB(0x51, 0x51, 0x51)
                     : SkColorSetRGB(0xED, 0xED, 0xED);
  } else if (part == ScrollbarPart::kTrack) {
    if (extra_params.track_color.has_value()) {
      return extra_params.track_color.value();
    }
  }

  return std::nullopt;
}

void NativeThemeMac::PaintMacScrollbarThumb(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool dark_mode) const {
  // Compute the bounds for the rounded rect for the thumb from the bounds of
  // the thumb.
  gfx::Rect bounds(rect);
  {
    // Shrink the thumb evenly in length and girth to fit within the track.
    gfx::Insets thumb_insets(GetScrollbarThumbInset(
        extra_params.is_overlay, extra_params.scale_from_dip));

    // Also shrink the thumb in girth to not touch the border.
    const bool horizontal =
        extra_params.orientation == ScrollbarOrientation::kHorizontal;
    if (horizontal) {
      thumb_insets.set_top(
          thumb_insets.top() +
          ScrollbarTrackBorderWidth(extra_params.scale_from_dip));
    } else {
      thumb_insets.set_left(
          thumb_insets.left() +
          ScrollbarTrackBorderWidth(extra_params.scale_from_dip));
    }

    const gfx::Size min_size = NativeThemeMac::GetThumbMinSize(
        horizontal, extra_params.scale_from_dip);
    ConstrainedInset(&bounds, min_size, thumb_insets);
  }

  const SkScalar radius = std::min(bounds.width(), bounds.height());

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(
      GetScrollbarColor(ScrollbarPart::kThumb, dark_mode, extra_params)
          .value());
  gfx::Canvas(canvas, 1.0f).DrawRoundRect(bounds, radius, flags);
}

void NativeThemeMac::PaintMacScrollBarTrackOrCorner(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarExtraParams& extra_params,
    const gfx::Rect& rect,
    bool dark_mode,
    bool is_corner) const {
  if (is_corner && extra_params.is_overlay) {
    return;
  }
  PaintScrollBarTrackGradient(canvas, rect, extra_params, is_corner, dark_mode);
  PaintScrollbarTrackInnerBorder(canvas, rect, extra_params, is_corner,
                                 dark_mode);
  PaintScrollbarTrackOuterBorder(canvas, rect, extra_params, is_corner,
                                 dark_mode);
}

void NativeThemeMac::PaintScrollBarTrackGradient(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool is_corner,
    bool dark_mode) const {
  cc::PaintFlags flags;
  std::optional<SkColor> track_color =
      GetScrollbarColor(ScrollbarPart::kTrack, dark_mode, extra_params);
  if (track_color.has_value()) {
    flags.setAntiAlias(true);
    flags.setColor(track_color.value());
  } else {
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

    // Select colors.
    std::vector<SkColor4f> gradient_colors;
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
      // On Safari non-overlay scrollbar track colors are transparent, but on
      // all other macOS applications they are not.
      if (dark_mode) {
        gradient_colors = {SkColor4f{0.176f, 0.176f, 0.176f, 1.0f},
                           SkColor4f{0.169f, 0.169f, 0.169f, 1.0f}};
      } else {
        gradient_colors = {SkColor4f{0.98f, 0.98f, 0.98f, 1.0f},
                           SkColor4f{0.98f, 0.98f, 0.98f, 1.0f}};
      }
    }

    flags.setShader(cc::PaintShader::MakeLinearGradient(
        gradient_bounds.data(), gradient_colors.data(), nullptr,
        gradient_colors.size(), SkTileMode::kClamp));
  }
  gfx::Canvas(canvas, 1.0f).DrawRect(rect, flags);
}

void NativeThemeMac::PaintScrollbarTrackInnerBorder(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool is_corner,
    bool dark_mode) const {
  // Compute the rect for the border.
  gfx::Rect inner_border(rect);
  const int border_width =
      ScrollbarTrackBorderWidth(extra_params.scale_from_dip);
  if (extra_params.orientation == ScrollbarOrientation::kVerticalOnLeft) {
    inner_border.set_x(rect.right() - border_width);
  }
  const bool horizontal =
      extra_params.orientation == ScrollbarOrientation::kHorizontal;
  if (is_corner || horizontal) {
    inner_border.set_height(border_width);
  }
  if (is_corner || !horizontal) {
    inner_border.set_width(border_width);
  }

  cc::PaintFlags flags;
  flags.setColor(GetScrollbarColor(ScrollbarPart::kTrackInnerBorder, dark_mode,
                                   extra_params)
                     .value());
  gfx::Canvas(canvas, 1.0f).DrawRect(inner_border, flags);
}

void NativeThemeMac::PaintScrollbarTrackOuterBorder(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    const ScrollbarExtraParams& extra_params,
    bool is_corner,
    bool dark_mode) const {
  gfx::Canvas paint_canvas(canvas, 1.0f);

  cc::PaintFlags flags;
  flags.setColor(GetScrollbarColor(ScrollbarPart::kTrackOuterBorder, dark_mode,
                                   extra_params)
                     .value());

  // Draw the horizontal outer border.
  const bool horizontal =
      extra_params.orientation == ScrollbarOrientation::kHorizontal;
  const int border_width =
      ScrollbarTrackBorderWidth(extra_params.scale_from_dip);
  if (is_corner || horizontal) {
    gfx::Rect outer_border(rect);
    outer_border.set_y(rect.bottom() - border_width);
    outer_border.set_height(border_width);
    paint_canvas.DrawRect(outer_border, flags);
  }

  // Draw the vertical outer border.
  if (is_corner || !horizontal) {
    gfx::Rect outer_border(rect);
    if (extra_params.orientation == ScrollbarOrientation::kVerticalOnRight) {
      outer_border.set_x(rect.right() - border_width);
    }
    outer_border.set_width(border_width);
    paint_canvas.DrawRect(outer_border, flags);
  }
}

}  // namespace ui
