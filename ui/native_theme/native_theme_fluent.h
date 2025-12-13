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
}

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
  int GetPaintedScrollbarTrackInset() const override;

  // Gets/sets whether arrow icons are treated as available for metric
  // computations.
  bool GetArrowIconsAvailable() const;
  void SetArrowIconsAvailableForTesting(bool available);

 protected:
  NativeThemeFluent();
  ~NativeThemeFluent() override;

  // NativeThemeBase:
  gfx::Size GetVerticalScrollbarButtonSize() const override;
  gfx::Size GetVerticalScrollbarThumbSize() const override;
  gfx::RectF GetArrowRect(const gfx::Rect& rect,
                          Part part,
                          State state) const override;
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

  // Returns the typeface to use for arrow icons. May return null if the
  // typeface is not available. Lazily loads the typeface on first call.
  sk_sp<SkTypeface> GetArrowIconTypeface() const;

  // The typeface which contains arrow icons. Because `GetArrowIconTypeface()`
  // lazily loads, a null optional means "no load attempted" while a null
  // pointer inside the optional means "load failed and will not be retried".
  mutable std::optional<sk_sp<SkTypeface>> typeface_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_
