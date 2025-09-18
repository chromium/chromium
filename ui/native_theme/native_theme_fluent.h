// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_
#define UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_

#include <optional>

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_base.h"

class SkTypeface;

namespace cc {
class PaintCanvas;
class PaintFlags;
}  // namespace cc

namespace gfx {
class Rect;
}

namespace ui {

class ColorProvider;
class NativeThemeFluentTest;

class COMPONENT_EXPORT(NATIVE_THEME) NativeThemeFluent
    : public NativeThemeBase {
 public:
  // LINT.IfChange(FluentScrollbarThickness)
  static constexpr int kScrollbarThickness = 15;
  // LINT.ThenChange(//third_party/blink/web_tests/resources/scrollbar-util.js:FluentScrollbarThickness)

  // Button height for the vertical scrollbar or width for the horizontal.
  static constexpr int kScrollbarButtonSideLength = 18;

  NativeThemeFluent(const NativeThemeFluent&) = delete;
  NativeThemeFluent& operator=(const NativeThemeFluent&) = delete;

  // NativeThemeBase:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra_params) const override;
  int GetPaintedScrollbarTrackInset() const override;
  gfx::Insets GetScrollbarSolidColorThumbInsets(Part part) const override;

  // Gets whether arrow icons are treated as available for metric computations.
  bool ArrowIconsAvailable() const {
    return typeface_.has_value() && typeface_.value().get();
  }

 protected:
  NativeThemeFluent();
  ~NativeThemeFluent() override;

  // NativeThemeBase:
  std::optional<ColorId> GetScrollbarThumbColorId(
      State state,
      const ScrollbarThumbExtraParams& extra_params) const override;
  float GetScrollbarPartContrastRatioForState(State state) const override;
  void PaintArrowButton(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      const gfx::Rect& rect,
      Part part,
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
  friend class base::NoDestructor<NativeThemeFluent>;
  friend class NativeThemeFluentTest;

  // Calculates and returns the position and dimensions of the scaled arrow rect
  // within the scrollbar button rect. The goal is to keep the arrow in the
  // center of the button with the applied kFluentScrollbarArrowOffset.
  gfx::RectF GetArrowRect(const gfx::Rect& rect, Part part, State state) const;

  // Used by Overlay Fluent scrollbars to paint buttons with rounded corners.
  void PaintRoundedButton(cc::PaintCanvas* canvas,
                          const gfx::RectF& paint_rect,
                          cc::PaintFlags paint_flags,
                          Part part) const;

  // The typeface which contains arrow icons. Because `PaintArrow()` lazily
  // loads, a null optional means "no load attempted" while a null pointer
  // inside the optional means "load failed and will not be retried".
  mutable std::optional<sk_sp<SkTypeface>> typeface_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_
