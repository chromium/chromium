// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_NATIVE_THEME_GTK_H_
#define UI_GTK_NATIVE_THEME_GTK_H_

#include "base/no_destructor.h"
#include "ui/native_theme/native_theme_base.h"

namespace gtk {

class NativeThemeGtk : public ui::NativeThemeBase {
 public:
  static NativeThemeGtk* instance();

  NativeThemeGtk(const NativeThemeGtk&) = delete;
  NativeThemeGtk& operator=(const NativeThemeGtk&) = delete;

  // ui::NativeThemeBase:
  void PaintMenuPopupBackground(
      cc::PaintCanvas* canvas,
      const ui::ColorProvider* color_provider,
      const gfx::Size& size,
      const MenuBackgroundExtraParams& extra_params) const override;
  void PaintMenuSeparator(
      cc::PaintCanvas* canvas,
      const ui::ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const MenuSeparatorExtraParams& extra_params) const override;
  void PaintMenuItemBackground(
      cc::PaintCanvas* canvas,
      const ui::ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const MenuItemExtraParams& extra_params) const override;
  void PaintFrameTopArea(
      cc::PaintCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const FrameTopAreaExtraParams& extra_params) const override;

 private:
  friend class base::NoDestructor<NativeThemeGtk>;

  NativeThemeGtk();
  ~NativeThemeGtk() override;
};

}  // namespace gtk

#endif  // UI_GTK_NATIVE_THEME_GTK_H_
