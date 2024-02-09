// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GTK_NATIVE_THEME_GTK_H_
#define UI_GTK_NATIVE_THEME_GTK_H_

#include <optional>

#include "base/callback_list.h"
#include "base/no_destructor.h"
#include "ui/base/glib/scoped_gobject.h"
#include "ui/native_theme/native_theme_base.h"

typedef struct _GtkCssProvider GtkCssProvider;
typedef struct _GtkParamSpec GtkParamSpec;
typedef struct _GtkSettings GtkSettings;

namespace gtk {

using ScopedCssProvider = ScopedGObject<GtkCssProvider>;

// A version of NativeTheme that uses GTK-rendered widgets.
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
      const MenuBackgroundExtraParams& menu_background,
      ColorScheme color_scheme) const override;
  void PaintMenuSeparator(
      cc::PaintCanvas* canvas,
      const ui::ColorProvider* color_provider,
      State state,
      const gfx::Rect& rect,
      const MenuSeparatorExtraParams& menu_separator) const override;
  void PaintMenuItemBackground(cc::PaintCanvas* canvas,
                               const ui::ColorProvider* color_provider,
                               State state,
                               const gfx::Rect& rect,
                               const MenuItemExtraParams& menu_item,
                               ColorScheme color_scheme) const override;
  void PaintFrameTopArea(cc::PaintCanvas* canvas,
                         State state,
                         const gfx::Rect& rect,
                         const FrameTopAreaExtraParams& frame_top_area,
                         ColorScheme color_scheme) const override;
  void NotifyOnNativeThemeUpdated() override;

  void OnThemeChanged(GtkSettings* settings, GtkParamSpec* param);

 private:
  friend class base::NoDestructor<NativeThemeGtk>;

  NativeThemeGtk();
  ~NativeThemeGtk() override;

  void SetThemeCssOverride(ScopedCssProvider provider);

  ScopedCssProvider theme_css_override_;
};

}  // namespace gtk

#endif  // UI_GTK_NATIVE_THEME_GTK_H_
