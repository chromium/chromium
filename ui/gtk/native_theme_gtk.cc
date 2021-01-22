// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/native_theme_gtk.h"

#include <gtk/gtk.h>

#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/gtk/gtk_util.h"
#include "ui/native_theme/native_theme_aura.h"

namespace gtk {

namespace {

enum BackgroundRenderMode {
  BG_RENDER_NORMAL,
  BG_RENDER_NONE,
  BG_RENDER_RECURSIVE,
};

ScopedStyleContext GetTooltipContext() {
  return AppendCssNodeToStyleContext(
      nullptr, GtkCheckVersion(3, 20) ? "#tooltip.background"
                                      : "GtkWindow#window.background.tooltip");
}

SkBitmap GetWidgetBitmap(const gfx::Size& size,
                         GtkStyleContext* context,
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
  if (render_frame)
    gtk_render_frame(context, cr, 0, 0, size.width(), size.height());
  bitmap.setImmutable();
  return bitmap;
}

void PaintWidget(cc::PaintCanvas* canvas,
                 const gfx::Rect& rect,
                 GtkStyleContext* context,
                 BackgroundRenderMode bg_mode,
                 bool render_frame) {
  canvas->drawImage(cc::PaintImage::CreateFromBitmap(GetWidgetBitmap(
                        rect.size(), context, bg_mode, render_frame)),
                    rect.x(), rect.y());
}

base::Optional<SkColor> SkColorFromColorId(
    ui::NativeTheme::ColorId color_id,
    const ui::NativeTheme* base_theme,
    ui::NativeTheme::ColorScheme color_scheme) {
  switch (color_id) {
    // Windows
    case ui::NativeTheme::kColorId_WindowBackground:
    // Dialogs
    case ui::NativeTheme::kColorId_DialogBackground:
    case ui::NativeTheme::kColorId_BubbleBackground:
    // Notifications
    case ui::NativeTheme::kColorId_NotificationDefaultBackground:
      return GetBgColor("");
    case ui::NativeTheme::kColorId_DialogForeground:
    case ui::NativeTheme::kColorId_AvatarIconIncognito:
      return GetFgColor("GtkLabel");
    case ui::NativeTheme::kColorId_BubbleFooterBackground:
      return GetBgColor("#statusbar");

    // FocusableBorder
    case ui::NativeTheme::kColorId_FocusedBorderColor:
      // GetBorderColor("GtkEntry#entry:focus") is correct here.  The focus ring
      // around widgets is usually a lighter version of the "canonical theme
      // color" - orange on Ambiance, blue on Adwaita, etc.  However, Chrome
      // lightens the color we give it, so it would look wrong if we give it an
      // already-lightened color.  This workaround returns the theme color
      // directly, taken from a selected table row.  This has matched the theme
      // color on every theme that I've tested.
      return GetBgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeView#treeview.view.cell:selected:focus");
    case ui::NativeTheme::kColorId_UnfocusedBorderColor:
      return GetBorderColor("GtkEntry#entry");

    // Menu
    case ui::NativeTheme::kColorId_MenuBackgroundColor:
    case ui::NativeTheme::kColorId_HighlightedMenuItemBackgroundColor:
    case ui::NativeTheme::kColorId_MenuItemInitialAlertBackgroundColor:
    case ui::NativeTheme::kColorId_MenuItemTargetAlertBackgroundColor:
      return GetBgColor("GtkMenu#menu");
    case ui::NativeTheme::kColorId_MenuBorderColor:
      return GetBorderColor("GtkMenu#menu");
    case ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor:
      return GetBgColor("GtkMenu#menu GtkMenuItem#menuitem:hover");
    case ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor:
    case ui::NativeTheme::kColorId_MenuDropIndicator:
    case ui::NativeTheme::kColorId_HighlightedMenuItemForegroundColor:
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem GtkLabel");
    case ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor:
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem:hover GtkLabel");
    case ui::NativeTheme::kColorId_DisabledMenuItemForegroundColor:
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem:disabled GtkLabel");
    case ui::NativeTheme::kColorId_AvatarIconGuest:
    case ui::NativeTheme::kColorId_MenuItemMinorTextColor:
      if (GtkCheckVersion(3, 20)) {
        return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem #accelerator");
      }
      return GetFgColor(
          "GtkMenu#menu GtkMenuItem#menuitem GtkLabel.accelerator");
    case ui::NativeTheme::kColorId_MenuSeparatorColor:
    case ui::NativeTheme::kColorId_AvatarHeaderArt:
      if (GtkCheckVersion(3, 20)) {
        return GetSeparatorColor(
            "GtkMenu#menu GtkSeparator#separator.horizontal");
      }
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem.separator");

    // Dropdown
    case ui::NativeTheme::kColorId_DropdownBackgroundColor:
      return GetBgColor(
          "GtkComboBoxText#combobox GtkWindow#window.background.popup "
          "GtkTreeMenu#menu(gtk-combobox-popup-menu) GtkMenuItem#menuitem "
          "GtkCellView#cellview");
    case ui::NativeTheme::kColorId_DropdownForegroundColor:
      return GetFgColor(
          "GtkComboBoxText#combobox GtkWindow#window.background.popup "
          "GtkTreeMenu#menu(gtk-combobox-popup-menu) GtkMenuItem#menuitem "
          "GtkCellView#cellview");
    case ui::NativeTheme::kColorId_DropdownSelectedBackgroundColor:
      return GetBgColor(
          "GtkComboBoxText#combobox GtkWindow#window.background.popup "
          "GtkTreeMenu#menu(gtk-combobox-popup-menu) "
          "GtkMenuItem#menuitem:hover GtkCellView#cellview");
    case ui::NativeTheme::kColorId_DropdownSelectedForegroundColor:
      return GetFgColor(
          "GtkComboBoxText#combobox GtkWindow#window.background.popup "
          "GtkTreeMenu#menu(gtk-combobox-popup-menu) "
          "GtkMenuItem#menuitem:hover GtkCellView#cellview");

    // Label
    case ui::NativeTheme::kColorId_LabelEnabledColor:
      return GetFgColor("GtkLabel");
    case ui::NativeTheme::kColorId_LabelDisabledColor:
    case ui::NativeTheme::kColorId_LabelSecondaryColor:
      return GetFgColor("GtkLabel:disabled");
    case ui::NativeTheme::kColorId_LabelTextSelectionColor:
      return GetFgColor(GtkCheckVersion(3, 20) ? "GtkLabel #selection"
                                               : "GtkLabel:selected");
    case ui::NativeTheme::kColorId_LabelTextSelectionBackgroundFocused:
      return GetSelectionBgColor(GtkCheckVersion(3, 20) ? "GtkLabel #selection"
                                                        : "GtkLabel:selected");

    // Link
    case ui::NativeTheme::kColorId_LinkDisabled:
      return SkColorSetA(
          base_theme->GetSystemColor(ui::NativeTheme::kColorId_LinkEnabled,
                                     color_scheme),
          0xBB);
    case ui::NativeTheme::kColorId_LinkPressed:
      if (GtkCheckVersion(3, 12))
        return GetFgColor("GtkLabel.link:link:hover:active");
      FALLTHROUGH;
    case ui::NativeTheme::kColorId_LinkEnabled: {
      if (GtkCheckVersion(3, 12))
        return GetFgColor("GtkLabel.link:link");
#if !GTK_CHECK_VERSION(3, 90, 0)
      auto link_context = GetStyleContextFromCss("GtkLabel.view");
      GdkColor* color;
      gtk_style_context_get_style(link_context, "link-color", &color, nullptr);
      if (color) {
        SkColor ret_color =
            SkColorSetRGB(color->red >> 8, color->green >> 8, color->blue >> 8);
        // gdk_color_free() was deprecated in Gtk3.14.  This code path is only
        // taken on versions earlier than Gtk3.12, but the compiler doesn't know
        // that, so silence the deprecation warnings.
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        gdk_color_free(color);
        G_GNUC_END_IGNORE_DEPRECATIONS;
        return ret_color;
      }
#endif
      // Default color comes from gtklinkbutton.c.
      return SkColorSetRGB(0x00, 0x00, 0xEE);
    }

    // Scrollbar
    case ui::NativeTheme::kColorId_OverlayScrollbarThumbBackground:
      return GetBgColor("#GtkScrollbar#scrollbar #trough");
    case ui::NativeTheme::kColorId_OverlayScrollbarThumbForeground:
      return GetBgColor("#GtkScrollbar#scrollbar #slider");

    // Slider
    case ui::NativeTheme::kColorId_SliderThumbDefault:
      return GetBgColor("GtkScale#scale #highlight");
    case ui::NativeTheme::kColorId_SliderTroughDefault:
      return GetBgColor("GtkScale#scale #trough");
    case ui::NativeTheme::kColorId_SliderThumbMinimal:
      return GetBgColor("GtkScale#scale:disabled #highlight");
    case ui::NativeTheme::kColorId_SliderTroughMinimal:
      return GetBgColor("GtkScale#scale:disabled #trough");

    // Separator
    case ui::NativeTheme::kColorId_SeparatorColor:
      return GetSeparatorColor("GtkSeparator#separator.horizontal");

    // Button
    case ui::NativeTheme::kColorId_ButtonColor:
      return GetBgColor("GtkButton#button");
    case ui::NativeTheme::kColorId_ButtonEnabledColor:
    case ui::NativeTheme::kColorId_ButtonUncheckedColor:
      return GetFgColor("GtkButton#button.text-button GtkLabel");
    case ui::NativeTheme::kColorId_ButtonDisabledColor:
      return GetFgColor("GtkButton#button.text-button:disabled GtkLabel");
    // TODO(thomasanderson): Add this once this CL lands:
    // https://chromium-review.googlesource.com/c/chromium/src/+/2053144
    // case ui::NativeTheme::kColorId_ButtonHoverColor:
    //   return GetBgColor("GtkButton#button:hover");

    // ProminentButton
    case ui::NativeTheme::kColorId_ButtonCheckedColor:
    case ui::NativeTheme::kColorId_ProminentButtonColor:
    case ui::NativeTheme::kColorId_ProminentButtonFocusedColor:
      return GetBgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeView#treeview.view.cell:selected:focus");
    case ui::NativeTheme::kColorId_TextOnProminentButtonColor:
      return GetFgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeView#treeview.view.cell:selected:focus GtkLabel");
    case ui::NativeTheme::kColorId_ProminentButtonDisabledColor:
      return GetBgColor("GtkButton#button.text-button:disabled");
    case ui::NativeTheme::kColorId_ButtonBorderColor:
      return GetBorderColor("GtkButton#button.text-button");
    // TODO(thomasanderson): Add this once this CL lands:
    // https://chromium-review.googlesource.com/c/chromium/src/+/2053144
    // case ui::NativeTheme::kColorId_ProminentButtonHoverColor:
    //   return GetBgColor(
    //       "GtkTreeView#treeview.view "
    //       "GtkTreeView#treeview.view.cell:selected:focus:hover");

    // ToggleButton
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOff:
      return GetBgColor("GtkButton#button.text-button.toggle");
    case ui::NativeTheme::kColorId_ToggleButtonTrackColorOn:
      return GetBgColor("GtkButton#button.text-button.toggle:checked");

    // TabbedPane
    case ui::NativeTheme::kColorId_TabTitleColorActive:
      return GetFgColor("GtkLabel");
    case ui::NativeTheme::kColorId_TabTitleColorInactive:
      return GetFgColor("GtkLabel:disabled");
    case ui::NativeTheme::kColorId_TabBottomBorder:
      return GetBorderColor(GtkCheckVersion(3, 20) ? "GtkFrame#frame #border"
                                                   : "GtkFrame#frame");
    case ui::NativeTheme::kColorId_TabHighlightBackground:
      return GetBgColor("GtkNotebook#notebook #tab:checked");
    case ui::NativeTheme::kColorId_TabHighlightFocusedBackground:
      return GetBgColor("GtkNotebook#notebook:focus #tab:checked");

    // Textfield
    case ui::NativeTheme::kColorId_TextfieldDefaultColor:
      return GetFgColor(GtkCheckVersion(3, 20)
                            ? "GtkTextView#textview.view #text"
                            : "GtkTextView.view");
    case ui::NativeTheme::kColorId_TextfieldDefaultBackground:
      return GetBgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view"
                                               : "GtkTextView.view");
    case ui::NativeTheme::kColorId_TextfieldPlaceholderColor:
      if (!GtkCheckVersion(3, 90)) {
        auto context = GetStyleContextFromCss("GtkEntry#entry");
        // This is copied from gtkentry.c.
        GdkRGBA fg = {0.5, 0.5, 0.5};
        gtk_style_context_lookup_color(context, "placeholder_text_color", &fg);
        return GdkRgbaToSkColor(fg);
      }
      return GetFgColor("GtkEntry#entry #text #placeholder");
    case ui::NativeTheme::kColorId_TextfieldReadOnlyColor:
      return GetFgColor(GtkCheckVersion(3, 20)
                            ? "GtkTextView#textview.view:disabled #text"
                            : "GtkTextView.view:disabled");
    case ui::NativeTheme::kColorId_TextfieldReadOnlyBackground:
      return GetBgColor(GtkCheckVersion(3, 20)
                            ? "GtkTextView#textview.view:disabled"
                            : "GtkTextView.view:disabled");
    case ui::NativeTheme::kColorId_TextfieldSelectionColor:
      return GetFgColor(GtkCheckVersion(3, 20)
                            ? "GtkTextView#textview.view #text #selection"
                            : "GtkTextView.view:selected");
    case ui::NativeTheme::kColorId_TextfieldSelectionBackgroundFocused:
      return GetSelectionBgColor(
          GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text #selection"
                                 : "GtkTextView.view:selected");

    // Tooltips
    case ui::NativeTheme::kColorId_TooltipBackground:
      return GetBgColorFromStyleContext(GetTooltipContext());
    case ui::NativeTheme::kColorId_TooltipIcon:
      return GetFgColor("GtkButton#button.image-button");
    case ui::NativeTheme::kColorId_TooltipIconHovered:
      return GetFgColor("GtkButton#button.image-button:hover");
    case ui::NativeTheme::kColorId_TooltipText: {
      auto context = GetTooltipContext();
      context = AppendCssNodeToStyleContext(context, "GtkLabel");
      return GetFgColorFromStyleContext(context);
    }

    // Trees and Tables (implemented on GTK using the same class)
    case ui::NativeTheme::kColorId_TableBackground:
    case ui::NativeTheme::kColorId_TableBackgroundAlternate:
    case ui::NativeTheme::kColorId_TreeBackground:
      return GetBgColor(
          "GtkTreeView#treeview.view GtkTreeView#treeview.view.cell");
    case ui::NativeTheme::kColorId_TableText:
    case ui::NativeTheme::kColorId_TreeText:
    case ui::NativeTheme::kColorId_TableGroupingIndicatorColor:
      return GetFgColor(
          "GtkTreeView#treeview.view GtkTreeView#treeview.view.cell GtkLabel");
    case ui::NativeTheme::kColorId_TableSelectedText:
    case ui::NativeTheme::kColorId_TableSelectedTextUnfocused:
    case ui::NativeTheme::kColorId_TreeSelectedText:
    case ui::NativeTheme::kColorId_TreeSelectedTextUnfocused:
      return GetFgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeView#treeview.view.cell:selected:focus GtkLabel");
    case ui::NativeTheme::kColorId_TableSelectionBackgroundFocused:
    case ui::NativeTheme::kColorId_TableSelectionBackgroundUnfocused:
    case ui::NativeTheme::kColorId_TreeSelectionBackgroundFocused:
    case ui::NativeTheme::kColorId_TreeSelectionBackgroundUnfocused:
      return GetBgColor(
          "GtkTreeView#treeview.view "
          "GtkTreeView#treeview.view.cell:selected:focus");

    // Table Header
    case ui::NativeTheme::kColorId_TableHeaderText:
      return GetFgColor("GtkTreeView#treeview.view GtkButton#button GtkLabel");
    case ui::NativeTheme::kColorId_TableHeaderBackground:
      return GetBgColor("GtkTreeView#treeview.view GtkButton#button");
    case ui::NativeTheme::kColorId_TableHeaderSeparator:
      return GetBorderColor("GtkTreeView#treeview.view GtkButton#button");

    // Throbber
    // TODO(thomasanderson): Render GtkSpinner directly.
    case ui::NativeTheme::kColorId_ThrobberSpinningColor:
      return GetFgColor("GtkSpinner#spinner");
    case ui::NativeTheme::kColorId_ThrobberWaitingColor:
    case ui::NativeTheme::kColorId_ThrobberLightColor:
      return GetFgColor("GtkSpinner#spinner:disabled");

    // Alert icons
    // Fallback to the same colors as Aura.
    case ui::NativeTheme::kColorId_AlertSeverityLow:
    case ui::NativeTheme::kColorId_AlertSeverityMedium:
    case ui::NativeTheme::kColorId_AlertSeverityHigh: {
      // Alert icons appear on the toolbar, so use the toolbar BG
      // color (the GTK window bg color) to determine if the dark
      // or light native theme should be used for the icons.
      ui::NativeTheme* fallback_theme =
          color_utils::IsDark(GetBgColor(""))
              ? ui::NativeTheme::GetInstanceForDarkUI()
              : ui::NativeTheme::GetInstanceForNativeUi();
      return fallback_theme->GetSystemColor(color_id);
    }

    case ui::NativeTheme::kColorId_MenuIconColor:
      if (GtkCheckVersion(3, 20))
        return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem #radio");
      return GetFgColor("GtkMenu#menu GtkMenuItem#menuitem.radio");

    case ui::NativeTheme::kColorId_DefaultIconColor:
      return GetFgColor("GtkButton#button.flat.scale GtkImage#image");

    case ui::NativeTheme::kColorId_NumColors:
      NOTREACHED();
      break;

    default:
      break;
  }
  return base::nullopt;
}

}  // namespace

