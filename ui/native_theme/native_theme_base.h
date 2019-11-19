// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
#define UI_NATIVE_THEME_NATIVE_THEME_BASE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "cc/paint/paint_flags.h"
#include "ui/native_theme/native_theme.h"

namespace gfx {
class Rect;
class Size;
}

namespace ui {

// Theme support for non-Windows toolkits.
class NATIVE_THEME_EXPORT NativeThemeBase : public NativeTheme {
 public:
  // NativeTheme implementation:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  void Paint(cc::PaintCanvas* canvas,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra,
             ColorScheme color_scheme) const override;

  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;

 protected:
  // Colors for form controls refresh.
  enum ControlColorId {
    kBorder,
    kDisabledBorder,
    kHoveredBorder,
    kAccent,
    kDisabledAccent,
    kHoveredAccent,
    kBackground,
    kDisabledBackground,
    kFill,
    kDisabledFill,
    kHoveredFill,
    kLightenLayer,
    kProgressValue,
    kSlider,
    kDisabledSlider,
    kHoveredSlider
  };

  NativeThemeBase();
  ~NativeThemeBase() override;

  // Draw the arrow. Used by scrollbar and inner spin button.
  virtual void PaintArrowButton(cc::PaintCanvas* gc,
                                const gfx::Rect& rect,
                                Part direction,
                                State state,
                                ColorScheme color_scheme,
                                const ScrollbarArrowExtraParams& arrow) const;
  // Paint the scrollbar track. Done before the thumb so that it can contain
  // alpha.
  virtual void PaintScrollbarTrack(
      cc::PaintCanvas* canvas,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect,
      ColorScheme color_scheme) const;
  // Draw the scrollbar thumb over the track.
  virtual void PaintScrollbarThumb(
      cc::PaintCanvas* canvas,
      Part part,
      State state,
      const gfx::Rect& rect,
      NativeTheme::ScrollbarOverlayColorTheme theme,
      ColorScheme color_scheme) const;

  virtual void PaintScrollbarCorner(cc::PaintCanvas* canvas,
                                    State state,
                                    const gfx::Rect& rect,
                                    ColorScheme color_scheme) const;

  void PaintCheckbox(cc::PaintCanvas* canvas,
                     State state,
                     const gfx::Rect& rect,
                     const ButtonExtraParams& button,
                     ColorScheme color_scheme) const;

  void PaintRadio(cc::PaintCanvas* canvas,
                  State state,
                  const gfx::Rect& rect,
                  const ButtonExtraParams& button,
                  ColorScheme color_scheme) const;

  void PaintButton(cc::PaintCanvas* canvas,
                   State state,
                   const gfx::Rect& rect,
                   const ButtonExtraParams& button,
                   ColorScheme color_scheme) const;

  void PaintTextField(cc::PaintCanvas* canvas,
                      State state,
                      const gfx::Rect& rect,
                      const TextFieldExtraParams& text,
                      ColorScheme color_scheme) const;

  void PaintMenuList(cc::PaintCanvas* canvas,
                     State state,
                     const gfx::Rect& rect,
                     const MenuListExtraParams& menu_list,
                     ColorScheme color_scheme) const;

