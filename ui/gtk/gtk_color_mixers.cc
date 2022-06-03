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

  const SkColor kColorNativeButtonBackground = GetBgColor("GtkButton#button");
  const SkColor kColorNativeButtonBackgroundDisabled =
      GetBgColor("GtkButton#button.text-button:disabled");
  const SkColor kColorNativeButtonBorder = GetBorderColor("GtkButton#button");
  const SkColor kColorNativeButtonForeground =
      GetFgColor("GtkButton#button.text-button GtkLabel#label");
  const SkColor kColorNativeButtonForegroundDisabled =
      GetFgColor("GtkButton#button.text-button:disabled GtkLabel#label");
  const SkColor kColorNativeButtonIcon =
      GetFgColor("GtkButton#button.flat.scale GtkImage#image");
  const SkColor kColorNativeComboboxBackground = GetBgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(), " ",
       "GtkCellView#cellview"}));
  const SkColor kColorNativeComboboxBackgroundHovered = GetBgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(),
       ":hover GtkCellView#cellview"}));
  const SkColor kColorNativeComboboxForeground = GetFgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(), " ",
       "GtkCellView#cellview"}));
  const SkColor kColorNativeComboboxForegroundHovered = GetFgColor(base::StrCat(
      {"GtkComboBoxText#combobox GtkWindow#window.background.popup ",
       "GtkTreeMenu#menu(gtk-combobox-popup-menu) ", GtkCssMenuItem(),
       ":hover GtkCellView#cellview"}));
  const SkColor frame_color =
      SkColorSetA(GetBgColor(header_selector), SK_AlphaOPAQUE);
  const SkColor frame_color_inactive =
      SkColorSetA(GetBgColor(header_selector_inactive), SK_AlphaOPAQUE);
  const SkColor kColorNativeFrameActive = frame_color;
  const SkColor kColorNativeFrameInactive = frame_color_inactive;
  const SkColor kColorNativeFrameBorder = GetBorderColor(
      GtkCheckVersion(3, 20) ? "GtkFrame#frame #border" : "GtkFrame#frame");
  const SkColor kColorNativeImageButtonForeground =
      GetFgColor("GtkButton#button.image-button");
  const SkColor kColorNativeImageButtonForegroundHovered =
      GetFgColor("GtkButton#button.image-button:hover");
  const SkColor kColorNativeLabelBackgroundSelected =
      GetSelectionBgColor(GtkCheckVersion(3, 20) ? "GtkLabel#label #selection"
                                                 : "GtkLabel#label:selected");
  const SkColor kColorNativeLabelForeground = GetFgColor("GtkLabel#label");
  const SkColor kColorNativeLabelForegroundDisabled =
      GetFgColor("GtkLabel#label:disabled");
  const SkColor kColorNativeLabelForegroundSelected =
      GetFgColor(GtkCheckVersion(3, 20) ? "GtkLabel#label #selection"
                                        : "GtkLabel#label:selected");

  SkColor link;
  SkColor link_disabled;
  SkColor link_hovered;
  if (GtkCheckVersion(3, 12)) {
    link = GetFgColor("GtkLabel#label.link:link");
    link_disabled = GetFgColor("GtkLabel#label.link:link:disabled");
    link_hovered = GetFgColor("GtkLabel#label.link:link:hover:active");
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
    link = color;
    link_disabled = color;
    link_hovered = color;
  }
  const SkColor kColorNativeMenuBackground = GetBgColor(GtkCssMenu());
  const SkColor kColorNativeMenuBorder = GetBorderColor(GtkCssMenu());
  const SkColor kColorNativeMenuItemAccelerator = GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(),
                    GtkCheckVersion(3, 20) ? " #accelerator"
                                           : " GtkLabel#label.accelerator"}));
  const SkColor kColorNativeMenuItemBackgroundHovered =
      GetBgColor(base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ":hover"}));
  const SkColor kColorNativeMenuItemForeground = GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), " GtkLabel#label"}));
  const SkColor kColorNativeMenuItemForegroundHovered = GetFgColor(base::StrCat(
      {GtkCssMenu(), " ", GtkCssMenuItem(), ":hover GtkLabel#label"}));
  const SkColor kColorNativeMenuItemForegroundDisabled =
      GetFgColor(base::StrCat(
          {GtkCssMenu(), " ", GtkCssMenuItem(), ":disabled GtkLabel#label"}));
  const SkColor kColorNativeMenuRadio =
      GetFgColor(base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(),
                               GtkCheckVersion(3, 20) ? " #radio" : ".radio"}));
  const SkColor kColorNativeMenuSeparator = GetSeparatorColor(
      GtkCheckVersion(3, 20)
          ? base::StrCat({GtkCssMenu(), " GtkSeparator#separator.horizontal"})
          : base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ".separator"}));
  const SkColor kColorNativeScaleHighlightBackground =
      GetBgColor("GtkScale#scale #highlight");
  const SkColor kColorNativeScaleHighlightBackgroundDisabled =
      GetBgColor("GtkScale#scale:disabled #highlight");
  const SkColor kColorNativeScaleTroughBackground =
      GetBgColor("GtkScale#scale #trough");
  const SkColor kColorNativeScaleTroughBackgroundDisabled =
      GetBgColor("GtkScale#scale:disabled #trough");
  const SkColor kColorNativeScrollbarSliderBackground =
      GetBgColor("#GtkScrollbar#scrollbar #slider");
  const SkColor kColorNativeScrollbarSliderBackgroundHovered =
      GetBgColor("#GtkScrollbar#scrollbar #slider:hover");
  const SkColor kColorNativeScrollbarTroughBackground =
      GetBgColor("#GtkScrollbar#scrollbar #trough");
  const SkColor kColorNativeScrollbarTroughBackgroundHovered =
      GetBgColor("#GtkScrollbar#scrollbar #trough:hover");
  const SkColor kColorNativeSeparator =
      GetSeparatorColor("GtkSeparator#separator.horizontal");
  const SkColor kColorNativeSpinner = GetFgColor("GtkSpinner#spinner");
  const SkColor kColorNativeSpinnerDisabled =
      GetFgColor("GtkSpinner#spinner:disabled");
  const SkColor kColorNativeStatusbarBackground = GetBgColor("#statusbar");
  const SkColor kColorNativeTabBackgroundChecked =
      GetBgColor("GtkNotebook#notebook #tab:checked");
  const SkColor kColorNativeTabBackgroundCheckedFocused =
      GetBgColor("GtkNotebook#notebook:focus #tab:checked");
  const SkColor kColorNativeTextareaBackground =
      GetBgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view"
                                        : "GtkTextView.view");
  const SkColor kColorNativeTextareaBackgroundDisabled =
      GetBgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:disabled"
                                        : "GtkTextView.view:disabled");
  const SkColor kColorNativeTextareaBackgroundSelected = GetSelectionBgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text #selection"
                             : "GtkTextView.view:selected");
  const SkColor kColorNativeTextareaForeground =
      GetFgColor(GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text"
                                        : "GtkTextView.view");
  const SkColor kColorNativeTextareaForegroundDisabled = GetFgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view:disabled #text"
                             : "GtkTextView.view:disabled");
  const SkColor kColorNativeTextareaForegroundSelected = GetFgColor(
      GtkCheckVersion(3, 20) ? "GtkTextView#textview.view #text #selection"
                             : "GtkTextView.view:selected");
  const SkColor kColorNativeTextfieldBorderUnfocused =
      GetBorderColor("GtkEntry#entry");
  const SkColor kColorNativeTextfieldBorderFocused =
      GetBorderColor("GtkEntry#entry:focus");
  const SkColor kColorNativeTextfieldForegroundPlaceholder =
      GtkCheckVersion(4)
          ? GetFgColor("GtkEntry#entry #text #placeholder")
          : GtkStyleContextLookupColor(GetStyleContextFromCss("GtkEntry#entry"),
                                       "placeholder_text_color")
                // This is copied from gtkentry.c.  GTK uses a fallback of 50%
                // gray when the theme doesn't provide a placeholder color.
                .value_or(SkColorSetRGB(127, 127, 127));
  const SkColor kColorNativeToggleButtonBackgroundChecked =
      GetBgColor("GtkButton#button.text-button.toggle:checked");
  const SkColor kColorNativeToggleButtonBackgroundUnchecked =
      GetBgColor("GtkButton#button.text-button.toggle");
  SkColor toolbar_color =
      color_utils::GetResultingPaintColor(GetBgColor(""), frame_color);
  const auto tooltip_context = AppendCssNodeToStyleContext(
      {}, GtkCheckVersion(3, 20) ? "#tooltip.background"
                                 : "GtkWindow#window.background.tooltip");
  const SkColor kColorNativeTooltipBackground =
      GetBgColorFromStyleContext(tooltip_context);
  const SkColor kColorNativeTooltipForeground = GtkStyleContextGetColor(
      AppendCssNodeToStyleContext(tooltip_context, "GtkLabel#label"));
  const SkColor kColorNativeTreeHeaderBackground =
      GetBgColor("GtkTreeView#treeview.view GtkButton#button");
  const SkColor kColorNativeTreeHeaderBorder =
      GetBorderColor("GtkTreeView#treeview.view GtkButton#button");
  const SkColor kColorNativeTreeHeaderForeground =
      GetFgColor("GtkTreeView#treeview.view GtkButton#button GtkLabel#label");
  const SkColor kColorNativeTreeNodeBackground =
      GetBgColor("GtkTreeView#treeview.view GtkTreeView#treeview.view.cell");
  const SkColor kColorNativeTreeNodeBackgroundSelected = GetBgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected");
  const SkColor kColorNativeTreeNodeBackgroundSelectedFocused = GetBgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus");
  const SkColor kColorNativeTreeNodeForeground = GetFgColor(
      "GtkTreeView#treeview.view GtkTreeView#treeview.view.cell "
      "GtkLabel#label");
  const SkColor kColorNativeTreeNodeForegroundSelected = GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected GtkLabel#label");
  const SkColor kColorNativeTreeNodeForegroundSelectedFocused = GetFgColor(
      "GtkTreeView#treeview.view "
      "GtkTreeView#treeview.view.cell:selected:focus GtkLabel#label");
  const SkColor kColorNativeWindowBackground = GetBgColor("");

  // Core colors
  mixer[ui::kColorAccent] = {kColorNativeTreeNodeBackgroundSelectedFocused};
  mixer[ui::kColorAlertHighSeverity] = {SelectBasedOnDarkInput(
      ui::kColorPrimaryBackground, gfx::kGoogleRed300, gfx::kGoogleRed600)};
  mixer[ui::kColorAlertLowSeverity] = {SelectBasedOnDarkInput(
      ui::kColorPrimaryBackground, gfx::kGoogleGreen300, gfx::kGoogleGreen700)};
  mixer[ui::kColorAlertMediumSeverity] = {
      SelectBasedOnDarkInput(ui::kColorPrimaryBackground, gfx::kGoogleYellow300,
                             gfx::kGoogleYellow700)};
  mixer[ui::kColorDisabledForeground] = {kColorNativeLabelForegroundDisabled};
  mixer[ui::kColorItemHighlight] = {kColorNativeTextfieldBorderFocused};
  mixer[ui::kColorItemSelectionBackground] = {ui::kColorAccent};
  mixer[ui::kColorMenuSelectionBackground] = {
      kColorNativeMenuItemBackgroundHovered};
  mixer[ui::kColorMidground] = {kColorNativeSeparator};
  mixer[ui::kColorPrimaryBackground] = {kColorNativeWindowBackground};
  mixer[ui::kColorPrimaryForeground] = {kColorNativeLabelForeground};
  mixer[ui::kColorSecondaryForeground] = {kColorNativeLabelForegroundDisabled};
  mixer[ui::kColorTextSelectionBackground] = {
      kColorNativeLabelBackgroundSelected};
  mixer[ui::kColorTextSelectionForeground] = {
      kColorNativeLabelForegroundSelected};

  // UI element colors
  mixer[ui::kColorAvatarHeaderArt] =
      AlphaBlend(ui::kColorPrimaryForeground, ui::kColorPrimaryBackground,
                 gfx::kGoogleGreyAlpha300);
  mixer[ui::kColorAvatarIconGuest] =
      DeriveDefaultIconColor(ui::kColorPrimaryForeground);
  mixer[ui::kColorButtonBackground] = {kColorNativeButtonBackground};
  mixer[ui::kColorButtonBackgroundProminentDisabled] = {
      kColorNativeButtonBackgroundDisabled};
  mixer[ui::kColorButtonBorder] = {kColorNativeButtonBorder};
  mixer[ui::kColorButtonBorderDisabled] = {
      kColorNativeButtonBackgroundDisabled};
  mixer[ui::kColorButtonForeground] = {kColorNativeButtonForeground};
  mixer[ui::kColorButtonForegroundChecked] = {ui::kColorAccent};
  mixer[ui::kColorButtonForegroundDisabled] = {
      kColorNativeButtonForegroundDisabled};
  mixer[ui::kColorButtonForegroundProminent] = {
      kColorNativeTreeNodeForegroundSelectedFocused};
  mixer[ui::kColorButtonForegroundUnchecked] = {ui::kColorButtonForeground};
  mixer[ui::kColorDialogForeground] = {ui::kColorPrimaryForeground};
  mixer[ui::kColorDropdownBackground] = {kColorNativeComboboxBackground};
  mixer[ui::kColorDropdownBackgroundSelected] = {
      kColorNativeComboboxBackgroundHovered};
  mixer[ui::kColorDropdownForeground] = {kColorNativeComboboxForeground};
  mixer[ui::kColorDropdownForegroundSelected] = {
      kColorNativeComboboxForegroundHovered};
  mixer[ui::kColorFrameActive] = {kColorNativeFrameActive};
  mixer[ui::kColorFrameInactive] = {kColorNativeFrameInactive};
  mixer[ui::kColorFocusableBorderUnfocused] = {
      kColorNativeTextfieldBorderUnfocused};
  mixer[ui::kColorHelpIconActive] = {kColorNativeImageButtonForegroundHovered};
  mixer[ui::kColorIcon] = {kColorNativeButtonIcon};
  mixer[ui::kColorHelpIconInactive] = {kColorNativeImageButtonForeground};
  mixer[ui::kColorLinkForeground] = {link};
  mixer[ui::kColorLinkForegroundDisabled] = {link_disabled};
  mixer[ui::kColorLinkForegroundPressed] = {link_hovered};
  mixer[ui::kColorMenuBackground] = {kColorNativeMenuBackground};
  mixer[ui::kColorMenuBorder] = {kColorNativeMenuBorder};
  mixer[ui::kColorMenuDropmarker] = {ui::kColorMenuItemForeground};
  mixer[ui::kColorMenuIcon] = {kColorNativeMenuRadio};
  mixer[ui::kColorMenuItemBackgroundHighlighted] = {ui::kColorMenuBackground};
  mixer[ui::kColorMenuItemForeground] = {kColorNativeMenuItemForeground};
  mixer[ui::kColorMenuItemForegroundHighlighted] = {
      ui::kColorMenuItemForeground};
  mixer[ui::kColorMenuItemForegroundDisabled] = {
      kColorNativeMenuItemForegroundDisabled};
  mixer[ui::kColorMenuItemForegroundSecondary] = {
      kColorNativeMenuItemAccelerator};
  mixer[ui::kColorMenuItemForegroundSelected] = {
      kColorNativeMenuItemForegroundHovered};
  mixer[ui::kColorMenuSeparator] = {kColorNativeMenuSeparator};
  mixer[ui::kColorNotificationInputForeground] = {
      kColorNativeTreeNodeForegroundSelectedFocused};
  mixer[ui::kColorOverlayScrollbarFill] = {
      kColorNativeScrollbarSliderBackground};
  mixer[ui::kColorOverlayScrollbarFillHovered] = {
      kColorNativeScrollbarSliderBackgroundHovered};
  mixer[ui::kColorOverlayScrollbarStroke] = {
      kColorNativeScrollbarTroughBackground};
  mixer[ui::kColorOverlayScrollbarStrokeHovered] = {
      kColorNativeScrollbarTroughBackgroundHovered};
  mixer[ui::kColorSliderThumb] = {kColorNativeScaleHighlightBackground};
  mixer[ui::kColorSliderThumbMinimal] = {
      kColorNativeScaleHighlightBackgroundDisabled};
  mixer[ui::kColorSliderTrack] = {kColorNativeScaleTroughBackground};
  mixer[ui::kColorSliderTrackMinimal] = {
      kColorNativeScaleTroughBackgroundDisabled};
  mixer[ui::kColorSyncInfoBackground] = {kColorNativeStatusbarBackground};
  mixer[ui::kColorTabBackgroundHighlighted] = {
      kColorNativeTabBackgroundChecked};
  mixer[ui::kColorTabBackgroundHighlightedFocused] = {
      kColorNativeTabBackgroundCheckedFocused};
  mixer[ui::kColorTabContentSeparator] = {kColorNativeFrameBorder};
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
  mixer[ui::kColorTableHeaderBackground] = {kColorNativeTreeHeaderBackground};
  mixer[ui::kColorTableHeaderForeground] = {kColorNativeTreeHeaderForeground};
  mixer[ui::kColorTableHeaderSeparator] = {kColorNativeTreeHeaderBorder};
  mixer[ui::kColorTextfieldBackground] = {kColorNativeTextareaBackground};
  mixer[ui::kColorTextfieldBackgroundDisabled] = {
      kColorNativeTextareaBackgroundDisabled};
  mixer[ui::kColorTextfieldForeground] = {kColorNativeTextareaForeground};
  mixer[ui::kColorTextfieldForegroundDisabled] = {
      kColorNativeTextareaForegroundDisabled};
  mixer[ui::kColorTextfieldForegroundPlaceholder] = {
      kColorNativeTextfieldForegroundPlaceholder};
  mixer[ui::kColorTextfieldSelectionBackground] = {
      kColorNativeTextareaBackgroundSelected};
  mixer[ui::kColorTextfieldSelectionForeground] = {
      kColorNativeTextareaForegroundSelected};
  mixer[ui::kColorThrobber] = {kColorNativeSpinner};
  mixer[ui::kColorThrobberPreconnect] = {kColorNativeSpinnerDisabled};
  mixer[ui::kColorToggleButtonTrackOff] = {
      kColorNativeToggleButtonBackgroundUnchecked};
  mixer[ui::kColorToggleButtonTrackOn] = {
      kColorNativeToggleButtonBackgroundChecked};
  mixer[ui::kColorTooltipBackground] = {kColorNativeTooltipBackground};
  mixer[ui::kColorTooltipForeground] = {kColorNativeTooltipForeground};
  mixer[ui::kColorTreeBackground] = {kColorNativeTreeNodeBackground};
  mixer[ui::kColorTreeNodeForeground] = {kColorNativeTreeNodeForeground};
  mixer[ui::kColorTreeNodeForegroundSelectedFocused] = {
      kColorNativeTreeNodeForegroundSelectedFocused};
  mixer[ui::kColorTreeNodeBackgroundSelectedUnfocused] = {
      kColorNativeTreeNodeBackgroundSelected};
  mixer[ui::kColorTreeNodeForegroundSelectedUnfocused] = {
      kColorNativeTreeNodeForegroundSelected};

  // Platform-specific UI elements
  mixer[ui::kColorNativeHeaderButtonBorderActive] = {
      GetBorderColor(header_selector + " GtkButton#button")};
  mixer[ui::kColorNativeHeaderButtonBorderInactive] = {
      GetBorderColor(header_selector + ":backdrop GtkButton#button")};
  mixer[ui::kColorNativeHeaderSeparatorBorderActive] = {GetBorderColor(
      header_selector + " GtkSeparator#separator.vertical.titlebutton")};
  mixer[ui::kColorNativeHeaderSeparatorBorderInactive] = {
      GetBorderColor(header_selector +
                     ":backdrop GtkSeparator#separator.vertical.titlebutton")};
  mixer[ui::kColorNativeTabForegroundInactiveFrameActive] = {
      GetFgColor(header_selector + " GtkLabel#label.title")};
  mixer[ui::kColorNativeTabForegroundInactiveFrameInactive] = {
      GetFgColor(header_selector_inactive + " GtkLabel#label.title")};
  mixer[ui::kColorNativeToolbarBackground] = {toolbar_color};
  mixer[ui::kColorNativeTextfieldBorderUnfocused] = {
      kColorNativeTextfieldBorderUnfocused};
  mixer[ui::kColorNativeButtonBorder] = {kColorNativeButtonBorder};
  mixer[ui::kColorNativeLabelForeground] = {kColorNativeLabelForeground};
}

}  // namespace gtk