// static
NativeThemeGtk* NativeThemeGtk::instance() {
  static base::NoDestructor<NativeThemeGtk> s_native_theme;
  return s_native_theme.get();
}

NativeThemeGtk::NativeThemeGtk() {
  // These types are needed by g_type_from_name(), but may not be registered at
  // this point.  We need the g_type_class magic to make sure the compiler
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

  // Initialize the GtkTreeMenu type.  _gtk_tree_menu_get_type() is private, so
  // we need to initialize it indirectly.
  ScopedGObject<GtkTreeModel> model{
      GTK_TREE_MODEL(gtk_tree_store_new(1, G_TYPE_STRING))};
  ScopedGObject<GtkWidget> combo{gtk_combo_box_new_with_model(model)};

  OnThemeChanged(gtk_settings_get_default(), nullptr);
}

NativeThemeGtk::~NativeThemeGtk() {
  NOTREACHED();
}

void NativeThemeGtk::SetThemeCssOverride(ScopedCssProvider provider) {
  if (theme_css_override_) {
    gtk_style_context_remove_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(theme_css_override_.get()));
  }
  theme_css_override_ = std::move(provider);
  if (theme_css_override_) {
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(), GTK_STYLE_PROVIDER(theme_css_override_.get()),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
}

void NativeThemeGtk::NotifyObservers() {
  NativeTheme::NotifyObservers();

  // Update the preferred contrast settings for the NativeThemeAura instance and
  // notify its observers about the change.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  native_theme->set_preferred_contrast(
      UserHasContrastPreference()
          ? ui::NativeThemeBase::PreferredContrast::kMore
          : ui::NativeThemeBase::PreferredContrast::kNoPreference);
  native_theme->NotifyObservers();
}

