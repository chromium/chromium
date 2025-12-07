// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/native_theme_gtk.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/system_theme.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_base.h"

namespace gtk {

namespace {

SkBitmap GetWidgetBitmap(const gfx::Size& size,
                         GtkCssContext context,
                         bool use_recursive_rendering,
                         bool render_frame) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  bitmap.eraseColor(0);

  CairoSurface surface(bitmap);
  cairo_t* const cr = surface.cairo();

  const double opacity = GetOpacityFromContext(context);
  if (opacity < 1) {
    cairo_push_group(cr);
  }

  if (use_recursive_rendering) {
    RenderBackground(size, cr, context);
  } else {
    gtk_render_background(context, cr, 0, 0, size.width(), size.height());
  }
  if (render_frame) {
    gtk_render_frame(context, cr, 0, 0, size.width(), size.height());
  }

  if (opacity < 1) {
    cairo_pop_group_to_source(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_paint_with_alpha(cr, opacity);
  }

  bitmap.setImmutable();
  return bitmap;
}

void PaintWidget(cc::PaintCanvas* canvas,
                 const gfx::Rect& rect,
                 GtkCssContext context,
                 bool use_recursive_rendering,
                 bool render_frame) {
  canvas->drawImage(
      cc::PaintImage::CreateFromBitmap(GetWidgetBitmap(
          rect.size(), context, use_recursive_rendering, render_frame)),
      rect.x(), rect.y());
}

}  // namespace

// static
NativeThemeGtk* NativeThemeGtk::instance() {
  static base::NoDestructor<NativeThemeGtk> s_native_theme;
  return s_native_theme.get();
}

void NativeThemeGtk::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const ui::ColorProvider* color_provider,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& extra_params) const {
  GtkCssContext context = GetStyleContextFromCss(GtkCssMenu());

  // Chrome menus aren't rendered with transparency, so avoid rounded corners.
  ApplyCssToContext(context, "* { border-radius: 0px; }");

  PaintWidget(canvas, gfx::Rect(size), std::move(context), true, false);
}

void NativeThemeGtk::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    const ui::ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuSeparatorExtraParams& extra_params) const {
  // TODO(crbug.com/41312291): Use GTK to draw vertical separators too.
  if (extra_params.type == ui::VERTICAL_SEPARATOR) {
    CHECK(color_provider);
    cc::PaintFlags paint;
    paint.setColor(color_provider->GetColor(extra_params.color_id));
    canvas->drawRect(gfx::RectToSkRect(rect), paint);
    return;
  }

  GtkCssContext context = GetStyleContextFromCss(
      base::StrCat({GtkCssMenu(), " separator.horizontal"}));
  const gfx::Insets margin = GtkStyleContextGetMargin(context);
  const gfx::Insets border = GtkStyleContextGetBorder(context);
  const gfx::Insets padding = GtkStyleContextGetPadding(context);
  int min_height;
  if (GtkCheckVersion(4)) {
    min_height = GetSeparatorSize(true).height();
  } else {
    GtkStyleContextGet(context, "min-height", &min_height, nullptr);
  }

  const auto separator_offset = [&](int separator_thickness) {
    if (extra_params.type == ui::UPPER_SEPARATOR) {
      return 0;
    }
    const int offset = rect.height() - separator_thickness;
    return (extra_params.type == ui::LOWER_SEPARATOR) ? offset : (offset / 2);
  };

  const int w = rect.width() - margin.width();
  const int h = std::max(min_height + padding.height() + border.height(), 1);
  const int x = margin.left();
  const int y = separator_offset(h);
  PaintWidget(canvas, gfx::Rect(x, y, w, h), std::move(context), false, true);
}

void NativeThemeGtk::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    const ui::ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& extra_params) const {
  GtkCssContext context = GetStyleContextFromCss(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem()}));
  gtk_style_context_set_state(context, StateToStateFlags(state));
  PaintWidget(canvas, rect, std::move(context), false, true);
}

void NativeThemeGtk::PaintFrameTopArea(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const FrameTopAreaExtraParams& extra_params) const {
  GtkCssContext context = GetStyleContextFromCss(
      extra_params.use_custom_frame ? "headerbar.header-bar.titlebar"
                                    : "menubar");
  ApplyCssToContext(context, "* { border-radius: 0px; border-style: none; }");
  gtk_style_context_set_state(context, extra_params.is_active
                                           ? GTK_STATE_FLAG_NORMAL
                                           : GTK_STATE_FLAG_BACKDROP);

  SkBitmap bitmap = GetWidgetBitmap(rect.size(), std::move(context),
                                    !extra_params.use_custom_frame, false);
  bitmap.setImmutable();
  canvas->drawImage(cc::PaintImage::CreateFromBitmap(std::move(bitmap)),
                    rect.x(), rect.y());
}

NativeThemeGtk::NativeThemeGtk() : NativeThemeBase(ui::SystemTheme::kGtk) {
  BeginObservingOsSettingChanges();
}

NativeThemeGtk::~NativeThemeGtk() = default;

}  // namespace gtk
