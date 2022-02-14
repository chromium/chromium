// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_color_mixers.h"

#include <array>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/strcat.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_set.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gtk/gtk_util.h"

namespace gtk {

namespace {

GtkCssContext GetTooltipContext() {
  return AppendCssNodeToStyleContext(
      {}, GtkCheckVersion(3, 20) ? "#tooltip.background"
                                 : "GtkWindow#window.background.tooltip");
}

// TODO(pkasting): Inline functionality below using a transform.
SkColor GetAlertSeverityColor(ui::ColorId color_id, bool dark) {
  constexpr auto kColorIdMap =
      base::MakeFixedFlatMap<ui::ColorId, std::array<SkColor, 2>>({
          {ui::kColorAlertHighSeverity,
           {{gfx::kGoogleRed600, gfx::kGoogleRed300}}},
          {ui::kColorAlertLowSeverity,
           {{gfx::kGoogleGreen700, gfx::kGoogleGreen300}}},
          {ui::kColorAlertMediumSeverity,
           {{gfx::kGoogleYellow700, gfx::kGoogleYellow300}}},
      });
  return kColorIdMap.at(color_id)[dark];
}

}  // namespace

void AddGtkNativeCoreColorMixer(ui::ColorProvider* provider,
                                const ui::ColorProviderManager::Key& key) {
  if (key.system_theme == ui::ColorProviderManager::SystemTheme::kDefault)
    return;

  ui::ColorMixer& mixer = provider->AddMixer();

  // TODO(pkasting): Most of these don't belong in a "native core color mixer"
  // as they're cross-platform UI color concepts. They should be moved into a UI
  // color mixer, systematized, and have the native access portions pushed into
  // new native colors which can live in this mixer.
  mixer[ui::kColorWindowBackground] = {GetBgColor("")};
  mixer[ui::kColorDialogBackground] = {GetBgColor("")};
  mixer[ui::kColorBubbleBackground] = {GetBgColor("")};
  mixer[ui::kColorNotificationBackgroundInactive] = {GetBgColor("")};
  mixer[ui::kColorDialogForeground] = {GetFgColor("GtkLabel#label")};
  mixer[ui::kColorBubbleFooterBackground] = {GetBgColor("#statusbar")};
  mixer[ui::kColorSyncInfoBackground] = {GetBgColor("#statusbar")};
  mixer[ui::kColorNotificationActionsBackground] = {
      ui::BlendTowardMaxContrast(GetBgColor(""), gfx::kGoogleGreyAlpha100)};
  mixer[ui::kColorNotificationBackgroundActive] = {
      ui::BlendTowardMaxContrast(GetBgColor(""), gfx::kGoogleGreyAlpha100)};
  mixer[ui::kColorNotificationImageBackground] = {
      ui::BlendTowardMaxContrast(GetBgColor(""), gfx::kGoogleGreyAlpha100)};
  mixer[ui::kColorFocusableBorderFocused] = {
      // GetBorderColor("GtkEntry#entry:focus") is correct here.  The focus ring
      // around widgets is usually a lighter version of the "canonical theme
      // color" - orange on Ambiance, blue on Adwaita, etc.  However, Chrome
      // lightens the color we give it, so it would look wrong if we give it an
      // already-lightened color.  This workaround returns the theme color
      // directly, taken from a selected table row.  This has matched the theme
      // color on every theme that I've tested.
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorFocusableBorderUnfocused] = {
      GetBorderColor("GtkEntry#entry")};
  mixer[ui::kColorMenuBackground] = {GetBgColor(GtkCssMenu())};
  mixer[ui::kColorMenuItemBackgroundHighlighted] = {GetBgColor(GtkCssMenu())};
  mixer[ui::kColorMenuItemBackgroundAlertedInitial] = {
      GetBgColor(GtkCssMenu())};
  mixer[ui::kColorMenuItemBackgroundAlertedTarget] = {GetBgColor(GtkCssMenu())};
  mixer[ui::kColorSubtleEmphasisBackground] = {GetBgColor(GtkCssMenu())};
  mixer[ui::kColorMenuBorder] = {GetBorderColor(GtkCssMenu())};
  mixer[ui::kColorMenuItemBackgroundSelected] = {GetBgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ":hover"}))};
  mixer[ui::kColorMenuItemForeground] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), " GtkLabel#label"}))};
  mixer[ui::kColorMenuDropmarker] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), " GtkLabel#label"}))};
  mixer[ui::kColorMenuItemForegroundHighlighted] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), " GtkLabel#label"}))};
  mixer[ui::kColorMenuItemForegroundSelected] = {GetFgColor(base::StrCat(
      {GtkCssMenu(), " ", GtkCssMenuItem(), ":hover GtkLabel#label"}))};
  mixer[ui::kColorMenuItemForegroundDisabled] = {GetFgColor(base::StrCat(
      {GtkCssMenu(), " ", GtkCssMenuItem(), ":disabled GtkLabel#label"}))};
  mixer[ui::kColorMenuItemForegroundSecondary] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(),
                    GtkCheckVersion(3, 20) ? " #accelerator"
                                           : " GtkLabel#label.accelerator"}))};
  mixer[ui::kColorMenuSeparator] = {GetSeparatorColor(
      GtkCheckVersion(3, 20)
          ? base::StrCat({GtkCssMenu(), " GtkSeparator#separator.horizontal"})
          : base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ".separator"}))};
  mixer[ui::kColorDropdownBackground] = {GetBgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(), " ",
       "GtkCellView#cellview"}))};
  mixer[ui::kColorDropdownForeground] = {GetFgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(), " ",
       "GtkCellView#cellview"}))};
  mixer[ui::kColorDropdownBackgroundSelected] = {GetBgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(),
       ":hover GtkCellView#cellview"}))};
  mixer[ui::kColorDropdownForegroundSelected] = {GetFgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(),
       ":hover GtkCellView#cellview"}))};
  mixer[ui::kColorLabelForeground] = {GetFgColor("GtkLabel#label")};
  mixer[ui::kColorPrimaryForeground] = {GetFgColor("GtkLabel#label")};
  mixer[ui::kColorLabelForegroundDisabled] = {
      GetFgColor("GtkLabel#label:disabled")};
  mixer[ui::kColorLabelForegroundSecondary] = {
      GetFgColor("GtkLabel#label:disabled")};
  mixer[ui::kColorDisabledForeground] = {GetFgColor("GtkLabel#label:disabled")};
  mixer[ui::kColorSecondaryForeground] = {
      GetFgColor("GtkLabel#label:disabled")};
  mixer[ui::kColorLabelSelectionForeground] = {
      GetFgColor(GtkCheckVersion(3, 20) ? "GtkLabel#label #selection"
                                        : "GtkLabel#label:selected")};
  mixer[ui::kColorLabelSelectionBackground] = {
      GetSelectionBgColor(GtkCheckVersion(3, 20) ? "GtkLabel#label #selection"
                                                 : "GtkLabel#label:selected")};
  if (GtkCheckVersion(3, 12)) {
    mixer[ui::kColorLinkForegroundDisabled] = {
        GetFgColor("GtkLabel#label.link:link:disabled")};
    mixer[ui::kColorLinkForegroundPressed] = {
        GetFgColor("GtkLabel#label.link:link:hover:active")};
    mixer[ui::kColorLinkForeground] = {GetFgColor("GtkLabel#label.link:link")};
  } else {
    auto link_context = GetStyleContextFromCss("GtkLabel#label.view");
    GdkColor* gdk_color = nullptr;
    GtkStyleContextGetStyle(link_context, "link-color", &gdk_color, nullptr);
    SkColor color = SkColorSetRGB(0x00, 0x00, 0xEE);  // From gtklinkbutton.c.
    if (gdk_color) {
      color = SkColorSetRGB(gdk_color->red >> 8, gdk_color->green >> 8,
                            gdk_color->blue >> 8);
      // gdk_color_free() was deprecated in Gtk3.14.  This code path is only
      // taken on versions earlier than Gtk3.12, but the compiler doesn't know
      // that, so silence the deprecation warnings.
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gdk_color_free(gdk_color);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
    mixer[ui::kColorLinkForegroundDisabled] = {color};
    mixer[ui::kColorLinkForegroundPressed] = {color};
    mixer[ui::kColorLinkForeground] = {color};
  }
  mixer[ui::kColorOverlayScrollbarStroke] = {
      GetBgColor("#GtkScrollbar#scrollbar #trough")};
  mixer[ui::kColorOverlayScrollbarStrokeHovered] = {
      GetBgColor("#GtkScrollbar#scrollbar #trough:hover")};
  mixer[ui::kColorOverlayScrollbarFill] = {
      GetBgColor("#GtkScrollbar#scrollbar #slider")};
  mixer[ui::kColorOverlayScrollbarFillHovered] = {
      GetBgColor("#GtkScrollbar#scrollbar #slider:hover")};
  mixer[ui::kColorSliderThumb] = {GetBgColor("GtkScale#scale #highlight")};
  mixer[ui::kColorSliderTrack] = {GetBgColor("GtkScale#scale #trough")};
  mixer[ui::kColorSliderThumbMinimal] = {
      GetBgColor("GtkScale#scale:disabled #highlight")};
  mixer[ui::kColorSliderTrackMinimal] = {
      GetBgColor("GtkScale#scale:disabled #trough")};
  mixer[ui::kColorMidground] = {
      GetSeparatorColor("GtkSeparator#separator.horizontal")};
  mixer[ui::kColorSeparator] = {
      GetSeparatorColor("GtkSeparator#separator.horizontal")};
  mixer[ui::kColorButtonBackground] = {GetBgColor("GtkButton#button")};
  mixer[ui::kColorButtonForeground] = {
      GetFgColor("GtkButton#button.text-button GtkLabel#label")};
  mixer[ui::kColorButtonForegroundUnchecked] = {
      GetFgColor("GtkButton#button.text-button GtkLabel#label")};
  mixer[ui::kColorButtonForegroundDisabled] = {
      GetFgColor("GtkButton#button.text-button:disabled GtkLabel#label")};
  mixer[ui::kColorAccent] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorButtonForegroundChecked] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorButtonBackgroundProminent] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorButtonBackgroundProminentFocused] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorNotificationInputBackground] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorProgressBar] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorButtonForegroundProminent] = {GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus GtkLabel#label")};
  mixer[ui::kColorNotificationInputForeground] = {GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus GtkLabel#label")};
  mixer[ui::kColorButtonBackgroundProminentDisabled] = {
      GetBgColor("GtkButton#button.text-button:disabled")};
  mixer[ui::kColorButtonBorderDisabled] = {
      GetBgColor("GtkButton#button.text-button:disabled")};
  mixer[ui::kColorButtonBorder] = {
      GetBorderColor("GtkButton#button.text-button")};
  mixer[ui::kColorToggleButtonTrackOff] = {
      GetBgColor("GtkButton#button.text-button.toggle")};
  mixer[ui::kColorToggleButtonTrackOn] = {
      GetBgColor("GtkButton#button.text-button.toggle:checked")};
  mixer[ui::kColorTabForegroundSelected] = {GetFgColor("GtkLabel#label")};
  mixer[ui::kColorTabForeground] = {GetFgColor("GtkLabel#label:disabled")};
  mixer[ui::kColorTabContentSeparator] = {GetBorderColor(
      GtkCheckVersion(3, 20) ? "GtkFrame#frame #border" : "GtkFrame#frame")};
  mixer[ui::kColorTabBackgroundHighlighted] = {
      GetBgColor("GtkNotebook#notebook #tab:checked")};
  mixer[ui::kColorTabBackgroundHighlightedFocused] = {
      GetBgColor("GtkNotebook#notebook:focus #tab:checked")};
  mixer[ui::kColorTextfieldForeground] = {
      GetFgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text"
                                        : "GtkTextView.view")};
  mixer[ui::kColorTextfieldBackground] = {
      GetBgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view"
                                        : "GtkTextView.view")};
  mixer[ui::kColorTextfieldForegroundPlaceholder] = {
      GtkCheckVersion(4)
          ? GetFgColor("GtkEntry#entry #text #placeholder")
          : GtkStyleContextLookupColor(GetStyleContextFromCss("GtkEntry#entry"),
                                       "placeholder_text_color")
                // This is copied from gtkentry.c.  GTK uses a fallback of 50%
                // gray when the theme doesn't provide a placeholder color.
                .value_or(SkColorSetRGB(127, 127, 127))};
  mixer[ui::kColorTextfieldForegroundDisabled] = {GetFgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:disabled #text"
                             : "GtkTextView.view:disabled")};
  mixer[ui::kColorTextfieldBackgroundDisabled] = {
      GetBgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:disabled"
                                        : "GtkTextView.view:disabled")};
  mixer[ui::kColorTextfieldSelectionForeground] = {GetFgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text #selection"
                             : "GtkTextView.view:selected")};
  mixer[ui::kColorTextfieldSelectionBackground] = {GetSelectionBgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text #selection"
                             : "GtkTextView.view:selected")};
  mixer[ui::kColorTooltipBackground] = {
      GetBgColorFromStyleContext(GetTooltipContext())};
  mixer[ui::kColorHelpIconInactive] = {
      GetFgColor("GtkButton#button.image-button")};
  mixer[ui::kColorHelpIconActive] = {
      GetFgColor("GtkButton#button.image-button:hover")};
  mixer[ui::kColorTooltipForeground] = {GtkStyleContextGetColor(
      AppendCssNodeToStyleContext(GetTooltipContext(), "GtkLabel#label"))};
  mixer[ui::kColorTableBackground] = {
      GetBgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell")};
  mixer[ui::kColorTableBackgroundAlternate] = {
      GetBgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell")};
  mixer[ui::kColorTreeBackground] = {
      GetBgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell")};
  mixer[ui::kColorTableForeground] = {
      GetFgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell "
                 "GtkLabel#label")};
  mixer[ui::kColorTreeNodeForeground] = {
      GetFgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell "
                 "GtkLabel#label")};
  mixer[ui::kColorTableGroupingIndicator] = {
      GetFgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell "
                 "GtkLabel#label")};
  mixer[ui::kColorTableForegroundSelectedFocused] = {GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus GtkLabel#label")};
  mixer[ui::kColorTableForegroundSelectedUnfocused] = {GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus GtkLabel#label")};
  mixer[ui::kColorTreeNodeForegroundSelectedFocused] = {GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus GtkLabel#label")};
  mixer[ui::kColorTreeNodeForegroundSelectedUnfocused] = {GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus GtkLabel#label")};
  mixer[ui::kColorTableBackgroundSelectedFocused] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorTableBackgroundSelectedUnfocused] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorTreeNodeBackgroundSelectedFocused] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorTreeNodeBackgroundSelectedUnfocused] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorTableHeaderForeground] = {
      GetFgColor("GtkTreeView#treeview.view GtkButton#button GtkLabel#label")};
  mixer[ui::kColorTableHeaderBackground] = {
      GetBgColor("GtkTreeView#treeview.view GtkButton#button")};
  mixer[ui::kColorTableHeaderSeparator] = {
      GetBorderColor("GtkTreeView#treeview.view GtkButton#button")};
  mixer[ui::kColorThrobber] = {GetFgColor("GtkSpinner#spinner")};
  mixer[ui::kColorThrobberPreconnect] = {
      GetFgColor("GtkSpinner#spinner:disabled")};
  mixer[ui::kColorAvatarIconIncognito] = {GetFgColor("GtkLabel#label")};
  mixer[ui::kColorAvatarIconGuest] =
      ui::DeriveDefaultIconColor(GetFgColor("GtkLabel#label"));
  mixer[ui::kColorAvatarHeaderArt] = ui::AlphaBlend(
      GetFgColor("GtkLabel#label"), GetBgColor(""), gfx::kGoogleGreyAlpha300);
  mixer[ui::kColorAlertLowSeverity] = {GetAlertSeverityColor(
      ui::kColorAlertLowSeverity, color_utils::IsDark(GetBgColor("")))};
  mixer[ui::kColorAlertMediumSeverity] = {GetAlertSeverityColor(
      ui::kColorAlertMediumSeverity, color_utils::IsDark(GetBgColor("")))};
  mixer[ui::kColorAlertHighSeverity] = {GetAlertSeverityColor(
      ui::kColorAlertHighSeverity, color_utils::IsDark(GetBgColor("")))};
  mixer[ui::kColorMenuIcon] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(),
                    GtkCheckVersion(3, 20) ? " #radio" : ".radio"}))};
  mixer[ui::kColorIcon] = {
      GetFgColor("GtkButton#button.flat.scale GtkImage#image")};

  mixer[ui::kColorEndpointBackground] =
      ui::GetColorWithMaxContrast(ui::kColorEndpointForeground);
  mixer[ui::kColorEndpointForeground] =
      ui::GetColorWithMaxContrast(ui::kColorWindowBackground);

  mixer[ui::kColorNativeButtonBorder] = {GetBorderColor("GtkButton#button")};
}

}  // namespace gtk