void NativeThemeGtk::OnThemeChanged(GtkSettings* settings,
                                    GtkParamSpec* param) {
  SetThemeCssOverride(ScopedCssProvider());
  for (auto& color : color_cache_)
    color = base::nullopt;

  // Hack to workaround a bug on GNOME standard themes which would
  // cause black patches to be rendered on GtkFileChooser dialogs.
  std::string theme_name =
      GetGtkSettingsStringProperty(settings, "gtk-theme-name");
  if (!GtkCheckVersion(3, 14)) {
    if (theme_name == "Adwaita") {
      SetThemeCssOverride(GetCssProvider(
          "GtkFileChooser GtkPaned { background-color: @theme_bg_color; }"));
    } else if (theme_name == "HighContrast") {
      SetThemeCssOverride(GetCssProvider(
          "GtkFileChooser GtkPaned { background-color: @theme_base_color; }"));
    }
  }

  // GTK has a dark mode setting called "gtk-application-prefer-dark-theme", but
  // this is really only used for themes that have a dark or light variant that
  // gets toggled based on this setting (eg. Adwaita).  Most dark themes do not
  // have a light variant and aren't affected by the setting.  Because of this,
  // experimentally check if the theme is dark by checking if the window
  // background color is dark.
  set_use_dark_colors(
      color_utils::IsDark(GetSystemColor(kColorId_WindowBackground)));
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
  set_preferred_contrast(
      high_contrast ? ui::NativeThemeBase::PreferredContrast::kMore
                    : ui::NativeThemeBase::PreferredContrast::kNoPreference);

  NotifyObservers();
}

