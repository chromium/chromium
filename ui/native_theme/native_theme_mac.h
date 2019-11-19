// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
#define UI_NATIVE_THEME_NATIVE_THEME_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/no_destructor.h"
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

  // Overridden from NativeThemeBase:
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
  bool SystemDarkModeSupported() const override;

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

 private:
  NativeThemeMac();
  ~NativeThemeMac() override;

  // Paint the selected menu item background, and a border for emphasis when in
  // high contrast.
  void PaintSelectedMenuItem(cc::PaintCanvas* canvas,
                             const gfx::Rect& rect,
                             ColorScheme color_scheme) const;

  void InitializeDarkModeStateAndObserver();

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
