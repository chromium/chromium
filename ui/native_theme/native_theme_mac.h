// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
#define UI_NATIVE_THEME_NATIVE_THEME_MAC_H_

#include "base/no_destructor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_base.h"
#include "ui/native_theme/native_theme_export.h"

@class NativeThemeEffectiveAppearanceObserver;

namespace ui {

// Mac implementation of native theme support.
class NATIVE_THEME_EXPORT NativeThemeMac : public NativeThemeBase {
 public:
  static const int kButtonCornerRadius = 3;

  // Type of gradient to use on a button background. Use HIGHLIGHTED for the
  // default button of a window and all combobox controls, but only when the
  // window is active.
  enum class ButtonBackgroundType {
    DISABLED,
    HIGHLIGHTED,
    NORMAL,
    PRESSED,
    COUNT
  };

  NativeThemeMac(const NativeThemeMac&) = delete;
  NativeThemeMac& operator=(const NativeThemeMac&) = delete;

  // NativeTheme:
  SkColor GetSystemButtonPressedColor(SkColor base_color) const override;
  PreferredContrast CalculatePreferredContrast() const override;

  // NativeThemeBase:
  void Paint(cc::PaintCanvas* canvas,
             const ColorProvider* color_provider,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra,
             ColorScheme color_scheme,
             bool in_forced_colors,
             const std::optional<SkColor>& accent_color) const override;
  void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background,
      ColorScheme color_scheme) const override;
  void PaintMenuItemBackground(cc::PaintCanvas* canvas,
                               const ColorProvider* color_provider,
                               State state,
                               const gfx::Rect& rect,
                               const MenuItemExtraParams& menu_item,
                               ColorScheme color_scheme) const override;
  void PaintMacScrollbarThumb(cc::PaintCanvas* canvas,
                              Part part,
                              State state,
                              const gfx::Rect& rect,
                              const ScrollbarExtraParams& scroll_thumb,
                              ColorScheme color_scheme) const;
  // Paint the track. |track_bounds| is the bounds for the track.
  void PaintMacScrollBarTrackOrCorner(cc::PaintCanvas* canvas,
                                      Part part,
                                      State state,
                                      const ScrollbarExtraParams& extra_params,
                                      const gfx::Rect& rect,
                                      ColorScheme color_scheme,
                                      bool is_corner) const;

  // Paints the styled button shape used for default controls on Mac. The basic
  // style is used for dialog buttons, comboboxes, and tabbed pane tabs.
  // Depending on the control part being drawn, the left or the right side can
  // be given rounded corners.
  static void PaintStyledGradientButton(cc::PaintCanvas* canvas,
                                        const gfx::Rect& bounds,
                                        ButtonBackgroundType type,
                                        bool round_left,
                                        bool round_right,
                                        bool focus);

  // Returns the minimum size for the thumb. We will not inset the thumb if it
  // will be smaller than this size. The scale parameter should be the device
  // scale factor.
  static gfx::Size GetThumbMinSize(bool vertical, float scale);

 protected:
  friend class NativeTheme;
  friend class base::NoDestructor<NativeThemeMac>;
  static NativeThemeMac* instance();

  NativeThemeMac(bool configure_web_instance, bool should_only_use_dark_colors);
  ~NativeThemeMac() override;

  // NativeTheme:
  std::optional<base::TimeDelta> GetPlatformCaretBlinkInterval() const override;

 private:
  // Paint the selected menu item background, and a border for emphasis when in
  // high contrast.
  void PaintSelectedMenuItem(cc::PaintCanvas* canvas,
                             const ColorProvider* color_provider,
                             const gfx::Rect& rect,
                             const MenuItemExtraParams& extra_params) const;

  void PaintScrollBarTrackGradient(cc::PaintCanvas* canvas,
                                   const gfx::Rect& rect,
                                   const ScrollbarExtraParams& extra_params,
                                   bool is_corner,
                                   ColorScheme color_scheme) const;
  void PaintScrollbarTrackInnerBorder(cc::PaintCanvas* canvas,
                                      const gfx::Rect& rect,
                                      const ScrollbarExtraParams& extra_params,
                                      bool is_corner,
                                      ColorScheme color_scheme) const;
  void PaintScrollbarTrackOuterBorder(cc::PaintCanvas* canvas,
                                      const gfx::Rect& rect,
                                      const ScrollbarExtraParams& extra_params,
                                      bool is_corner,
                                      ColorScheme color_scheme) const;

  void InitializeDarkModeStateAndObserver();

  void ConfigureWebInstance() override;

  enum ScrollbarPart {
    kThumb,
    kTrack,
    kTrackInnerBorder,
    kTrackOuterBorder,
  };

  std::optional<SkColor> GetScrollbarColor(
      ScrollbarPart part,
      ColorScheme color_scheme,
      const ScrollbarExtraParams& extra_params) const;

  int ScrollbarTrackBorderWidth(float scale_from_dip) const {
    constexpr float border_width = 1.0f;
    return scale_from_dip * border_width;
  }

  // The amount the thumb is inset from the ends and the inside edge of track
  // border.
  int GetScrollbarThumbInset(bool is_overlay, float scale_from_dip) const {
    return scale_from_dip * (is_overlay ? 2.0f : 3.0f);
  }

  NativeThemeEffectiveAppearanceObserver* __strong appearance_observer_;
  id __strong display_accessibility_notification_token_;

  // Used to notify the web native theme of changes to dark mode and high
  // contrast.
  std::unique_ptr<NativeTheme::ColorSchemeNativeThemeObserver>
      color_scheme_observer_;
};

// Mac implementation of native theme support for web controls.
// For consistency with older versions of Chrome for Mac, we do multiply
// the border width and radius by the zoom, unlike the generic impl.
class NativeThemeMacWeb : public NativeThemeAura {
 public:
  NativeThemeMacWeb();

  static NativeThemeMacWeb* instance();
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