SkColor NativeThemeGtk::GetSystemColor(ColorId color_id,
                                       ColorScheme color_scheme) const {
  base::Optional<SkColor> color = color_cache_[color_id];
  if (!color) {
    color = SkColorFromColorId(color_id, this, color_scheme);
    if (!color)
      color = ui::NativeThemeBase::GetSystemColor(color_id, color_scheme);
    color_cache_[color_id] = color;
  }
  DCHECK(color);
  return color.value();
}

void NativeThemeGtk::PaintArrowButton(
    cc::PaintCanvas* canvas,
    const gfx::Rect& rect,
    Part direction,
    State state,
    ColorScheme color_scheme,
    const ScrollbarArrowExtraParams& arrow) const {
  auto context = GetStyleContextFromCss(
      GtkCheckVersion(3, 20)
          ? "GtkScrollbar#scrollbar #contents GtkButton#button"
          : "GtkRange.scrollbar.button");
  GtkStateFlags state_flags = StateToStateFlags(state);
  gtk_style_context_set_state(context, state_flags);

  switch (direction) {
    case kScrollbarUpArrow:
      gtk_style_context_add_class(context, GTK_STYLE_CLASS_TOP);
      break;
    case kScrollbarRightArrow:
      gtk_style_context_add_class(context, GTK_STYLE_CLASS_RIGHT);
      break;
    case kScrollbarDownArrow:
      gtk_style_context_add_class(context, GTK_STYLE_CLASS_BOTTOM);
      break;
    case kScrollbarLeftArrow:
      gtk_style_context_add_class(context, GTK_STYLE_CLASS_LEFT);
      break;
    default:
      NOTREACHED();
  }

  PaintWidget(canvas, rect, context, BG_RENDER_NORMAL, true);
  PaintArrow(canvas, rect, direction, GetFgColorFromStyleContext(context));
}

