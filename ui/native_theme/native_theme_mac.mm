// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_block.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#import "skia/ext/skia_utils_mac.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_aura.h"

namespace {

bool IsDarkMode() {
  if (@available(macOS 10.14, *)) {
    NSAppearanceName appearance =
        [[NSApp effectiveAppearance] bestMatchFromAppearancesWithNames:@[
          NSAppearanceNameAqua, NSAppearanceNameDarkAqua
        ]];
    return [appearance isEqual:NSAppearanceNameDarkAqua];
  }
  return false;
}

bool IsHighContrast() {
  NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
  if ([workspace respondsToSelector:@selector
                 (accessibilityDisplayShouldIncreaseContrast)]) {
    return workspace.accessibilityDisplayShouldIncreaseContrast;
  }
  return false;
}
}  // namespace

@interface NSWorkspace (Redeclarations)

@property(readonly) BOOL accessibilityDisplayShouldIncreaseContrast;

@end

// Helper object to respond to light mode/dark mode changeovers.
@interface NativeThemeEffectiveAppearanceObserver : NSObject
@end

@implementation NativeThemeEffectiveAppearanceObserver {
  base::mac::ScopedBlock<void (^)()> handler_;
}

- (instancetype)initWithHandler:(void (^)())handler {
  self = [super init];
  if (self) {
    handler_.reset([handler copy]);
    if (@available(macOS 10.14, *)) {
      [NSApp addObserver:self
              forKeyPath:@"effectiveAppearance"
                 options:0
                 context:nullptr];
    }
  }
  return self;
}

- (void)dealloc {
  if (@available(macOS 10.14, *)) {
    [NSApp removeObserver:self forKeyPath:@"effectiveAppearance"];
  }
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)forKeyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  handler_.get()();
}

@end

namespace {

// Helper to make indexing an array by an enum class easier.
template <class KEY, class VALUE>
struct EnumArray {
  VALUE& operator[](const KEY& key) { return array[static_cast<size_t>(key)]; }
  VALUE array[static_cast<size_t>(KEY::COUNT)];
};

// NSColor has a number of methods that return system colors (i.e. controlled by
// user preferences). This function converts the color given by an NSColor class
// method to an SkColor. Official documentation suggests developers only rely on
// +[NSColor selectedTextBackgroundColor] and +[NSColor selectedControlColor],
// but other colors give a good baseline. For many, a gradient is involved; the
// palette chosen based on the enum value given by +[NSColor currentColorTint].
// Apple's documentation also suggests to use NSColorList, but the system color
// list is just populated with class methods on NSColor.
SkColor NSSystemColorToSkColor(NSColor* color) {
  // System colors use the an NSNamedColorSpace called "System", so first step
  // is to convert the color into something that can be worked with.
  NSColor* device_color =
      [color colorUsingColorSpace:[NSColorSpace deviceRGBColorSpace]];
  if (device_color)
    return skia::NSDeviceColorToSkColor(device_color);

  // Sometimes the conversion is not possible, but we can get an approximation
  // by going through a CGColorRef. Note that simply using NSColor methods for
  // accessing components for system colors results in exceptions like
  // "-numberOfComponents not valid for the NSColor NSNamedColorSpace System
  // windowBackgroundColor; need to first convert colorspace." Hence the
  // conversion first to CGColor.
  CGColorRef cg_color = [color CGColor];
  const size_t component_count = CGColorGetNumberOfComponents(cg_color);
  if (component_count == 4)
    return skia::CGColorRefToSkColor(cg_color);

  CHECK(component_count == 1 || component_count == 2);
  // 1-2 components means a grayscale channel and maybe an alpha channel, which
  // CGColorRefToSkColor will not like. But RGB is additive, so the conversion
  // is easy (RGB to grayscale is less easy).
  const CGFloat* components = CGColorGetComponents(cg_color);
  CGFloat alpha = component_count == 2 ? components[1] : 1.0;
  return SkColorSetARGB(SkScalarRoundToInt(255.0 * alpha),
                        SkScalarRoundToInt(255.0 * components[0]),
                        SkScalarRoundToInt(255.0 * components[0]),
                        SkScalarRoundToInt(255.0 * components[0]));
}

// Converts an SkColor to grayscale by using luminance for all three components.
// Experimentally, this seems to produce a better result than a flat average or
// a min/max average for UI controls.
SkColor ColorToGrayscale(SkColor color) {
  SkScalar luminance = SkColorGetR(color) * 0.21 +
                       SkColorGetG(color) * 0.72 +
                       SkColorGetB(color) * 0.07;
  uint8_t component = SkScalarRoundToInt(luminance);
  return SkColorSetARGB(SkColorGetA(color), component, component, component);
}

}  // namespace

