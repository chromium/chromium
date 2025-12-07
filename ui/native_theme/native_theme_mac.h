// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
#define UI_NATIVE_THEME_NATIVE_THEME_MAC_H_

#include "base/component_export.h"
#include "base/no_destructor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

class COMPONENT_EXPORT(NATIVE_THEME) NativeThemeMac : public NativeThemeBase {
 public:
  NativeThemeMac(const NativeThemeMac&) = delete;
  NativeThemeMac& operator=(const NativeThemeMac&) = delete;

  // The minimum size in px for the thumb, given device scale factor `scale`.
  // Exposed publicly for testing.
  static gfx::Size GetThumbMinSize(bool horizontal, float scale);

  // NativeThemeBase:
  SkColor GetSystemButtonPressedColor(SkColor base_color) const override;
  void PaintMenuItemBackground(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const MenuItemExtraParams& extra_params) const override;

 protected:
  NativeThemeMac();
  ~NativeThemeMac() override;

  // NativeThemeBase:
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
  void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const ColorProvider* color_provider,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& extra_params) const override;

 private:
  friend class base::NoDestructor<NativeThemeMac>;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_MAC_H_
