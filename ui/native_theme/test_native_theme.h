// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_TEST_NATIVE_THEME_H_
#define UI_NATIVE_THEME_TEST_NATIVE_THEME_H_

#include "base/component_export.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) TestNativeTheme : public NativeTheme {
 public:
  TestNativeTheme();

  TestNativeTheme(const TestNativeTheme&) = delete;
  TestNativeTheme& operator=(const TestNativeTheme&) = delete;

  ~TestNativeTheme() override;

  // NativeTheme:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra_params) const override;
  void Paint(cc::PaintCanvas* canvas,
             const ui::ColorProvider* color_provider,
             Part part,
             State state,
             const gfx::Rect& rect,
             const ExtraParams& extra_params,
             bool forced_colors,
             PreferredColorScheme color_scheme,
             PreferredContrast contrast,
             std::optional<SkColor> accent_color) const override;
  bool SupportsNinePatch(Part part) const override;
  gfx::Size GetNinePatchCanvasSize(Part part) const override;
  gfx::Rect GetNinePatchAperture(Part part) const override;

  void SetPreferredColorScheme(PreferredColorScheme color_scheme);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_TEST_NATIVE_THEME_H_
