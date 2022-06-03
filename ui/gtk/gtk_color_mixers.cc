// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/gtk_color_mixers.h"

#include "base/strings/strcat.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gtk/gtk_util.h"

namespace gtk {

void AddGtkNativeColorMixer(ui::ColorProvider* provider,
                            const ui::ColorProviderManager::Key& key) {
  if (key.system_theme == ui::ColorProviderManager::SystemTheme::kDefault)
    return;

  ui::ColorMixer& mixer = provider->AddMixer();

  const std::string header_selector =
      key.frame_type == ui::ColorProviderManager::FrameType::kChromium
          ? "#headerbar.header-bar.titlebar"
          : "GtkMenuBar#menubar";
  const std::string header_selector_inactive = header_selector + ":backdrop";

  mixer[ui::kColorNativeButtonBackground] = {GetBgColor("GtkButton#button")};
  mixer[ui::kColorNativeButtonBackgroundDisabled] = {
      GetBgColor("GtkButton#button.text-button:disabled")};
  mixer[ui::kColorNativeButtonBorder] = {GetBorderColor("GtkButton#button")};
  mixer[ui::kColorNativeButtonForeground] = {
      GetFgColor("GtkButton#button.text-button GtkLabel#label")};
  mixer[ui::kColorNativeButtonForegroundDisabled] = {
      GetFgColor("GtkButton#button.text-button:disabled GtkLabel#label")};
  mixer[ui::kColorNativeButtonIcon] = {
      GetFgColor("GtkButton#button.flat.scale GtkImage#image")};
  mixer[ui::kColorNativeComboboxBackground] = {GetBgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(), " ",
       "GtkCellView#cellview"}))};
  mixer[ui::kColorNativeComboboxBackgroundHovered] = {GetBgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(),
       ":hover GtkCellView#cellview"}))};
  mixer[ui::kColorNativeComboboxForeground] = {GetFgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(), " ",
       "GtkCellView#cellview"}))};
  mixer[ui::kColorNativeComboboxForegroundHovered] = {GetFgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(),
       ":hover GtkCellView#cellview"}))};
  const SkColor frame_color =
      SkColorSetA(GetBgColor(header_selector), SK_AlphaOPAQUE);
  const SkColor frame_color_inactive =
      SkColorSetA(GetBgColor(header_selector_inactive), SK_AlphaOPAQUE);
  mixer[ui::kColorNativeFrameActive] = {frame_color};
  mixer[ui::kColorNativeFrameInactive] = {frame_color_inactive};
  mixer[ui::kColorNativeFrameBorder] = {GetBorderColor(
      GtkCheckVersion(3, 20) ? "GtkFrame#frame #border" : "GtkFrame#frame")};
  mixer[ui::kColorNativeHeaderButtonBorderActive] = {
      GetBorderColor(header_selector + " GtkButton#button")};
  mixer[ui::kColorNativeHeaderButtonBorderInactive] = {
      GetBorderColor(header_selector + ":backdrop GtkButton#button")};
  mixer[ui::kColorNativeHeaderSeparatorBorderActive] = {GetBorderColor(
      header_selector + " GtkSeparator#separator.vertical.titlebutton")};
  mixer[ui::kColorNativeHeaderSeparatorBorderInactive] = {
      GetBorderColor(header_selector +
                     ":backdrop GtkSeparator#separator.vertical.titlebutton")};
  mixer[ui::kColorNativeImageButtonForeground] = {
      GetFgColor("GtkButton#button.image-button")};
  mixer[ui::kColorNativeImageButtonForegroundHovered] = {
      GetFgColor("GtkButton#button.image-button:hover")};
  mixer[ui::kColorNativeLabelBackgroundSelected] = {
      GetSelectionBgColor(GtkCheckVersion(3, 20) ? "GtkLabel#label #selection"
                                                 : "GtkLabel#label:selected")};
  mixer[ui::kColorNativeLabelForeground] = {GetFgColor("GtkLabel#label")};
  mixer[ui::kColorNativeLabelForegroundDisabled] = {
      GetFgColor("GtkLabel#label:disabled")};
  mixer[ui::kColorNativeLabelForegroundSelected] = {
      GetFgColor(GtkCheckVersion(3, 20) ? "GtkLabel#label #selection"
                                        : "GtkLabel#label:selected")};
  if (GtkCheckVersion(3, 12)) {
    mixer[ui::kColorNativeLinkForeground] = {
        GetFgColor("GtkLabel#label.link:link")};
    mixer[ui::kColorNativeLinkForegroundDisabled] = {
        GetFgColor("GtkLabel#label.link:link:disabled")};
    mixer[ui::kColorNativeLinkForegroundHovered] = {
        GetFgColor("GtkLabel#label.link:link:hover:active")};
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
    mixer[ui::kColorNativeLinkForeground] = {color};
    mixer[ui::kColorNativeLinkForegroundDisabled] = {color};
    mixer[ui::kColorNativeLinkForegroundHovered] = {color};
  }
  mixer[ui::kColorNativeMenuBackground] = {GetBgColor(GtkCssMenu())};
  mixer[ui::kColorNativeMenuBorder] = {GetBorderColor(GtkCssMenu())};
  mixer[ui::kColorNativeMenuItemAccelerator] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(),
                    GtkCheckVersion(3, 20) ? " #accelerator"
                                           : " GtkLabel#label.accelerator"}))};
  mixer[ui::kColorNativeMenuItemBackgroundHovered] = {GetBgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ":hover"}))};
  mixer[ui::kColorNativeMenuItemForeground] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), " GtkLabel#label"}))};
  mixer[ui::kColorNativeMenuItemForegroundHovered] = {GetFgColor(base::StrCat(
      {GtkCssMenu(), " ", GtkCssMenuItem(), ":hover GtkLabel#label"}))};
  mixer[ui::kColorNativeMenuItemForegroundDisabled] = {GetFgColor(base::StrCat(
      {GtkCssMenu(), " ", GtkCssMenuItem(), ":disabled GtkLabel#label"}))};
  mixer[ui::kColorNativeMenuRadio] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(),
                    GtkCheckVersion(3, 20) ? " #radio" : ".radio"}))};
  mixer[ui::kColorNativeMenuSeparator] = {GetSeparatorColor(
      GtkCheckVersion(3, 20)
          ? base::StrCat({GtkCssMenu(), " GtkSeparator#separator.horizontal"})
          : base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ".separator"}))};
  mixer[ui::kColorNativeScaleHighlightBackground] = {
      GetBgColor("GtkScale#scale #highlight")};
  mixer[ui::kColorNativeScaleHighlightBackgroundDisabled] = {
      GetBgColor("GtkScale#scale:disabled #highlight")};
  mixer[ui::kColorNativeScaleTroughBackground] = {
      GetBgColor("GtkScale#scale #trough")};
  mixer[ui::kColorNativeScaleTroughBackgroundDisabled] = {
      GetBgColor("GtkScale#scale:disabled #trough")};
  mixer[ui::kColorNativeScrollbarSliderBackground] = {
      GetBgColor("#GtkScrollbar#scrollbar #slider")};
  mixer[ui::kColorNativeScrollbarSliderBackgroundHovered] = {
      GetBgColor("#GtkScrollbar#scrollbar #slider:hover")};
  mixer[ui::kColorNativeScrollbarTroughBackground] = {
      GetBgColor("#GtkScrollbar#scrollbar #trough")};
  mixer[ui::kColorNativeScrollbarTroughBackgroundHovered] = {
      GetBgColor("#GtkScrollbar#scrollbar #trough:hover")};
  mixer[ui::kColorNativeSeparator] = {
      GetSeparatorColor("GtkSeparator#separator.horizontal")};
  mixer[ui::kColorNativeSpinner] = {GetFgColor("GtkSpinner#spinner")};
  mixer[ui::kColorNativeSpinnerDisabled] = {
      GetFgColor("GtkSpinner#spinner:disabled")};
  mixer[ui::kColorNativeStatusbarBackground] = {GetBgColor("#statusbar")};
  mixer[ui::kColorNativeTabBackgroundChecked] = {
      GetBgColor("GtkNotebook#notebook #tab:checked")};
  mixer[ui::kColorNativeTabBackgroundCheckedFocused] = {
      GetBgColor("GtkNotebook#notebook:focus #tab:checked")};
  mixer[ui::kColorNativeTabForegroundInactiveFrameActive] = {
      GetFgColor(header_selector + " GtkLabel#label.title")};
  mixer[ui::kColorNativeTabForegroundInactiveFrameInactive] = {
      GetFgColor(header_selector_inactive + " GtkLabel#label.title")};
  mixer[ui::kColorNativeTextareaBackground] = {
      GetBgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view"
                                        : "GtkTextView.view")};
  mixer[ui::kColorNativeTextareaBackgroundDisabled] = {
      GetBgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:disabled"
                                        : "GtkTextView.view:disabled")};
  mixer[ui::kColorNativeTextareaBackgroundSelected] = {GetSelectionBgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text #selection"
                             : "GtkTextView.view:selected")};
  mixer[ui::kColorNativeTextareaForeground] = {
      GetFgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text"
                                        : "GtkTextView.view")};
  mixer[ui::kColorNativeTextareaForegroundDisabled] = {GetFgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:disabled #text"
                             : "GtkTextView.view:disabled")};
  mixer[ui::kColorNativeTextareaForegroundSelected] = {GetFgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text #selection"
                             : "GtkTextView.view:selected")};
  mixer[ui::kColorNativeTextfieldBorderUnfocused] = {
      GetBorderColor("GtkEntry#entry")};
  mixer[ui::kColorNativeTextfieldBorderFocused] = {
      GetBorderColor("GtkEntry#entry:focus")};
  mixer[ui::kColorNativeTextfieldForegroundPlaceholder] = {
      GtkCheckVersion(4)
          ? GetFgColor("GtkEntry#entry #text #placeholder")
          : GtkStyleContextLookupColor(GetStyleContextFromCss("GtkEntry#entry"),
                                       "placeholder_text_color")
                // This is copied from gtkentry.c.  GTK uses a fallback of 50%
                // gray when the theme doesn't provide a placeholder color.
                .value_or(SkColorSetRGB(127, 127, 127))};
  mixer[ui::kColorNativeToggleButtonBackgroundChecked] = {
      GetBgColor("GtkButton#button.text-button.toggle:checked")};
  mixer[ui::kColorNativeToggleButtonBackgroundUnchecked] = {
      GetBgColor("GtkButton#button.text-button.toggle")};
  SkColor toolbar_color =
      color_utils::GetResultingPaintColor(GetBgColor(""), frame_color);
  mixer[ui::kColorNativeToolbarBackground] = {toolbar_color};
  const auto tooltip_context = AppendCssNodeToStyleContext(
      {}, GtkCheckVersion(3, 20) ? "#tooltip.background"
                                 : "GtkWindow#window.background.tooltip");
  mixer[ui::kColorNativeTooltipBackground] = {
      GetBgColorFromStyleContext(tooltip_context)};
  mixer[ui::kColorNativeTooltipForeground] = {GtkStyleContextGetColor(
      AppendCssNodeToStyleContext(tooltip_context, "GtkLabel#label"))};
  mixer[ui::kColorNativeTreeHeaderBackground] = {
      GetBgColor("GtkTreeView#treeview.view GtkButton#button")};
  mixer[ui::kColorNativeTreeHeaderBorder] = {
      GetBorderColor("GtkTreeView#treeview.view GtkButton#button")};
  mixer[ui::kColorNativeTreeHeaderForeground] = {
      GetFgColor("GtkTreeView#treeview.view GtkButton#button GtkLabel#label")};
  mixer[ui::kColorNativeTreeNodeBackground] = {
      GetBgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell")};
  mixer[ui::kColorNativeTreeNodeBackgroundSelected] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected")};
  mixer[ui::kColorNativeTreeNodeBackgroundSelectedFocused] = {
      GetBgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected:focus")};
  mixer[ui::kColorNativeTreeNodeForeground] = {
      GetFgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell "
                 "GtkLabel#label")};
  mixer[ui::kColorNativeTreeNodeForegroundSelected] = {
      GetFgColor("GtkTreeView#treeview.view "
                 "GtkTreeView#treeview.view.cell:selected GtkLabel#label")};
  mixer[ui::kColorNativeTreeNodeForegroundSelectedFocused] = {GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus GtkLabel#label")};
  mixer[ui::kColorNativeWindowBackground] = {GetBgColor("")};

  // Core colors
  mixer[ui::kColorAccent] = {ui::kColorNativeTreeNodeBackgroundSelectedFocused};
  mixer[ui::kColorAlertHighSeverity] = {SelectBasedOnDarkInput(
      ui::kColorPrimaryBackground, gfx::kGoogleRed300, gfx::kGoogleRed600)};
  mixer[ui::kColorAlertLowSeverity] = {SelectBasedOnDarkInput(
      ui::kColorPrimaryBackground, gfx::kGoogleGreen300, gfx::kGoogleGreen700)};
  mixer[ui::kColorAlertMediumSeverity] = {
      SelectBasedOnDarkInput(ui::kColorPrimaryBackground, gfx::kGoogleYellow300,
                             gfx::kGoogleYellow700)};
  mixer[ui::kColorDisabledForeground] = {
      ui::kColorNativeLabelForegroundDisabled};
  mixer[ui::kColorItemHighlight] = {ui::kColorNativeTextfieldBorderFocused};
  mixer[ui::kColorItemSelectionBackground] = {ui::kColorAccent};
  mixer[ui::kColorMenuSelectionBackground] = {
      ui::kColorNativeMenuItemBackgroundHovered};
  mixer[ui::kColorMidground] = {ui::kColorNativeSeparator};
  mixer[ui::kColorPrimaryBackground] = {ui::kColorNativeWindowBackground};
  mixer[ui::kColorPrimaryForeground] = {ui::kColorNativeLabelForeground};
  mixer[ui::kColorSecondaryForeground] = {
      ui::kColorNativeLabelForegroundDisabled};
  mixer[ui::kColorTextSelectionBackground] = {
      ui::kColorNativeLabelBackgroundSelected};
  mixer[ui::kColorTextSelectionForeground] = {
      ui::kColorNativeLabelForegroundSelected};

  // UI element colors
  mixer[ui::kColorAvatarHeaderArt] =
      AlphaBlend(ui::kColorPrimaryForeground, ui::kColorPrimaryBackground,
                 gfx::kGoogleGreyAlpha300);
  mixer[ui::kColorAvatarIconGuest] =
      DeriveDefaultIconColor(ui::kColorPrimaryForeground);
  mixer[ui::kColorButtonBackground] = {ui::kColorNativeButtonBackground};
  mixer[ui::kColorButtonBackgroundProminentDisabled] = {
      ui::kColorNativeButtonBackgroundDisabled};
  mixer[ui::kColorButtonBorder] = {ui::kColorNativeButtonBorder};
  mixer[ui::kColorButtonBorderDisabled] = {
      ui::kColorNativeButtonBackgroundDisabled};
  mixer[ui::kColorButtonForeground] = {ui::kColorNativeButtonForeground};
  mixer[ui::kColorButtonForegroundChecked] = {ui::kColorAccent};
  mixer[ui::kColorButtonForegroundDisabled] = {
      ui::kColorNativeButtonForegroundDisabled};
  mixer[ui::kColorButtonForegroundProminent] = {
      ui::kColorNativeTreeNodeForegroundSelectedFocused};
  mixer[ui::kColorButtonForegroundUnchecked] = {ui::kColorButtonForeground};
  mixer[ui::kColorDialogForeground] = {ui::kColorPrimaryForeground};
  mixer[ui::kColorDropdownBackground] = {ui::kColorNativeComboboxBackground};
  mixer[ui::kColorDropdownBackgroundSelected] = {
      ui::kColorNativeComboboxBackgroundHovered};
  mixer[ui::kColorDropdownForeground] = {ui::kColorNativeComboboxForeground};
  mixer[ui::kColorDropdownForegroundSelected] = {
      ui::kColorNativeComboboxForegroundHovered};
  mixer[ui::kColorFrameActive] = {ui::kColorNativeFrameActive};
  mixer[ui::kColorFrameInactive] = {ui::kColorNativeFrameInactive};
  mixer[ui::kColorFocusableBorderUnfocused] = {
      ui::kColorNativeTextfieldBorderUnfocused};
  mixer[ui::kColorHelpIconActive] = {
      ui::kColorNativeImageButtonForegroundHovered};
  mixer[ui::kColorIcon] = {ui::kColorNativeButtonIcon};
  mixer[ui::kColorHelpIconInactive] = {ui::kColorNativeImageButtonForeground};
  mixer[ui::kColorLinkForeground] = {ui::kColorNativeLinkForeground};
  mixer[ui::kColorLinkForegroundDisabled] = {
      ui::kColorNativeLinkForegroundDisabled};
  mixer[ui::kColorLinkForegroundPressed] = {
      ui::kColorNativeLinkForegroundHovered};
  mixer[ui::kColorMenuBackground] = {ui::kColorNativeMenuBackground};
  mixer[ui::kColorMenuBorder] = {ui::kColorNativeMenuBorder};
  mixer[ui::kColorMenuDropmarker] = {ui::kColorMenuItemForeground};
  mixer[ui::kColorMenuIcon] = {ui::kColorNativeMenuRadio};
  mixer[ui::kColorMenuItemBackgroundHighlighted] = {ui::kColorMenuBackground};
  mixer[ui::kColorMenuItemForeground] = {ui::kColorNativeMenuItemForeground};
  mixer[ui::kColorMenuItemForegroundHighlighted] = {
      ui::kColorMenuItemForeground};
  mixer[ui::kColorMenuItemForegroundDisabled] = {
      ui::kColorNativeMenuItemForegroundDisabled};
  mixer[ui::kColorMenuItemForegroundSecondary] = {
      ui::kColorNativeMenuItemAccelerator};
  mixer[ui::kColorMenuItemForegroundSelected] = {
      ui::kColorNativeMenuItemForegroundHovered};
  mixer[ui::kColorMenuSeparator] = {ui::kColorNativeMenuSeparator};
  mixer[ui::kColorNotificationInputForeground] = {
      ui::kColorNativeTreeNodeForegroundSelectedFocused};
  mixer[ui::kColorOverlayScrollbarFill] = {
      ui::kColorNativeScrollbarSliderBackground};
  mixer[ui::kColorOverlayScrollbarFillHovered] = {
      ui::kColorNativeScrollbarSliderBackgroundHovered};
  mixer[ui::kColorOverlayScrollbarStroke] = {
      ui::kColorNativeScrollbarTroughBackground};
  mixer[ui::kColorOverlayScrollbarStrokeHovered] = {
      ui::kColorNativeScrollbarTroughBackgroundHovered};
  mixer[ui::kColorSliderThumb] = {ui::kColorNativeScaleHighlightBackground};
  mixer[ui::kColorSliderThumbMinimal] = {
      ui::kColorNativeScaleHighlightBackgroundDisabled};
  mixer[ui::kColorSliderTrack] = {ui::kColorNativeScaleTroughBackground};
  mixer[ui::kColorSliderTrackMinimal] = {
      ui::kColorNativeScaleTroughBackgroundDisabled};
  mixer[ui::kColorSyncInfoBackground] = {ui::kColorNativeStatusbarBackground};
  mixer[ui::kColorTabBackgroundHighlighted] = {
      ui::kColorNativeTabBackgroundChecked};
  mixer[ui::kColorTabBackgroundHighlightedFocused] = {
      ui::kColorNativeTabBackgroundCheckedFocused};
  mixer[ui::kColorTabContentSeparator] = {ui::kColorNativeFrameBorder};
  mixer[ui::kColorTabForegroundSelected] = {ui::kColorPrimaryForeground};
  mixer[ui::kColorTableBackground] = {ui::kColorTreeBackground};
  mixer[ui::kColorTableBackgroundAlternate] = {ui::kColorTreeBackground};
  mixer[ui::kColorTableBackgroundSelectedUnfocused] = {
      ui::kColorTreeNodeBackgroundSelectedUnfocused};
  mixer[ui::kColorTableForeground] = {ui::kColorTreeNodeForeground};
  mixer[ui::kColorTableForegroundSelectedFocused] = {
      ui::kColorTreeNodeForegroundSelectedFocused};
  mixer[ui::kColorTableForegroundSelectedUnfocused] = {
      ui::kColorTreeNodeForegroundSelectedUnfocused};
  mixer[ui::kColorTableGroupingIndicator] = {ui::kColorTableForeground};
  mixer[ui::kColorTableHeaderBackground] = {
      ui::kColorNativeTreeHeaderBackground};
  mixer[ui::kColorTableHeaderForeground] = {
      ui::kColorNativeTreeHeaderForeground};
  mixer[ui::kColorTableHeaderSeparator] = {ui::kColorNativeTreeHeaderBorder};
  mixer[ui::kColorTextfieldBackground] = {ui::kColorNativeTextareaBackground};
  mixer[ui::kColorTextfieldBackgroundDisabled] = {
      ui::kColorNativeTextareaBackgroundDisabled};
  mixer[ui::kColorTextfieldForeground] = {ui::kColorNativeTextareaForeground};
  mixer[ui::kColorTextfieldForegroundDisabled] = {
      ui::kColorNativeTextareaForegroundDisabled};
  mixer[ui::kColorTextfieldForegroundPlaceholder] = {
      ui::kColorNativeTextfieldForegroundPlaceholder};
  mixer[ui::kColorTextfieldSelectionBackground] = {
      ui::kColorNativeTextareaBackgroundSelected};
  mixer[ui::kColorTextfieldSelectionForeground] = {
      ui::kColorNativeTextareaForegroundSelected};
  mixer[ui::kColorThrobber] = {ui::kColorNativeSpinner};
  mixer[ui::kColorThrobberPreconnect] = {ui::kColorNativeSpinnerDisabled};
  mixer[ui::kColorToggleButtonTrackOff] = {
      ui::kColorNativeToggleButtonBackgroundUnchecked};
  mixer[ui::kColorToggleButtonTrackOn] = {
      ui::kColorNativeToggleButtonBackgroundChecked};
  mixer[ui::kColorTooltipBackground] = {ui::kColorNativeTooltipBackground};
  mixer[ui::kColorTooltipForeground] = {ui::kColorNativeTooltipForeground};
  mixer[ui::kColorTreeBackground] = {ui::kColorNativeTreeNodeBackground};
  mixer[ui::kColorTreeNodeForeground] = {ui::kColorNativeTreeNodeForeground};
  mixer[ui::kColorTreeNodeForegroundSelectedFocused] = {
      ui::kColorNativeTreeNodeForegroundSelectedFocused};
  mixer[ui::kColorTreeNodeBackgroundSelectedUnfocused] = {
      ui::kColorNativeTreeNodeBackgroundSelected};
  mixer[ui::kColorTreeNodeForegroundSelectedUnfocused] = {
      ui::kColorNativeTreeNodeForegroundSelected};
}

}  // namespace gtk
