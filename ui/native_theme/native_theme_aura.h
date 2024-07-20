// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
#define UI_NATIVE_THEME_NATIVE_THEME_AURA_H_

#include "base/no_destructor.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

// Aura implementation of native theme support.
class NATIVE_THEME_EXPORT NativeThemeAura : public NativeThemeBase {
 protected:
  friend class NativeTheme;
  friend class NativeThemeAuraTest;
  friend class base::NoDestructor<NativeThemeAura>;

  NativeThemeAura(bool use_overlay_scrollbars,
                  bool should_only_use_dark_colors,
                  ui::SystemTheme system_theme = ui::SystemTheme::kDefault,
                  bool configure_web_instance = false);

  NativeThemeAura(const NativeThemeAura&) = delete;
  NativeThemeAura& operator=(const NativeThemeAura&) = delete;

  ~NativeThemeAura() override;

  static NativeThemeAura* web_instance();

  // Overridden from NativeTheme:
  SkColor4f FocusRingColorForBaseColor(SkColor4f base_color) const override;
  void ConfigureWebInstance() override;

  // NativeThemeBase:
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
  void PaintArrowButton(
      cc::PaintCanvas* gc,
      const ColorProvider* color_provider,
      const gfx::Rect& rect,
      Part direction,
      State state,
      ColorScheme color_scheme,
      bool in_forced_colors,
      const ScrollbarArrowExtraParams& extra_params) const override;
  void PaintScrollbarTrack(cc::PaintCanvas* canvas,
                           const ColorProvider* color_provider,
                           Part part,
                           State state,
                           const ScrollbarTrackExtraParams& extra_params,
                           const gfx::Rect& rect,
                           ColorScheme color_scheme,
                           bool in_forced_colors) const override;
  void PaintScrollbarThumb(cc::PaintCanvas* canvas,
                           const ColorProvider* color_provider,
                           Part part,
                           State state,
                           const gfx::Rect& rect,
                           const ScrollbarThumbExtraParams& extra_params,
                           ColorScheme color_scheme) const override;
  gfx::Insets GetScrollbarSolidColorThumbInsets(Part part) const override;
  SkColor4f GetScrollbarThumbColor(
      const ui::ColorProvider& color_provider,
      State state,
      const ScrollbarThumbExtraParams& extra) const override;
  void PaintScrollbarCorner(cc::PaintCanvas* canvas,
                            const ColorProvider* color_provider,
                            State state,
                            const gfx::Rect& rect,
                            const ScrollbarTrackExtraParams& extra_params,
                            ColorScheme color_scheme) const override;
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;

 private:
  static void DrawPartiallyRoundRect(cc::PaintCanvas* canvas,
                                     const gfx::Rect& rect,
                                     const SkScalar upper_left_radius,
                                     const SkScalar upper_right_radius,
                                     const SkScalar lower_right_radius,
                                     const SkScalar lower_left_radius,
                                     const cc::PaintFlags& flags);

  bool use_overlay_scrollbars_;

  // Used to notify the web native theme of changes to dark mode, high
  // contrast, preferred color scheme, and preferred contrast.
  std::unique_ptr<NativeTheme::ColorSchemeNativeThemeObserver>
      color_scheme_observer_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