namespace ui {

// static
NativeTheme* NativeTheme::GetInstanceForWeb() {
  if (features::IsFormControlsRefreshEnabled())
    return NativeThemeAura::web_instance();
  return NativeThemeMac::instance();
}

// static
NativeTheme* NativeTheme::GetInstanceForNativeUi() {
  return NativeThemeMac::instance();
}

// static
NativeThemeMac* NativeThemeMac::instance() {
  static base::NoDestructor<NativeThemeMac> s_native_theme;
  return s_native_theme.get();
}

// static
SkColor NativeThemeMac::ApplySystemControlTint(SkColor color) {
  if ([NSColor currentControlTint] == NSGraphiteControlTint)
    return ColorToGrayscale(color);
  return color;
}

SkColor NativeThemeMac::GetSystemColor(ColorId color_id,
                                       ColorScheme color_scheme) const {
  if (color_scheme == ColorScheme::kDefault)
    color_scheme = GetDefaultSystemColorScheme();

  // Empirically, currentAppearance is incorrect when switching
  // appearances. It's unclear exactly why right now, so work
  // around it for the time being by resynchronizing.
  if (@available(macOS 10.14, *)) {
    NSAppearance* effective_appearance = [NSApp effectiveAppearance];
    if (![effective_appearance isEqual:[NSAppearance currentAppearance]]) {
      [NSAppearance setCurrentAppearance:effective_appearance];
    }
  }

  if (UsesHighContrastColors()) {
    switch (color_id) {
      case kColorId_SelectedMenuItemForegroundColor:
        return color_scheme == ColorScheme::kDark ? SK_ColorBLACK
                                                  : SK_ColorWHITE;
      case kColorId_FocusedMenuItemBackgroundColor:
        return color_scheme == ColorScheme::kDark ? SK_ColorLTGRAY
                                                  : SK_ColorDKGRAY;
      default:
        break;
    }
  }
  // Even with --secondary-ui-md, menus use the platform colors and styling, and
  // Mac has a couple of specific color overrides, documented below.
  switch (color_id) {
    case kColorId_EnabledMenuItemForegroundColor:
      return NSSystemColorToSkColor([NSColor controlTextColor]);
    case kColorId_DisabledMenuItemForegroundColor:
      return NSSystemColorToSkColor([NSColor disabledControlTextColor]);
    case kColorId_MenuSeparatorColor:
      return color_scheme == ColorScheme::kDark
                 ? SkColorSetA(gfx::kGoogleGrey800, 0xCC)
                 : SkColorSetA(SK_ColorBLACK, 0x26);
    case kColorId_MenuBorderColor:
      return SkColorSetA(SK_ColorBLACK, 0x60);

    // Mac has a different "pressed button" styling because it doesn't use
    // ripples.
    case kColorId_ButtonPressedShade:
      return SkColorSetA(SK_ColorBLACK, 0x10);

    // There's a system setting General > Highlight color which sets the
    // background color for text selections. We honor that setting.
    // TODO(ellyjones): Listen for NSSystemColorsDidChangeNotification somewhere
    // and propagate it to the View hierarchy.
    case kColorId_LabelTextSelectionBackgroundFocused:
    case kColorId_TextfieldSelectionBackgroundFocused:
      return NSSystemColorToSkColor([NSColor selectedTextBackgroundColor]);

    case kColorId_FocusedBorderColor:
      return SkColorSetA(
          NSSystemColorToSkColor([NSColor keyboardFocusIndicatorColor]),
          0x66);

    default:
      break;
  }

  return ApplySystemControlTint(GetAuraColor(color_id, this, color_scheme));
}

void NativeThemeMac::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(GetSystemColor(kColorId_MenuBackgroundColor, color_scheme));
  const SkScalar radius = SkIntToScalar(menu_background.corner_radius);
  SkRect rect = gfx::RectToSkRect(gfx::Rect(size));
  canvas->drawRoundRect(rect, radius, radius, flags);
}

void NativeThemeMac::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
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
      PaintSelectedMenuItem(canvas, rect, color_scheme);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool NativeThemeMac::SystemDarkModeSupported() const {
  if (@available(macOS 10.14, *)) {
    return true;
  }
  return false;
}

NativeThemeMac::NativeThemeMac() {
  InitializeDarkModeStateAndObserver();

  if (!IsForcedHighContrast()) {
    set_high_contrast(IsHighContrast());
    __block auto theme = this;
    high_contrast_notification_token_ =
        [[[NSWorkspace sharedWorkspace] notificationCenter]
            addObserverForName:
                NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                        object:nil
                         queue:nil
                    usingBlock:^(NSNotification* notification) {
                      theme->set_high_contrast(IsHighContrast());
                      theme->NotifyObservers();
                    }];
  }

  // Add the web native theme as an observer to stay in sync with dark mode,
  // high contrast, and preferred color scheme changes.
  if (features::IsFormControlsRefreshEnabled()) {
    color_scheme_observer_ =
        std::make_unique<NativeTheme::ColorSchemeNativeThemeObserver>(
            NativeTheme::GetInstanceForWeb());
    AddObserver(color_scheme_observer_.get());
  }
}

NativeThemeMac::~NativeThemeMac() {
  [[NSNotificationCenter defaultCenter]
      removeObserver:high_contrast_notification_token_];
}

void NativeThemeMac::PaintSelectedMenuItem(cc::PaintCanvas* canvas,
                                           const gfx::Rect& rect,
                                           ColorScheme color_scheme) const {
  // Draw the background.
  cc::PaintFlags flags;
  flags.setColor(
      GetSystemColor(kColorId_FocusedMenuItemBackgroundColor, color_scheme));
  canvas->drawRect(gfx::RectToSkRect(rect), flags);
}

void NativeThemeMac::InitializeDarkModeStateAndObserver() {
  __block auto theme = this;
  set_use_dark_colors(IsDarkMode());
  set_preferred_color_scheme(CalculatePreferredColorScheme());
  appearance_observer_.reset(
      [[NativeThemeEffectiveAppearanceObserver alloc] initWithHandler:^{
        theme->set_use_dark_colors(IsDarkMode());
        theme->set_preferred_color_scheme(CalculatePreferredColorScheme());
        theme->NotifyObservers();
      }]);
}

}  // namespace ui