void NativeThemeGtk::PaintScrollbarTrack(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const ScrollbarTrackExtraParams& extra_params,
    const gfx::Rect& rect,
    ColorScheme color_scheme) const {
  PaintWidget(
      canvas, rect,
      GetStyleContextFromCss(GtkCheckVersion(3, 20)
                                 ? "GtkScrollbar#scrollbar #contents #trough"
                                 : "GtkScrollbar.scrollbar.trough"),
      BG_RENDER_NORMAL, true);
}

void NativeThemeGtk::PaintScrollbarThumb(
    cc::PaintCanvas* canvas,
    Part part,
    State state,
    const gfx::Rect& rect,
    NativeTheme::ScrollbarOverlayColorTheme theme,
    ColorScheme color_scheme) const {
  auto context = GetStyleContextFromCss(
      GtkCheckVersion(3, 20)
          ? "GtkScrollbar#scrollbar #contents #trough #slider"
          : "GtkScrollbar.scrollbar.slider");
  gtk_style_context_set_state(context, StateToStateFlags(state));
  PaintWidget(canvas, rect, context, BG_RENDER_NORMAL, true);
}

void NativeThemeGtk::PaintScrollbarCorner(cc::PaintCanvas* canvas,
                                          State state,
                                          const gfx::Rect& rect,
                                          ColorScheme color_scheme) const {
  auto context = GetStyleContextFromCss(
      GtkCheckVersion(3, 19, 2)
          ? "GtkScrolledWindow#scrolledwindow #junction"
          : "GtkScrolledWindow.scrolledwindow.scrollbars-junction");
  PaintWidget(canvas, rect, context, BG_RENDER_NORMAL, true);
}

