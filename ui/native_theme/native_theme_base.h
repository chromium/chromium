// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
#define UI_NATIVE_THEME_NATIVE_THEME_BASE_H_

#include "base/gtest_prod_util.h"
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
  NativeThemeBase(const NativeThemeBase&) = delete;
  NativeThemeBase& operator=(const NativeThemeBase&) = delete;

  // NativeTheme implementation:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;
  float GetBorderRadiusForPart(Part part,
                               float width,
                               float height) const override;
  void Paint(cc::PaintCanvas* canvas,
             const ui::ColorProvider* color_provider,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra,
             ColorScheme color_scheme,
             bool in_forced_colors,
             const std::optional<SkColor>& accent_color) const override;

  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;

 protected:
  // Colors for form controls refresh.
  enum ControlColorId {
    kBorder,
    kDisabledBorder,
    kHoveredBorder,
    kPressedBorder,
    kAccent,
    kDisabledAccent,
    kHoveredAccent,
    kPressedAccent,
    kBackground,
    kDisabledBackground,
    kFill,
    kDisabledFill,
    kHoveredFill,
    kPressedFill,
    kLightenLayer,
    kProgressValue,
    kSlider,
    kDisabledSlider,
    kHoveredSlider,
    kPressedSlider,
    kAutoCompleteBackground,
    kScrollbarArrowBackground,
    kScrollbarArrowBackgroundHovered,
    kScrollbarArrowBackgroundPressed,
    kScrollbarArrow,
    kScrollbarArrowHovered,
    kScrollbarArrowPressed,
    // TODO(crbug.com/40242489): kScrollbarCorner overlaps with
    // NativeTheme::Part::kScrollbarCorner. Make ControlColorId a enum class
    // or remove the class completely in favor of ColorProvider colors.
    kScrollbarCornerControlColorId,
    kScrollbarTrack,
    kScrollbarThumb,
    kScrollbarThumbHovered,
    kScrollbarThumbPressed,
    kScrollbarThumbInactive,
    kButtonBorder,
    kButtonDisabledBorder,
    kButtonHoveredBorder,
    kButtonPressedBorder,
    kButtonFill,
    kButtonDisabledFill,
    kButtonHoveredFill,
    kButtonPressedFill
  };

  using NativeTheme::NativeTheme;
  NativeThemeBase();
  explicit NativeThemeBase(
      bool should_only_use_dark_colors,
      ui::SystemTheme system_theme = ui::SystemTheme::kDefault);
  ~NativeThemeBase() override;

  // Draw the arrow. Used by scrollbar and inner spin button.
  virtual void PaintArrowButton(
      cc::PaintCanvas* gc,
      const ColorProvider* color_provider,
      const gfx::Rect& rect,
      Part direction,
      State state,
      ColorScheme color_scheme,
      bool in_forced_colors,
      const ScrollbarArrowExtraParams& extra_params) const;
  // Paint the scrollbar track. Done before the thumb so that it can contain
  // alpha.
  virtual void PaintScrollbarTrack(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect,
      ColorScheme color_scheme,
      bool in_forced_colors) const;
  // Draw the scrollbar thumb over the track.
  virtual void PaintScrollbarThumb(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      Part part,
      State state,
      const gfx::Rect& rect,
      const ScrollbarThumbExtraParams& extra_params,
      ColorScheme color_scheme) const;

  virtual void PaintScrollbarCorner(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const ScrollbarTrackExtraParams& extra_params,
      ColorScheme color_scheme) const;

  void PaintCheckbox(cc::PaintCanvas* canvas,
                     const ColorProvider* color_provider,
                     State state,
                     const gfx::Rect& rect,
                     const ButtonExtraParams& button,
                     ColorScheme color_scheme,
                     const std::optional<SkColor>& accent_color) const;

  void PaintRadio(cc::PaintCanvas* canvas,
                  const ColorProvider* color_provider,
                  State state,
                  const gfx::Rect& rect,
                  const ButtonExtraParams& button,
                  ColorScheme color_scheme,
                  const std::optional<SkColor>& accent_color) const;

  void PaintButton(cc::PaintCanvas* canvas,
                   const ColorProvider* color_provider,
                   State state,
                   const gfx::Rect& rect,
                   const ButtonExtraParams& button,
                   ColorScheme color_scheme) const;

  void PaintTextField(cc::PaintCanvas* canvas,
                      const ColorProvider* color_provider,
                      State state,
                      const gfx::Rect& rect,
                      const TextFieldExtraParams& text,
                      ColorScheme color_scheme) const;

  void PaintMenuList(cc::PaintCanvas* canvas,
                     const ColorProvider* color_provider,
                     State state,
                     const gfx::Rect& rect,
                     const MenuListExtraParams& menu_list,
                     ColorScheme color_scheme) const;

  virtual void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& menu_background,
      ColorScheme color_scheme) const;

  virtual void PaintMenuItemBackground(cc::PaintCanvas* canvas,
                                       const ColorProvider* color_provider,
                                       State state,
                                       const gfx::Rect& rect,
                                       const MenuItemExtraParams& menu_item,
                                       ColorScheme color_scheme) const;

  virtual void PaintMenuSeparator(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const MenuSeparatorExtraParams& menu_separator) const;

  void PaintSliderTrack(cc::PaintCanvas* canvas,
                        const ColorProvider* color_provider,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& slider,
                        ColorScheme color_scheme,
                        const std::optional<SkColor>& accent_color) const;

  void PaintSliderThumb(cc::PaintCanvas* canvas,
                        const ColorProvider* color_provider,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& slider,
                        ColorScheme color_scheme,
                        const std::optional<SkColor>& accent_color) const;

  virtual void PaintInnerSpinButton(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const InnerSpinButtonExtraParams& spin_button,
      ColorScheme color_scheme,
      bool in_forced_colors) const;

  void PaintProgressBar(cc::PaintCanvas* canvas,
                        const ColorProvider* color_provider,
                        State state,
                        const gfx::Rect& rect,
                        const ProgressBarExtraParams& progress_bar,
                        ColorScheme color_scheme,
                        const std::optional<SkColor>& accent_color) const;

  virtual void PaintFrameTopArea(cc::PaintCanvas* canvas,
                                 State state,
                                 const gfx::Rect& rect,
                                 const FrameTopAreaExtraParams& frame_top_area,
                                 ColorScheme color_scheme) const;

  virtual void PaintLightenLayer(cc::PaintCanvas* canvas,
                                 const ColorProvider* color_provider,
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
  SkPath PathForArrow(const gfx::Rect& bounding_rect, Part direction) const;

  // Returns the color used to draw the arrow.
  SkColor GetArrowColor(State state,
                        ColorScheme color_scheme,
                        const ColorProvider* color_provider) const;
  SkColor GetControlColor(ControlColorId color_id,
                          ColorScheme color_scheme,
                          const ColorProvider* color_provider) const;
  virtual SkColor ControlsAccentColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const;
  virtual SkColor ControlsSliderColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const;
  virtual SkColor ButtonBorderColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const;
  virtual SkColor ButtonFillColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const;
  virtual SkColor ControlsBorderColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const;
  virtual SkColor ControlsFillColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const;

  int scrollbar_width_ = 15;

 private:
  friend class NativeThemeAuraTest;

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
  SkRect PaintCheckboxRadioCommon(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button,
      bool is_checkbox,
      const SkScalar border_radius,
      ColorScheme color_scheme,
      const std::optional<SkColor>& accent_color) const;

  SkColor ControlsBackgroundColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const;
  SkColor GetDarkModeControlColor(ControlColorId color_id) const;

  SkColor GetControlColorFromColorProvider(
      ControlColorId color_id,
      const ColorProvider* color_provider) const;

  SkRect AlignSliderTrack(const gfx::Rect& slider_rect,
                          const NativeTheme::SliderExtraParams& slider,
                          bool is_value,
                          float track_height) const;

  // Returns true if the ColorProvider color map is not empty and a color
  // represented by the ControlColorId is added to the ui color mixer.
  // TODO(crbug.com/40242489): Remove this function when the NativeThemeBase
  // class is fully transitioned to the color pipeline and the GetControlColor()
  // is deleted.
  bool IsColorPipelineSupportedForControlColorId(
      const ColorProvider* color_provider,
      ControlColorId color_id) const;

  // The length of the arrow buttons, 0 means no buttons are drawn.
  int scrollbar_button_length_ = 14;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
