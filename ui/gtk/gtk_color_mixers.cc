// Copyright 2021 The Chromium Authors
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
                            const ui::ColorProviderKey& key,
                            std::optional<SkColor> accent_color) {
  if (key.system_theme != ui::SystemTheme::kGtk) {
    return;
  }

  ui::ColorMixer& mixer = provider->AddMixer();

  const std::string header_selector =
      key.frame_type == ui::ColorProviderKey::FrameType::kChromium
          ? "headerbar.header-bar.titlebar"
          : "menubar";
  const std::string header_selector_inactive = header_selector + ":backdrop";
  const auto tooltip_context =
      AppendCssNodeToStyleContext({}, "tooltip.background");

  const SkColor primary_bg = GetBgColor("");
  const SkColor button_bg_disabled = GetBgColor("button.text-button:disabled");
  const SkColor button_border = GetBorderColor("button");
  const SkColor frame_color =
      SkColorSetA(GetBgColor(header_selector), SK_AlphaOPAQUE);
  const SkColor frame_color_inactive =
      SkColorSetA(GetBgColor(header_selector_inactive), SK_AlphaOPAQUE);
  const SkColor label_fg = GetFgColor("label");
  const SkColor label_fg_disabled = GetFgColor("label:disabled");
  const SkColor entry_border = GetBorderColor("entry");
  const SkColor toolbar_color =
      color_utils::GetResultingPaintColor(primary_bg, frame_color);
  const SkColor accent_fg = GetFgColor(
      "treeview.view "
      "treeview.view.cell:selected:focus label");
  const SkColor accent_bg =
      accent_color.value_or(GetBgColor("treeview.view "
                                       "treeview.view.cell:selected:focus"));

  // Core colors
  mixer[ui::kColorAccent] = {accent_bg};
  mixer[ui::kColorAlertHighSeverity] = {SelectBasedOnDarkInput(
      ui::kColorPrimaryBackground, gfx::kGoogleRed300, gfx::kGoogleRed600)};
  mixer[ui::kColorAlertLowSeverity] = {SelectBasedOnDarkInput(
      ui::kColorPrimaryBackground, gfx::kGoogleGreen300, gfx::kGoogleGreen700)};
  mixer[ui::kColorAlertMediumSeverityIcon] = {
      SelectBasedOnDarkInput(ui::kColorPrimaryBackground, gfx::kGoogleYellow300,
                             gfx::kGoogleYellow700)};
  mixer[ui::kColorAlertMediumSeverityText] = {
      SelectBasedOnDarkInput(ui::kColorPrimaryBackground, gfx::kGoogleYellow300,
                             gfx::kGoogleOrange900)};
  mixer[ui::kColorDisabledForeground] = {label_fg_disabled};
  mixer[ui::kColorItemHighlight] = {GetBorderColor("entry:focus")};
  mixer[ui::kColorItemSelectionBackground] = {ui::kColorAccent};
  mixer[ui::kColorMenuSelectionBackground] = {GetBgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ":hover"}))};
  mixer[ui::kColorMidground] = {GetSeparatorColor("separator.horizontal")};
  mixer[ui::kColorPrimaryBackground] = {primary_bg};
  mixer[ui::kColorPrimaryForeground] = {label_fg};
  mixer[ui::kColorSecondaryForeground] = {label_fg_disabled};
  mixer[ui::kColorTextSelectionBackground] = {
      GetSelectionBgColor("label selection")};
  mixer[ui::kColorTextSelectionForeground] = {GetFgColor("label selection")};

  // UI element colors
  mixer[ui::kColorAvatarHeaderArt] =
      AlphaBlend(ui::kColorPrimaryForeground, ui::kColorPrimaryBackground,
                 gfx::kGoogleGreyAlpha300);
  mixer[ui::kColorAvatarIconGuest] =
      DeriveDefaultIconColor(ui::kColorPrimaryForeground);
  mixer[ui::kColorBubbleBackground] = {ui::kColorPrimaryBackground};
  mixer[ui::kColorBubbleFooterBackground] = {ui::kColorBubbleBackground};
  mixer[ui::kColorButtonBackground] = {GetBgColor("button")};
  mixer[ui::kColorButtonBackgroundProminent] =
      PickGoogleColor(ui::kColorAccent, ui::kColorDialogBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[ui::kColorButtonBackgroundProminentFocused] = {
      ui::kColorButtonBackgroundProminent};
  mixer[ui::kColorButtonBackgroundProminentDisabled] = {button_bg_disabled};
  mixer[ui::kColorButtonBorder] = {button_border};
  mixer[ui::kColorButtonBorderDisabled] = {button_bg_disabled};
  mixer[ui::kColorButtonForeground] = {GetFgColor("button.text-button label")};
  mixer[ui::kColorButtonForegroundDisabled] = {
      GetFgColor("button.text-button:disabled label")};
  mixer[ui::kColorButtonForegroundProminent] = {accent_fg};
  mixer[ui::kColorDialogForeground] = {ui::kColorPrimaryForeground};
  mixer[ui::kColorDropdownBackground] = {GetBgColor(base::StrCat(
      {"combobox window.background.popup ", "menu(gtk-combobox-popup-menu) ",
       GtkCssMenuItem(), " ", "cellview"}))};
  mixer[ui::kColorDropdownBackgroundSelected] = {GetBgColor(base::StrCat(
      {"combobox window.background.popup ", "menu(gtk-combobox-popup-menu) ",
       GtkCssMenuItem(), ":hover cellview"}))};
  mixer[ui::kColorDropdownForeground] = {GetFgColor(base::StrCat(
      {"combobox window.background.popup ", "menu(gtk-combobox-popup-menu) ",
       GtkCssMenuItem(), " ", "cellview"}))};
  mixer[ui::kColorDropdownForegroundSelected] = {GetFgColor(base::StrCat(
      {"combobox window.background.popup ", "menu(gtk-combobox-popup-menu) ",
       GtkCssMenuItem(), ":hover cellview"}))};
  mixer[ui::kColorFrameActive] = {frame_color};
  mixer[ui::kColorFrameInactive] = {frame_color_inactive};
  mixer[ui::kColorFocusableBorderUnfocused] = {entry_border};
  mixer[ui::kColorHelpIconActive] = {GetFgColor("button.image-button:hover")};
  mixer[ui::kColorIcon] = {GetFgColor("button.flat.scale image")};
  mixer[ui::kColorHelpIconInactive] = {GetFgColor("button.image-button")};
  mixer[ui::kColorLinkForegroundDefault] = {GetFgColor("label.link:link")};
  mixer[ui::kColorLinkForegroundDisabled] = {
      GetFgColor("label.link:link:disabled")};
  mixer[ui::kColorLinkForegroundPressedDefault] = {
      GetFgColor("label.link:link:hover:active")};
  mixer[ui::kColorMenuBackground] = {GetBgColor(GtkCssMenu())};
  mixer[ui::kColorMenuBorder] = {GetBorderColor(GtkCssMenu())};
  mixer[ui::kColorMenuDropmarker] = {ui::kColorMenuItemForeground};
  mixer[ui::kColorMenuIcon] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), " radio"}))};
  mixer[ui::kColorMenuItemBackgroundHighlighted] = {ui::kColorMenuBackground};
  mixer[ui::kColorMenuItemForeground] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), " label"}))};
  mixer[ui::kColorMenuItemForegroundHighlighted] = {
      ui::kColorMenuItemForeground};
  mixer[ui::kColorMenuItemForegroundDisabled] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ":disabled label"}))};
  mixer[ui::kColorMenuItemForegroundSecondary] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), " accelerator"}))};
  mixer[ui::kColorMenuItemForegroundSelected] = {GetFgColor(
      base::StrCat({GtkCssMenu(), " ", GtkCssMenuItem(), ":hover label"}))};
  mixer[ui::kColorMenuSeparator] = {
      GetSeparatorColor(base::StrCat({GtkCssMenu(), " separator.horizontal"}))};
  mixer[ui::kColorNotificationInputForeground] = {accent_fg};
  mixer[ui::kColorOverlayScrollbarFill] = {GetBgColor("scrollbar slider")};
  mixer[ui::kColorOverlayScrollbarFillHovered] = {
      GetBgColor("scrollbar slider:hover")};
  mixer[ui::kColorOverlayScrollbarStroke] = {GetBgColor("scrollbar trough")};
  mixer[ui::kColorOverlayScrollbarStrokeHovered] = {
      GetBgColor("scrollbar trough:hover")};
  mixer[ui::kColorRadioButtonForegroundChecked] = {ui::kColorAccent};
  mixer[ui::kColorRadioButtonForegroundUnchecked] = {
      ui::kColorButtonForeground};
  mixer[ui::kColorSliderThumb] = {GetBgColor("scale highlight")};
  mixer[ui::kColorSliderThumbMinimal] = {
      GetBgColor("scale:disabled highlight")};
  mixer[ui::kColorSliderTrack] = {GetBgColor("scale trough")};
  mixer[ui::kColorSliderTrackMinimal] = {GetBgColor("scale:disabled trough")};
  mixer[ui::kColorSyncInfoBackground] = {GetBgColor("statusbar")};
  mixer[ui::kColorTabBackgroundHighlighted] = {
      GetBgColor("notebook tab:checked")};
  mixer[ui::kColorTabBackgroundHighlightedFocused] = {
      GetBgColor("notebook:focus tab:checked")};
  mixer[ui::kColorTabContentSeparator] = {GetBorderColor("frame border")};
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
  mixer[ui::kColorTableHeaderBackground] = {GetBgColor("treeview.view button")};
  mixer[ui::kColorTableHeaderForeground] = {
      GetFgColor("treeview.view button label")};
  mixer[ui::kColorTableHeaderSeparator] = {
      GetBorderColor("treeview.view button")};
  mixer[ui::kColorTextfieldBackground] = {GetBgColor("textview.view")};
  mixer[ui::kColorTextfieldBackgroundDisabled] = {
      GetBgColor("textview.view:disabled")};
  mixer[ui::kColorTextfieldForeground] = {GetFgColor("textview.view text")};
  mixer[ui::kColorTextfieldForegroundDisabled] = {
      GetFgColor("textview.view:disabled text")};
  mixer[ui::kColorTextfieldForegroundPlaceholder] = {GtkCheckVersion(4)};
  mixer[ui::kColorTextfieldSelectionBackground] = {
      GetSelectionBgColor("textview.view text selection")};
  mixer[ui::kColorTextfieldSelectionForeground] = {
      GetFgColor("textview.view text selection")};
  mixer[ui::kColorThrobber] = {GetFgColor("spinner")};
  mixer[ui::kColorThrobberPreconnect] = {GetFgColor("spinner:disabled")};
  mixer[ui::kColorToggleButtonTrackOff] = {
      GetBgColor("button.text-button.toggle")};
  mixer[ui::kColorToggleButtonTrackOn] = {
      GetBgColor("button.text-button.toggle:checked")};
  mixer[ui::kColorTooltipBackground] = {
      GetBgColorFromStyleContext(tooltip_context)};
  mixer[ui::kColorTooltipForeground] = {GtkStyleContextGetColor(
      AppendCssNodeToStyleContext(tooltip_context, "label"))};
  mixer[ui::kColorTreeBackground] = {
      GetBgColor("treeview.view treeview.view.cell")};
  mixer[ui::kColorTreeNodeForeground] = {
      GetFgColor("treeview.view treeview.view.cell "
                 "label")};
  mixer[ui::kColorTreeNodeForegroundSelectedFocused] = {accent_fg};
  mixer[ui::kColorTreeNodeBackgroundSelectedUnfocused] = {
      GetBgColor("treeview.view "
                 "treeview.view.cell:selected")};
  mixer[ui::kColorTreeNodeForegroundSelectedUnfocused] = {
      GetFgColor("treeview.view "
                 "treeview.view.cell:selected label")};

  // Platform-specific UI elements
  mixer[ui::kColorNativeHeaderButtonBorderActive] = {
      GetBorderColor(header_selector + " button")};
  mixer[ui::kColorNativeHeaderButtonBorderInactive] = {
      GetBorderColor(header_selector + ":backdrop button")};
  mixer[ui::kColorNativeHeaderSeparatorBorderActive] = {
      GetBorderColor(header_selector + " separator.vertical.titlebutton")};
  mixer[ui::kColorNativeHeaderSeparatorBorderInactive] = {GetBorderColor(
      header_selector + ":backdrop separator.vertical.titlebutton")};
  mixer[ui::kColorNativeTabForegroundInactiveFrameActive] = {
      GetFgColor(header_selector + " label.title")};
  mixer[ui::kColorNativeTabForegroundInactiveFrameInactive] = {
      GetFgColor(header_selector_inactive + " label.title")};
  mixer[ui::kColorNativeToolbarBackground] = {toolbar_color};
  mixer[ui::kColorNativeTextfieldBorderUnfocused] = {entry_border};
  mixer[ui::kColorNativeButtonBorder] = {button_border};
  mixer[ui::kColorNativeLabelForeground] = {label_fg};
}

}  // namespace gtk
