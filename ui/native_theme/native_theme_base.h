// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
#define UI_NATIVE_THEME_NATIVE_THEME_BASE_H_

#include <optional>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme.h"

namespace cc {
class PaintCanvas;
}

namespace ui {

class ColorProvider;
class NativeThemeBaseTest;

class COMPONENT_EXPORT(NATIVE_THEME) NativeThemeBase : public NativeTheme {
 public:
  NativeThemeBase(const NativeThemeBase&) = delete;
  NativeThemeBase& operator=(const NativeThemeBase&) = delete;

  // NativeTheme:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra_params) const override;
  float GetBorderRadiusForPart(Part part,
                               float width,
                               float height) const override;

 protected:
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
  ~NativeThemeBase() override;

  int scrollbar_button_length() const { return scrollbar_button_length_; }
  void set_scrollbar_button_length(int length) {
    scrollbar_button_length_ = length;
  }

  // NativeTheme:
  void PaintImpl(cc::PaintCanvas* canvas,
                 const ColorProvider* color_provider,
                 Part part,
                 State state,
                 const gfx::Rect& rect,
                 const ExtraParams& extra_params,
                 bool forced_colors,
                 bool dark_mode,
                 PreferredContrast contrast,
                 std::optional<SkColor> accent_color) const override;

  // Shrinks checkbox/radio button rect, if necessary, to make room for padding
  // and drop shadow.
  // TODO(mohsen): This is needed because checkboxes/radio buttons on Android
  // have different padding from those on desktop Chrome. Get rid of this when
  // crbug.com/530746 is resolved.
  virtual void AdjustCheckboxRadioRectForPadding(SkRect* rect) const;

  virtual SkColor GetControlColor(ControlColorId color_id,
                                  bool dark_mode,
                                  const ColorProvider* color_provider) const;

  // Returns the amount a hovered or pressed scrollbar part should contrast with
  // the normal version of that part. Used when there is a custom scrollbar part
  // color to try and mimic the default behavior.
  virtual float GetContrastRatioForState(State state, Part part) const;

  virtual void PaintFrameTopArea(
      cc::PaintCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const FrameTopAreaExtraParams& extra_params) const;

  virtual void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& extra_params) const;

  virtual void PaintMenuSeparator(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const MenuSeparatorExtraParams& extra_params) const;

  // Paints arrow buttons for scrollbars and inner spin buttons.
  virtual void PaintArrowButton(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      const gfx::Rect& rect,
      Part part,
      State state,
      bool forced_colors,
      bool dark_mode,
      PreferredContrast contrast,
      const ScrollbarArrowExtraParams& extra_params) const;

  virtual void PaintScrollbarThumb(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      Part part,
      State state,
      const gfx::Rect& rect,
      const ScrollbarThumbExtraParams& extra_params) const;

  virtual void PaintScrollbarTrack(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      Part part,
      State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect,
      bool forced_colors,
      PreferredContrast contrast) const;

  virtual void PaintScrollbarCorner(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const ScrollbarTrackExtraParams& extra_params) const;

  // Returns the color used to draw the arrow.
  SkColor GetArrowColor(State state,
                        bool dark_mode,
                        const ColorProvider* color_provider) const;

  SkColor ControlsAccentColorForState(
      State state,
      bool dark_mode,
      const ColorProvider* color_provider) const;
  SkColor ControlsSliderColorForState(
      State state,
      bool dark_mode,
      const ColorProvider* color_provider) const;
  SkColor ButtonBorderColorForState(State state,
                                    bool dark_mode,
                                    const ColorProvider* color_provider) const;
  SkColor ButtonFillColorForState(State state,
                                  bool dark_mode,
                                  const ColorProvider* color_provider) const;
  SkColor ControlsBorderColorForState(
      State state,
      bool dark_mode,
      const ColorProvider* color_provider) const;
  SkColor ControlsFillColorForState(State state,
                                    bool dark_mode,
                                    const ColorProvider* color_provider) const;

  SkColor SaturateAndBrighten(SkScalar* hsv,
                              SkScalar saturate_amount,
                              SkScalar brighten_amount) const;

