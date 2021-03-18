// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "build/build_config.h"
#include "ui/color/color_mixers.h"

#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

namespace ui {

void AddUiColorMixer(ColorProvider* provider) {
  ColorMixer& mixer = provider->AddMixer();
  const auto button_disabled_background =
      BlendForMinContrastWithSelf(kColorButtonBackground, 1.2f);

  mixer[kColorAvatarHeaderArt] = {kColorMidground};
  mixer[kColorAvatarIconGuest] = {kColorSecondaryForeground};
  mixer[kColorAvatarIconIncognito] = {kColorPrimaryForeground};
  mixer[kColorBubbleBackground] = {kColorPrimaryBackground};
  mixer[kColorBubbleFooterBackground] = {kColorSubtleEmphasisBackground};
  mixer[kColorButtonBackground] = {kColorPrimaryBackground};
  mixer[kColorButtonBorder] = {kColorMidground};
  mixer[kColorButtonBorderDisabled] = button_disabled_background;
  mixer[kColorButtonForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorButtonForeground] = {kColorAccent};
  mixer[kColorButtonBackgroundPressed] = {kColorButtonBackground};
  mixer[kColorButtonBackgroundProminent] = {kColorAccent};
  mixer[kColorButtonBackgroundProminentDisabled] = button_disabled_background;
  mixer[kColorButtonBackgroundProminentFocused] =
      BlendForMinContrastWithSelf(kColorButtonBackgroundProminent, 1.3f);
  mixer[kColorButtonForegroundProminent] =
      GetColorWithMaxContrast(kColorButtonBackgroundProminent);
  mixer[kColorButtonForegroundUnchecked] = {kColorSecondaryForeground};
  mixer[kColorDialogBackground] = {kColorPrimaryBackground};
  mixer[kColorDialogForeground] = {kColorSecondaryForeground};
  mixer[kColorFocusableBorderFocused] = SetAlpha(kColorAccent, 0x4D);
  mixer[kColorFocusableBorderUnfocused] = {kColorMidground};
  mixer[kColorIcon] = {kColorSecondaryForeground};
  mixer[kColorMenuIcon] = {kColorIcon};
  mixer[kColorLabelForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorLabelForeground] = {kColorPrimaryForeground};
  mixer[kColorLabelForegroundSecondary] = {kColorSecondaryForeground};
  mixer[kColorLabelSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorLabelSelectionForeground] = {kColorLabelForeground};
  mixer[kColorLinkForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorLinkForegroundPressed] = {kColorLinkForeground};
  mixer[kColorLinkForeground] = {kColorAccent};
  mixer[kColorMenuBackground] = {kColorPrimaryBackground};
  mixer[kColorMenuBorder] = {kColorMidground};
  mixer[kColorMenuItemBackgroundAlertedInitial] = SetAlpha(kColorAccent, 0x4D);
  mixer[kColorMenuItemBackgroundAlertedTarget] = SetAlpha(kColorAccent, 0x1A);
  mixer[kColorMenuItemForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorMenuItemForeground] = {kColorPrimaryForeground};
  mixer[kColorMenuItemBackgroundHighlighted] = {kColorSubtleEmphasisBackground};
  mixer[kColorMenuItemForegroundHighlighted] = {kColorMenuItemForeground};
  mixer[kColorMenuItemForegroundSecondary] = {kColorSecondaryForeground};
  mixer[kColorMenuItemBackgroundSelected] = {kColorItemSelectionBackground};
  mixer[kColorMenuItemForegroundSelected] = {kColorMenuItemForeground};
  mixer[kColorMenuSeparator] = {kColorMidground};
  mixer[kColorTabContentSeparator] = {kColorMidground};
  mixer[kColorTabForeground] = {kColorSecondaryForeground};
  mixer[kColorTabBorderSelected] = {kColorAccent};
  mixer[kColorTabForegroundSelected] = {kColorAccent};
  mixer[kColorTableBackground] = {kColorPrimaryBackground};
  mixer[kColorTableForeground] = {kColorPrimaryForeground};
  mixer[kColorTableGroupingIndicator] = {kColorTableBackgroundSelectedFocused};
  mixer[kColorTableHeaderBackground] = {kColorTableBackground};
  mixer[kColorTableHeaderForeground] = {kColorTableForeground};
  mixer[kColorTableHeaderSeparator] = {kColorMidground};
  mixer[kColorTableBackgroundSelectedFocused] = {kColorItemSelectionBackground};
  mixer[kColorTableForegroundSelectedFocused] = {kColorTableForeground};
  mixer[kColorTableBackgroundSelectedUnfocused] = {
      kColorTableBackgroundSelectedFocused};
  mixer[kColorTableForegroundSelectedUnfocused] = {
      kColorTableForegroundSelectedFocused};
  mixer[kColorTextfieldBackground] =
      GetColorWithMaxContrast(kColorTextfieldForeground);
  mixer[kColorTextfieldBackgroundDisabled] = {kColorPrimaryBackground};
  mixer[kColorTextfieldForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorTextfieldForegroundPlaceholder] = {
      kColorTextfieldForegroundDisabled};
  mixer[kColorTextfieldForeground] = {kColorPrimaryForeground};
  mixer[kColorTextfieldSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorTextfieldSelectionForeground] = {kColorTextfieldForeground};
  mixer[kColorThrobber] = {kColorAccent};
  mixer[kColorTooltipBackground] = SetAlpha(kColorPrimaryBackground, 0xCC);
  mixer[kColorTooltipForeground] = SetAlpha(kColorPrimaryForeground, 0xDE);
  mixer[kColorTreeBackground] = {kColorPrimaryBackground};
  mixer[kColorTreeNodeForeground] = {kColorPrimaryForeground};
  mixer[kColorTreeNodeBackgroundSelectedFocused] = {
      kColorItemSelectionBackground};
  mixer[kColorTreeNodeForegroundSelectedFocused] = {kColorTreeNodeForeground};
  mixer[kColorTreeNodeBackgroundSelectedUnfocused] = {
      kColorTreeNodeBackgroundSelectedFocused};
  mixer[kColorTreeNodeForegroundSelectedUnfocused] = {
      kColorTreeNodeForegroundSelectedFocused};
  mixer[kColorWindowBackground] = {kColorPrimaryBackground};
}

}  // namespace ui
