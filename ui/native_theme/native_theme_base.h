// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
#define UI_NATIVE_THEME_NATIVE_THEME_BASE_H_

#include <array>
#include <optional>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/color/color_id.h"
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
  SkColor GetScrollbarThumbColor(
      const ColorProvider* color_provider,
      State state,
      const ScrollbarThumbExtraParams& extra_params) const override;

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
    kCheckboxBackground,
    kDisabledCheckboxBackground,
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
    kSliderBorder,
    kHoveredSliderBorder,
    kPressedSliderBorder,
    kAutoCompleteBackground,
    kScrollbarArrowBackground,
    kScrollbarArrowBackgroundDisabled,
    kScrollbarArrowBackgroundHovered,
    kScrollbarArrowBackgroundPressed,
    kScrollbarArrow,
    kScrollbarArrowDisabled,
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
    kButtonBorder,
    kButtonDisabledBorder,
    kButtonHoveredBorder,
    kButtonPressedBorder,
    kButtonFill,
    kButtonDisabledFill,
    kButtonHoveredFill,
    kButtonPressedFill
  };

  static constexpr auto kButtonBorderColors =
      std::to_array({kButtonDisabledBorder, kButtonHoveredBorder, kButtonBorder,
                     kButtonPressedBorder});

  using NativeTheme::NativeTheme;
  ~NativeThemeBase() override;

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

  // Returns the size of a vertical scrollbar button. Horizontal scrollbars
  // transpose this value.
  //
  // NOTE: The width here is also assumed to be the track width, so should be
  // nonzero even if buttons should not be drawn.
  virtual gfx::Size GetVerticalScrollbarButtonSize() const;

  // Returns the size of a vertical scrollbar thumb. Horizontal scrollbars
  // transpose this value.
  virtual gfx::Size GetVerticalScrollbarThumbSize() const;

  // Returns the scrollbar arrow rect, given an arrow button rect of `rect`.
  virtual gfx::RectF GetArrowRect(const gfx::Rect& rect,
                                  Part part,
                                  State state) const;

  virtual SkColor GetControlColor(ControlColorId color_id,
                                  bool dark_mode,
                                  PreferredContrast contrast,
                                  const ColorProvider* color_provider) const;

  // Returns any custom color ID to use based on `state` and `extra_params`. If
  // this returns null, the default thumb color for the state will be used.
  virtual std::optional<ColorId> GetScrollbarThumbColorId(
      State state,
      const ScrollbarThumbExtraParams& extra_params) const;

  // Returns the amount a hovered or pressed scrollbar part should contrast with
  // the normal version of that part. Used when there is a custom scrollbar part
  // color to try and mimic the default behavior.
  virtual float GetScrollbarPartContrastRatioForState(State state) const;

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

  // Selects a color from `colors` based on `state`, then returns the physical
  // color. `colors` must be in the same order as the actual `State` enum.
  SkColor GetControlColorForState(base::span<const ControlColorId, 4> colors,
                                  State state,
                                  bool dark_mode,
                                  PreferredContrast contrast,
                                  const ColorProvider* color_provider) const;

  SkColor GetScrollbarArrowBackgroundColor(
      const ScrollbarArrowExtraParams& extra_params,
      State state,
      bool dark_mode,
      PreferredContrast contrast,
      const ColorProvider* color_provider) const;

  SkColor GetScrollbarArrowForegroundColor(
      SkColor bg_color,
      const ScrollbarArrowExtraParams& extra_params,
      State state,
      bool dark_mode,
      PreferredContrast contrast,
      const ColorProvider* color_provider) const;

  // For disabled controls, lightens the background so the translucent disabled
  // color works regardless of what it's over.
  void PaintLightenLayer(cc::PaintCanvas* canvas,
                         const ColorProvider* color_provider,
                         const SkRect& skrect,
                         State state,
                         float border_radius,
                         bool dark_mode,
                         PreferredContrast contrast) const;

  // Paints arrows for scrollbars and inner spin buttons.
  void PaintArrow(cc::PaintCanvas* canvas,
                  const gfx::Rect& rect,
                  Part part,
                  State state,
                  SkColor color) const;

 private:
  friend class NativeThemeBaseTest;

  static constexpr auto kBorderColors =
      std::to_array({kDisabledBorder, kHoveredBorder, kBorder, kPressedBorder});
  static constexpr auto kAccentColors =
      std::to_array({kDisabledAccent, kHoveredAccent, kAccent, kPressedAccent});
  static constexpr auto kCheckboxBackgroundColors =
      std::to_array({kDisabledCheckboxBackground, kCheckboxBackground,
                     kCheckboxBackground, kCheckboxBackground});
  static constexpr auto kFillColors =
      std::to_array({kDisabledFill, kHoveredFill, kFill, kPressedFill});
  static constexpr auto kSliderColors =
      std::to_array({kDisabledSlider, kHoveredSlider, kSlider, kPressedSlider});
  static constexpr auto kSliderBorderColors =
      std::to_array({kDisabledBorder, kHoveredSliderBorder, kSliderBorder,
                     kPressedSliderBorder});

  static SkPath PathForArrow(const gfx::RectF& rect, Part part);

  // Like `GetControlColorForState()`; however, if `accent_color` is non-null
  // and `state` is not `kDisabled`, overrides the default colors with computed
  // ones based on `accent_color`.
  SkColor GetAccentOrControlColorForState(
      std::optional<SkColor> accent_color,
      base::span<const ControlColorId, 4> colors,
      State state,
      bool dark_mode,
      PreferredContrast contrast,
      const ColorProvider* color_provider) const;

  // Adjusts custom scrollbar button/thumb colors to meet contrast minima. When
  // `state` is hovered or pressed, `color` (if present) will be adjusted to
  // contrast with the normal state. If `bg_color` is present, also attempts to
  // ensure `color` maintains visible contrast with it.
  std::optional<SkColor> GetContrastingColorForScrollbarPart(
      std::optional<SkColor> color,
      std::optional<SkColor> bg_color,
      State state) const;

  void PaintCheckbox(cc::PaintCanvas* canvas,
                     const ColorProvider* color_provider,
                     State state,
                     const gfx::Rect& rect,
                     const ButtonExtraParams& extra_params,
                     bool dark_mode,
                     PreferredContrast contrast,
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
                     bool dark_mode,
                     PreferredContrast contrast) const;

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
                   bool dark_mode,
                   PreferredContrast contrast) const;

  void PaintRadio(cc::PaintCanvas* canvas,
                  const ColorProvider* color_provider,
                  State state,
                  const gfx::Rect& rect,
                  const ButtonExtraParams& extra_params,
                  bool dark_mode,
                  PreferredContrast contrast,
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
                        PreferredContrast contrast,
                        std::optional<SkColor> accent_color) const;

  void PaintTextField(cc::PaintCanvas* canvas,
                      const ColorProvider* color_provider,
                      State state,
                      const gfx::Rect& rect,
                      const TextFieldExtraParams& extra_params,
                      bool dark_mode,
                      PreferredContrast contrast) const;

  // Draws the common elements of checkboxes and radio buttons. Returns the
  // rectangle within which any additional decorations should be drawn, or empty
  // if none.
  SkRect PaintCheckboxRadioCommon(cc::PaintCanvas* canvas,
                                  const ColorProvider* color_provider,
                                  State state,
                                  const gfx::Rect& rect,
                                  const ButtonExtraParams& extra_params,
                                  bool is_checkbox,
                                  float border_radius,
                                  bool dark_mode,
                                  PreferredContrast contrast,
                                  std::optional<SkColor> accent_color) const;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_BASE_H_