  // For disabled controls, lightens the background so the translucent disabled
  // color works regardless of what it's over.
  void PaintLightenLayer(cc::PaintCanvas* canvas,
                         const ColorProvider* color_provider,
                         const SkRect& skrect,
                         State state,
                         float border_radius,
                         bool dark_mode) const;

  // Paints arrows for scrollbars and inner spin buttons.
  void PaintArrow(cc::PaintCanvas* canvas,
                  const gfx::Rect& rect,
                  Part part,
                  SkColor color) const;

  static SkPath PathForArrow(const gfx::RectF& rect, Part part);

  // Adjusts custom scrollbar button/thumb colors to meet contrast minima. When
  // `state` is hovered or pressed, `color` (if present) will be adjusted to
  // contrast with the normal state. If `bg_color` is present, also attempts to
  // ensure `color` maintains visible contrast with it.
  std::optional<SkColor> GetContrastingPressedOrHoveredColor(
      std::optional<SkColor> color,
      std::optional<SkColor> bg_color,
      State state,
      Part part) const;

  void PaintCheckbox(cc::PaintCanvas* canvas,
                     const ColorProvider* color_provider,
                     State state,
                     const gfx::Rect& rect,
                     const ButtonExtraParams& extra_params,
                     bool dark_mode,
                     std::optional<SkColor> accent_color) const;

  void PaintInnerSpinButton(cc::PaintCanvas* canvas,
                            const ColorProvider* color_provider,
                            State state,
                            gfx::Rect rect,
                            const InnerSpinButtonExtraParams& extra_params,
                            bool forced_colors,
                            bool dark_mode,
                            PreferredContrast contrast) const;

  void PaintMenuList(cc::PaintCanvas* canvas,
                     const ColorProvider* color_provider,
                     State state,
                     const gfx::Rect& rect,
                     const MenuListExtraParams& extra_params,
                     bool dark_mode) const;

  void PaintProgressBar(cc::PaintCanvas* canvas,
                        const ColorProvider* color_provider,
                        State state,
                        const gfx::Rect& rect,
                        const ProgressBarExtraParams& extra_params,
                        bool dark_mode,
                        PreferredContrast contrast,
                        std::optional<SkColor> accent_color) const;

  void PaintButton(cc::PaintCanvas* canvas,
                   const ColorProvider* color_provider,
                   State state,
                   const gfx::Rect& rect,
                   const ButtonExtraParams& extra_params,
                   bool dark_mode) const;

  void PaintRadio(cc::PaintCanvas* canvas,
                  const ColorProvider* color_provider,
                  State state,
                  const gfx::Rect& rect,
                  const ButtonExtraParams& extra_params,
                  bool dark_mode,
                  std::optional<SkColor> accent_color) const;

  void PaintSliderTrack(cc::PaintCanvas* canvas,
                        const ColorProvider* color_provider,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& extra_params,
                        bool dark_mode,
                        PreferredContrast contrast,
                        std::optional<SkColor> accent_color) const;

  void PaintSliderThumb(cc::PaintCanvas* canvas,
                        const ColorProvider* color_provider,
                        State state,
                        const gfx::Rect& rect,
                        const SliderExtraParams& extra_params,
                        bool dark_mode,
                        std::optional<SkColor> accent_color) const;

  void PaintTextField(cc::PaintCanvas* canvas,
                      const ColorProvider* color_provider,
                      State state,
                      const gfx::Rect& rect,
                      const TextFieldExtraParams& extra_params,
                      bool dark_mode) const;

  int scrollbar_width_ = 15;

 private:
  friend class NativeThemeAuraTest;
  friend class NativeThemeBaseTest;

  gfx::RectF GetArrowRect(const gfx::Rect& rect) const;

  SkColor ControlsBackgroundColorForState(
      State state,
      bool dark_mode,
      const ColorProvider* color_provider) const;

  SkColor OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const;

  // Draws the common elements of checkboxes and radio buttons. Returns the
  // rectangle within which any additional decorations should be drawn, or empty
  // if none.
  SkRect PaintCheckboxRadioCommon(cc::PaintCanvas* canvas,
                                  const ColorProvider* color_provider,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& button,
                                  bool is_checkbox,
                                  float border_radius,
                                  bool dark_mode,
                                  std::optional<SkColor> accent_color) const;

  // The length of the arrow buttons, 0 means no buttons are drawn.
  int scrollbar_button_length_ = 14;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
