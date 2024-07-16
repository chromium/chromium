// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/native_theme_gtk.h"

#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "cc/paint/paint_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gtk/gtk_color_mixers.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_utils.h"

using base::StrCat;

namespace gtk {

namespace {

enum BackgroundRenderMode {
  BG_RENDER_NORMAL,
  BG_RENDER_NONE,
  BG_RENDER_RECURSIVE,
};

SkBitmap GetWidgetBitmap(const gfx::Size& size,
                         GtkCssContext context,
                         BackgroundRenderMode bg_mode,
                         bool render_frame) {
  DCHECK(bg_mode != BG_RENDER_NONE || render_frame);
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width(), size.height());
  bitmap.eraseColor(0);

  CairoSurface surface(bitmap);
  cairo_t* cr = surface.cairo();

  double opacity = GetOpacityFromContext(context);
  if (opacity < 1)
    cairo_push_group(cr);

  switch (bg_mode) {
    case BG_RENDER_NORMAL:
      gtk_render_background(context, cr, 0, 0, size.width(), size.height());
      break;
    case BG_RENDER_RECURSIVE:
      RenderBackground(size, cr, context);
      break;
    case BG_RENDER_NONE:
      break;
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
                 BackgroundRenderMode bg_mode,
                 bool render_frame) {
  canvas->drawImage(cc::PaintImage::CreateFromBitmap(GetWidgetBitmap(
                        rect.size(), context, bg_mode, render_frame)),
                    rect.x(), rect.y());
}

}  // namespace

// static
NativeThemeGtk* NativeThemeGtk::instance() {
  static base::NoDestructor<NativeThemeGtk> s_native_theme;
  return s_native_theme.get();
}

NativeThemeGtk::NativeThemeGtk()
    : NativeThemeBase(/*should_only_use_dark_colors=*/false,
                      ui::SystemTheme::kGtk) {
  OnThemeChanged(gtk_settings_get_default(), nullptr);
}

NativeThemeGtk::~NativeThemeGtk() {
  NOTREACHED_IN_MIGRATION();
}

void NativeThemeGtk::SetThemeCssOverride(ScopedCssProvider provider) {
  if (theme_css_override_) {
    if (GtkCheckVersion(4)) {
      gtk_style_context_remove_provider_for_display(
          gdk_display_get_default(),
          GTK_STYLE_PROVIDER(theme_css_override_.get()));
    } else {
      gtk_style_context_remove_provider_for_screen(
          gdk_screen_get_default(),
          GTK_STYLE_PROVIDER(theme_css_override_.get()));
    }
  }
  theme_css_override_ = std::move(provider);
  if (theme_css_override_) {
    if (GtkCheckVersion(4)) {
      gtk_style_context_add_provider_for_display(
          gdk_display_get_default(),
          GTK_STYLE_PROVIDER(theme_css_override_.get()),
          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    } else {
      gtk_style_context_add_provider_for_screen(
          gdk_screen_get_default(),
          GTK_STYLE_PROVIDER(theme_css_override_.get()),
          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
  }
}

void NativeThemeGtk::NotifyOnNativeThemeUpdated() {
  // NativeThemeGtk pulls information about contrast from NativeThemeAura. As
  // such, Aura must be updated with this information before we call
  // NotifyOnNativeThemeUpdated().
  if (auto* native_theme_aura = ui::NativeTheme::GetInstanceForNativeUi();
      native_theme_aura->UpdateContrastRelatedStates(*this)) {
    native_theme_aura->NotifyOnNativeThemeUpdated();
  }

  NativeTheme::NotifyOnNativeThemeUpdated();
}

void NativeThemeGtk::OnThemeChanged(GtkSettings* settings,
                                    GtkParamSpec* param) {
  SetThemeCssOverride(ScopedCssProvider());

  std::string theme_name =
      GetGtkSettingsStringProperty(settings, "gtk-theme-name");

  // GTK has a dark mode setting called "gtk-application-prefer-dark-theme", but
  // this is really only used for themes that have a dark or light variant that
  // gets toggled based on this setting (eg. Adwaita).  Most dark themes do not
  // have a light variant and aren't affected by the setting.  Because of this,
  // experimentally check if the theme is dark by checking if the window
  // background color is dark.
  const SkColor window_bg_color = GetBgColor("");
  set_use_dark_colors(IsForcedDarkMode() ||
                      color_utils::IsDark(window_bg_color));
  set_preferred_color_scheme(CalculatePreferredColorScheme());

  // GTK doesn't have a native high contrast setting.  Rather, it's implied by
  // the theme name.  The only high contrast GTK themes that I know of are
  // HighContrast (GNOME) and ContrastHighInverse (MATE).  So infer the contrast
  // based on if the theme name contains both "high" and "contrast",
  // case-insensitive.
  base::ranges::transform(theme_name, theme_name.begin(), ::tolower);
  bool high_contrast = theme_name.find("high") != std::string::npos &&
                       theme_name.find("contrast") != std::string::npos;
  SetPreferredContrast(
      high_contrast ? ui::NativeThemeBase::PreferredContrast::kMore
                    : ui::NativeThemeBase::PreferredContrast::kNoPreference);

  NotifyOnNativeThemeUpdated();
}

void NativeThemeGtk::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const ui::ColorProvider* color_provider,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  auto context = GetStyleContextFromCss(GtkCssMenu());
  // Chrome menus aren't rendered with transparency, so avoid rounded corners.
  ApplyCssToContext(context, "* { border-radius: 0px; }");
  PaintWidget(canvas, gfx::Rect(size), context, BG_RENDER_RECURSIVE, false);
}

void NativeThemeGtk::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    const ui::ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item,
    ColorScheme color_scheme) const {
  auto context =
      GetStyleContextFromCss(StrCat({GtkCssMenu(), " ", GtkCssMenuItem()}));
  gtk_style_context_set_state(context, StateToStateFlags(state));
  PaintWidget(canvas, rect, context, BG_RENDER_NORMAL, true);
}

void NativeThemeGtk::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    const ui::ColorProvider* color_provider,
    State state,
    const gfx::Rect& rect,
    const MenuSeparatorExtraParams& menu_separator) const {
  // TODO(estade): use GTK to draw vertical separators too. See
  // crbug.com/710183
  if (menu_separator.type == ui::VERTICAL_SEPARATOR) {
    cc::PaintFlags paint;
    paint.setStyle(cc::PaintFlags::kFill_Style);
    DCHECK(color_provider);
    paint.setColor(color_provider->GetColor(ui::kColorMenuSeparator));
    canvas->drawRect(gfx::RectToSkRect(rect), paint);
    return;
  }

  auto separator_offset = [&](int separator_thickness) {
    switch (menu_separator.type) {
      case ui::LOWER_SEPARATOR:
        return rect.height() - separator_thickness;
      case ui::UPPER_SEPARATOR:
        return 0;
      default:
        return (rect.height() - separator_thickness) / 2;
    }
  };
  auto context =
      GetStyleContextFromCss(StrCat({GtkCssMenu(), " separator.horizontal"}));
  int min_height = 1;
  auto margin = GtkStyleContextGetMargin(context);
  auto border = GtkStyleContextGetBorder(context);
  auto padding = GtkStyleContextGetPadding(context);
  if (GtkCheckVersion(4)) {
    min_height = GetSeparatorSize(true).height();
  } else {
    GtkStyleContextGet(context, "min-height", &min_height, nullptr);
  }
  int w = rect.width() - margin.left() - margin.right();
  int h = std::max(min_height + padding.top() + padding.bottom() +
                       border.top() + border.bottom(),
                   1);
  int x = margin.left();
  int y = separator_offset(h);
  PaintWidget(canvas, gfx::Rect(x, y, w, h), context, BG_RENDER_NORMAL, true);
}

void NativeThemeGtk::PaintFrameTopArea(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const FrameTopAreaExtraParams& frame_top_area,
    ColorScheme color_scheme) const {
  auto context = GetStyleContextFromCss(frame_top_area.use_custom_frame
                                            ? "headerbar.header-bar.titlebar"
                                            : "menubar");
  ApplyCssToContext(context, "* { border-radius: 0px; border-style: none; }");
  gtk_style_context_set_state(context, frame_top_area.is_active
                                           ? GTK_STATE_FLAG_NORMAL
                                           : GTK_STATE_FLAG_BACKDROP);

  SkBitmap bitmap = GetWidgetBitmap(
      rect.size(), context,
      frame_top_area.use_custom_frame ? BG_RENDER_NORMAL : BG_RENDER_RECURSIVE,
      false);
  bitmap.setImmutable();
  canvas->drawImage(cc::PaintImage::CreateFromBitmap(std::move(bitmap)),
                    rect.x(), rect.y());
}

}  // namespace gtk
