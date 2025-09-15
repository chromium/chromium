// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
#define UI_NATIVE_THEME_NATIVE_THEME_AURA_H_

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

// Aura implementation of native theme support.
class COMPONENT_EXPORT(NATIVE_THEME) NativeThemeAura : public NativeThemeBase {
 public:
  NativeThemeAura(const NativeThemeAura&) = delete;
  NativeThemeAura& operator=(const NativeThemeAura&) = delete;

  // NativeThemeBase:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  gfx::Insets GetScrollbarSolidColorThumbInsets(Part part) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;
  SkColor GetScrollbarThumbColor(
      const ui::ColorProvider& color_provider,
      State state,
      const ScrollbarThumbExtraParams& extra) const override;

 protected:
  explicit NativeThemeAura(bool use_overlay_scrollbar = false);
  explicit NativeThemeAura(SystemTheme system_theme);
  ~NativeThemeAura() override;

  // NativeThemeBase:
  float GetContrastRatioForState(State state, Part part) const override;
  void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background) const override;
  void PaintArrowButton(
      cc::PaintCanvas* gc,
      const ColorProvider* color_provider,
      const gfx::Rect& rect,
      Part direction,
      State state,
      bool forced_colors,
      bool dark_mode,
      PreferredContrast contrast,
      const ScrollbarArrowExtraParams& extra_params) const override;
  void PaintScrollbarThumb(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      Part part,
      State state,
      const gfx::Rect& rect,
      const ScrollbarThumbExtraParams& extra_params) const override;
  void PaintScrollbarTrack(cc::PaintCanvas* canvas,
                           const ColorProvider* color_provider,
                           Part part,
                           State state,
                           const ScrollbarTrackExtraParams& extra_params,
                           const gfx::Rect& rect,
                           bool forced_colors,
                           PreferredContrast contrast) const override;
  void PaintScrollbarCorner(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const ScrollbarTrackExtraParams& extra_params) const override;

 private:
  friend class NativeTheme;
  friend class NativeThemeAuraTest;
  friend class base::NoDestructor<NativeThemeAura>;

  static void DrawPartiallyRoundRect(cc::PaintCanvas* canvas,
                                     const gfx::Rect& rect,
                                     const SkScalar upper_left_radius,
                                     const SkScalar upper_right_radius,
                                     const SkScalar lower_right_radius,
                                     const SkScalar lower_left_radius,
                                     const cc::PaintFlags& flags);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_AURA_H_
