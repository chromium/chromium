// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
#define UI_NATIVE_THEME_NATIVE_THEME_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "ui/gfx/geometry/size.h"
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

  // Adjusts an SkColor based on the current system control tint. For example,
  // if the current tint is "graphite", this function maps the provided value to
  // an appropriate gray.
  static SkColor ApplySystemControlTint(SkColor color);

  // Overridden from NativeTheme:
  SkColor GetSystemColor(ColorId color_id,
                         ColorScheme color_scheme) const override;

  // Overridden from NativeTheme:
  SkColor GetSystemButtonPressedColor(SkColor base_color) const override;

  // Overridden from NativeTheme:
  PreferredContrast CalculatePreferredContrast() const override;

  // Overridden from NativeThemeBase:
  void Paint(cc::PaintCanvas* canvas,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra,
             ColorScheme color_scheme) const override;
  void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background,
      ColorScheme color_scheme) const override;
  void PaintMenuItemBackground(cc::PaintCanvas* canvas,
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

 protected:
  friend class NativeTheme;
  friend class base::NoDestructor<NativeThemeMac>;
  static NativeThemeMac* instance();

  NativeThemeMac(bool configure_web_instance, bool should_only_use_dark_colors);
  ~NativeThemeMac() override;

 private:
  // Paint the selected menu item background, and a border for emphasis when in
  // high contrast.
  void PaintSelectedMenuItem(cc::PaintCanvas* canvas,
                             const gfx::Rect& rect,
                             ColorScheme color_scheme) const;

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

  // Used by the GetSystem to run the switch for MacOS override colors that may
  // use named NS system colors. This is a separate function from GetSystemColor
  // to make sure the NSAppearance can be set in a scoped way.
  base::Optional<SkColor> GetOSColor(ColorId color_id,
                                     ColorScheme color_scheme) const;

  enum ScrollbarPart {
    kThumb,
    kTrackInnerBorder,
    kTrackOuterBorder,
  };

  base::Optional<SkColor> GetScrollbarColor(
      ScrollbarPart part,
      ColorScheme color_scheme,
      const ScrollbarExtraParams& extra_params) const;

  int ScrollbarTrackBorderWidth() const { return 1; }

  // The amount the thumb is inset from the ends and the inside edge of track
  // border.
  int GetScrollbarThumbInset(bool is_overlay) const {
    return is_overlay ? 2 : 3;
  }

  // Returns the minimum size for the thumb. We will not inset the thumb if it
  // will be smaller than this size.
  gfx::Size GetThumbMinSize(bool vertical) const;

  base::scoped_nsobject<NativeThemeEffectiveAppearanceObserver>
      appearance_observer_;
  id high_contrast_notification_token_;

  // Used to notify the web native theme of changes to dark mode and high
  // contrast.
  std::unique_ptr<NativeTheme::ColorSchemeNativeThemeObserver>
      color_scheme_observer_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeMac);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
