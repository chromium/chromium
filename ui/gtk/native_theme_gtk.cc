// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/native_theme_gtk.h"

#include "base/no_destructor.h"
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
  // g_type_from_name() is only used in GTK3.
  if (!GtkCheckVersion(4)) {
    // These types are needed by g_type_from_name(), but may not be registered
    // at this point.  We need the g_type_class magic to make sure the compiler
    // doesn't optimize away this code.
    g_type_class_unref(g_type_class_ref(gtk_button_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_entry_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_frame_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_header_bar_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_image_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_info_bar_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_label_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_menu_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_menu_bar_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_menu_item_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_range_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_scrollbar_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_scrolled_window_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_separator_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_spinner_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_text_view_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_toggle_button_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_tree_view_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_window_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_combo_box_text_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_cell_view_get_type()));
    g_type_class_unref(g_type_class_ref(gtk_scale_get_type()));

    // Initialize the GtkTreeMenu type.  _gtk_tree_menu_get_type() is private,
    // so we need to initialize it indirectly.
    auto model = TakeGObject(GTK_TREE_MODEL(GtkTreeStoreNew(G_TYPE_STRING)));
    auto combo = TakeGObject(gtk_combo_box_new_with_model(model));
  }

  ui::ColorProviderManager::Get().AppendColorProviderInitializer(
      base::BindRepeating(AddGtkNativeColorMixer));

  OnThemeChanged(gtk_settings_get_default(), nullptr);
}

NativeThemeGtk::~NativeThemeGtk() {
  NOTREACHED();
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
  NativeTheme::NotifyOnNativeThemeUpdated();

  // Update the preferred contrast settings for the NativeThemeAura instance and
  // notify its observers about the change.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  native_theme->SetPreferredContrast(
      UserHasContrastPreference()
          ? ui::NativeThemeBase::PreferredContrast::kMore
          : ui::NativeThemeBase::PreferredContrast::kNoPreference);
  native_theme->NotifyOnNativeThemeUpdated();
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
  std::transform(theme_name.begin(), theme_name.end(), theme_name.begin(),
                 ::tolower);
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
  auto context = GetStyleContextFromCss(
      StrCat({GtkCssMenu(), " GtkSeparator#separator.horizontal"}));
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
                                            ? "#headerbar.header-bar.titlebar"
                                            : "GtkMenuBar#menubar");
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
