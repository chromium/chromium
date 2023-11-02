// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_
#define UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_

#include "ui/native_theme/native_theme_base.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class NATIVE_THEME_EXPORT NativeThemeFluent : public NativeThemeBase {
 public:
  explicit NativeThemeFluent(bool should_only_use_dark_colors);

  NativeThemeFluent(const NativeThemeFluent&) = delete;
  NativeThemeFluent& operator=(const NativeThemeFluent&) = delete;

  ~NativeThemeFluent() override;

  static NativeThemeFluent* web_instance();

  void PaintArrowButton(cc::PaintCanvas* canvas,
                        const gfx::Rect& rect,
                        Part direction,
                        State state,
                        ColorScheme color_scheme,
                        const ScrollbarArrowExtraParams& arrow) const override;
  void PaintScrollbarTrack(cc::PaintCanvas* canvas,
                           Part part,
                           State state,
                           const ScrollbarTrackExtraParams& extra_params,
                           const gfx::Rect& rect,
                           ColorScheme color_scheme) const override;
  void PaintScrollbarThumb(cc::PaintCanvas* canvas,
                           const ColorProvider* color_provider,
                           Part part,
                           State state,
                           const gfx::Rect& rect,
                           ScrollbarOverlayColorTheme theme,
                           ColorScheme color_scheme) const override;
  void PaintScrollbarCorner(cc::PaintCanvas* canvas,
                            State state,
                            const gfx::Rect& rect,
                            ColorScheme color_scheme) const override;
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NativeThemeFluentTest, VerticalArrowRectDefault);
  FRIEND_TEST_ALL_PREFIXES(NativeThemeFluentTest, HorizontalArrowRectDefault);

  void PaintButton(cc::PaintCanvas* canvas,
                   const gfx::Rect& rect,
                   ColorScheme color_scheme) const;
  void PaintArrow(cc::PaintCanvas* canvas,
                  const gfx::Rect& rect,
                  Part part,
                  State state,
                  ColorScheme color_scheme) const;

  // Calculates and returns the position and dimensions of the scaled arrow rect
  // within the scrollbar button rect. The goal is to keep the arrow in the
  // center of the button with the applied kFluentScrollbarArrowOffset. See
  // OffsetArrowRect method for more details.
  gfx::Rect GetArrowRect(const gfx::Rect& rect, Part part, State state) const;

  // By Fluent design, arrow rect is offset from the center to the side opposite
  // from the track rect border by kFluentScrollbarArrowOffset px.
  void OffsetArrowRect(gfx::Rect& arrow_rect,
                       Part part,
                       int max_arrow_rect_side) const;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_