  virtual void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background,
      ColorScheme color_scheme) const;

  virtual void PaintMenuItemBackground(cc::PaintCanvas* canvas,
                                       State state,
                                       const gfx::Rect& rect,
                                       const MenuItemExtraParams& menu_item,
                                       ColorScheme color_scheme) const;

  virtual void PaintMenuSeparator(
      cc::PaintCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const MenuSeparatorExtraParams& menu_separator,
      ColorScheme color_scheme) const;

  void PaintSliderTrack(cc::PaintCanvas* canvas,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& slider,
                        ColorScheme color_scheme) const;

  void PaintSliderThumb(cc::PaintCanvas* canvas,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& slider,
                        ColorScheme color_scheme) const;

  virtual void PaintInnerSpinButton(
      cc::PaintCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const InnerSpinButtonExtraParams& spin_button,
      ColorScheme color_scheme) const;

  void PaintProgressBar(cc::PaintCanvas* canvas,
                        State state,
                        const gfx::Rect& rect,
                        const ProgressBarExtraParams& progress_bar,
                        ColorScheme color_scheme) const;

  virtual void PaintFrameTopArea(cc::PaintCanvas* canvas,
                                 State state,
                                 const gfx::Rect& rect,
                                 const FrameTopAreaExtraParams& frame_top_area,
                                 ColorScheme color_scheme) const;

  virtual void PaintLightenLayer(cc::PaintCanvas* canvas,
                                 SkRect skrect,
                                 State state,
                                 SkScalar border_radius,
                                 ColorScheme color_scheme) const;

  // Shrinks checkbox/radio button rect, if necessary, to make room for padding
  // and drop shadow.
  // TODO(mohsen): This is needed because checkboxes/radio buttons on Android
  // have different padding from those on desktop Chrome. Get rid of this when
  // crbug.com/530746 is resolved.
  virtual void AdjustCheckboxRadioRectForPadding(SkRect* rect) const;

  void set_scrollbar_button_length(int length) {
    scrollbar_button_length_ = length;
  }
  int scrollbar_button_length() const { return scrollbar_button_length_; }

  SkColor SaturateAndBrighten(SkScalar* hsv,
                              SkScalar saturate_amount,
                              SkScalar brighten_amount) const;

  // Paints the arrow used on the scrollbar and spinner.
  void PaintArrow(cc::PaintCanvas* canvas,
                  const gfx::Rect& rect,
                  Part direction,
                  SkColor color) const;

  // Returns the color used to draw the arrow.
  SkColor GetArrowColor(State state, ColorScheme color_scheme) const;

  int scrollbar_width_;

 private:
  friend class NativeThemeAuraTest;

  SkPath PathForArrow(const gfx::Rect& rect, Part direction) const;
  gfx::Rect BoundingRectForArrow(const gfx::Rect& rect) const;

  void DrawVertLine(cc::PaintCanvas* canvas,
                    int x,
                    int y1,
                    int y2,
                    const cc::PaintFlags& flags) const;
  void DrawHorizLine(cc::PaintCanvas* canvas,
                     int x1,
                     int x2,
                     int y,
                     const cc::PaintFlags& flags) const;
  void DrawBox(cc::PaintCanvas* canvas,
               const gfx::Rect& rect,
               const cc::PaintFlags& flags) const;
  SkColor OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const;

  // Paint the common parts of the checkboxes and radio buttons.
  // border_radius specifies how rounded the corners should be.
  SkRect PaintCheckboxRadioCommon(cc::PaintCanvas* canvas,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& button,
                                  bool is_checkbox,
                                  const SkScalar border_radius,
                                  ColorScheme color_scheme) const;

  SkColor ControlsAccentColorForState(State state,
                                      ColorScheme color_scheme) const;
  SkColor ControlsBorderColorForState(State state,
                                      ColorScheme color_scheme) const;
  SkColor ControlsFillColorForState(State state,
                                    ColorScheme color_scheme) const;
  SkColor ControlsBackgroundColorForState(State state,
                                          ColorScheme color_scheme) const;
  SkColor ControlsSliderColorForState(State state,
                                      ColorScheme color_scheme) const;
  SkColor GetHighContrastControlColor(ControlColorId color_id,
                                      ColorScheme color_scheme) const;
  SkColor GetControlColor(ControlColorId color_id,
                          ColorScheme color_scheme) const;
  SkRect AlignSliderTrack(const gfx::Rect& slider_rect,
                          const NativeTheme::SliderExtraParams& slider,
                          bool is_value,
                          float track_height) const;

  // The length of the arrow buttons, 0 means no buttons are drawn.
  int scrollbar_button_length_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeBase);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