void NativeThemeGtk::PaintMenuPopupBackground(
    cc::PaintCanvas* canvas,
    const gfx::Size& size,
    const MenuBackgroundExtraParams& menu_background,
    ColorScheme color_scheme) const {
  PaintWidget(canvas, gfx::Rect(size), GetStyleContextFromCss("GtkMenu#menu"),
              BG_RENDER_RECURSIVE, false);
}

void NativeThemeGtk::PaintMenuItemBackground(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuItemExtraParams& menu_item,
    ColorScheme color_scheme) const {
  auto context = GetStyleContextFromCss("GtkMenu#menu GtkMenuItem#menuitem");
  gtk_style_context_set_state(context, StateToStateFlags(state));
  PaintWidget(canvas, rect, context, BG_RENDER_NORMAL, true);
}

void NativeThemeGtk::PaintMenuSeparator(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const MenuSeparatorExtraParams& menu_separator,
    ColorScheme color_scheme) const {
  // TODO(estade): use GTK to draw vertical separators too. See
  // crbug.com/710183
  if (menu_separator.type == ui::VERTICAL_SEPARATOR) {
    cc::PaintFlags paint;
    paint.setStyle(cc::PaintFlags::kFill_Style);
    paint.setColor(GetSystemColor(ui::NativeTheme::kColorId_MenuSeparatorColor,
                                  color_scheme));
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
  if (GtkCheckVersion(3, 20)) {
    auto context = GetStyleContextFromCss(
        "GtkMenu#menu GtkSeparator#separator.horizontal");
    GtkBorder margin, border, padding;
    int min_height = 1;
#if GTK_CHECK_VERSION(3, 90, 0)
    gtk_style_context_get_margin(context, &margin);
    gtk_style_context_get_border(context, &border);
    gtk_style_context_get_padding(context, &padding);
    gtk_style_context_get(context, "min-height", &min_height, nullptr);
#else
    GtkStateFlags state = gtk_style_context_get_state(context);
    gtk_style_context_get_margin(context, state, &margin);
    gtk_style_context_get_border(context, state, &border);
    gtk_style_context_get_padding(context, state, &padding);
    gtk_style_context_get(context, state, "min-height", &min_height, nullptr);
#endif
    int w = rect.width() - margin.left - margin.right;
    int h = std::max(
        min_height + padding.top + padding.bottom + border.top + border.bottom,
        1);
    int x = margin.left;
    int y = separator_offset(h);
    PaintWidget(canvas, gfx::Rect(x, y, w, h), context, BG_RENDER_NORMAL, true);
  } else {
#if !GTK_CHECK_VERSION(3, 90, 0)
    auto context = GetStyleContextFromCss(
        "GtkMenu#menu GtkMenuItem#menuitem.separator.horizontal");
    gboolean wide_separators = false;
    gint separator_height = 0;
    gtk_style_context_get_style(context, "wide-separators", &wide_separators,
                                "separator-height", &separator_height, nullptr);
    // This code was adapted from gtk/gtkmenuitem.c.  For some reason,
    // padding is used as the margin.
    GtkBorder padding;
    gtk_style_context_get_padding(context, gtk_style_context_get_state(context),
                                  &padding);
    int w = rect.width() - padding.left - padding.right;
    int x = rect.x() + padding.left;
    int h = wide_separators ? separator_height : 1;
    int y = rect.y() + separator_offset(h);
    if (wide_separators) {
      PaintWidget(canvas, gfx::Rect(x, y, w, h), context, BG_RENDER_NONE, true);
    } else {
      cc::PaintFlags flags;
      flags.setColor(GetFgColorFromStyleContext(context));
      flags.setAntiAlias(true);
      flags.setStrokeWidth(1);
      canvas->drawLine(x + 0.5f, y + 0.5f, x + w + 0.5f, y + 0.5f, flags);
    }
#endif
  }
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

  SkBitmap bitmap =
      GetWidgetBitmap(rect.size(), context, BG_RENDER_RECURSIVE, false);
  bitmap.setImmutable();
  canvas->drawImage(cc::PaintImage::CreateFromBitmap(std::move(bitmap)),
                    rect.x(), rect.y());
}

}  // namespace gtk
