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
  mixer[kColorAvatarHeaderArt] = {kColorMidground};
  mixer[kColorAvatarIconGuest] = {kColorSecondaryForeground};
  mixer[kColorAvatarIconIncognito] = {kColorPrimaryForeground};
  mixer[kColorBubbleBackground] = {kColorPrimaryBackground};
  mixer[kColorBubbleFooterBackground] = {kColorSubtleEmphasisBackground};
  mixer[kColorButtonBackground] = {kColorPrimaryBackground};
  mixer[kColorButtonBackgroundPressed] = {kColorButtonBackground};
  mixer[kColorButtonBackgroundProminent] = {kColorAccent};
  mixer[kColorButtonBackgroundProminentDisabled] = {
      kColorSubtleEmphasisBackground};
  mixer[kColorButtonBackgroundProminentFocused] =
      BlendForMinContrastWithSelf(kColorButtonBackgroundProminent, 1.3f);
  mixer[kColorButtonBorder] = {kColorMidground};
  mixer[kColorButtonBorderDisabled] = {kColorSubtleEmphasisBackground};
  mixer[kColorButtonForeground] = {kColorAccent};
  mixer[kColorButtonForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorButtonForegroundProminent] =
      GetColorWithMaxContrast(kColorButtonBackgroundProminent);
  mixer[kColorButtonForegroundUnchecked] = {kColorSecondaryForeground};
  mixer[kColorDialogBackground] = {kColorPrimaryBackground};
  mixer[kColorDialogForeground] = {kColorSecondaryForeground};
  mixer[kColorFocusableBorderFocused] = SetAlpha(kColorAccent, 0x4D);
  mixer[kColorFocusableBorderUnfocused] = {kColorMidground};
  mixer[kColorIcon] = {kColorSecondaryForeground};
  mixer[kColorLabelForeground] = {kColorPrimaryForeground};
  mixer[kColorLabelForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorLabelForegroundSecondary] = {kColorSecondaryForeground};
  mixer[kColorLabelSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorLabelSelectionForeground] = {kColorTextSelectionForeground};
  mixer[kColorLinkForeground] = {kColorAccent};
  mixer[kColorLinkForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorLinkForegroundPressed] = {kColorLinkForeground};
  mixer[kColorMenuBackground] = {kColorPrimaryBackground};
  mixer[kColorMenuBorder] = {kColorMidground};
  mixer[kColorMenuIcon] = {kColorIcon};
  mixer[kColorMenuItemBackgroundAlertedInitial] = SetAlpha(kColorAccent, 0x4D);
  mixer[kColorMenuItemBackgroundAlertedTarget] = SetAlpha(kColorAccent, 0x1A);
  mixer[kColorMenuItemBackgroundHighlighted] = {kColorSubtleEmphasisBackground};
  mixer[kColorMenuItemBackgroundSelected] = {kColorItemSelectionBackground};
  mixer[kColorMenuItemForeground] = {kColorPrimaryForeground};
  mixer[kColorMenuItemForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorMenuItemForegroundHighlighted] = {kColorMenuItemForeground};
  mixer[kColorMenuItemForegroundSecondary] = {kColorSecondaryForeground};
  mixer[kColorMenuItemForegroundSelected] = {kColorMenuItemForeground};
  mixer[kColorMenuSeparator] = {kColorMidground};
  mixer[kColorTabBorderSelected] = {kColorAccent};
  mixer[kColorTabContentSeparator] = {kColorMidground};
  mixer[kColorTabForeground] = {kColorSecondaryForeground};
  mixer[kColorTabForegroundSelected] = {kColorAccent};
  mixer[kColorTableBackground] = {kColorPrimaryBackground};
  mixer[kColorTableBackgroundSelectedFocused] = {kColorItemSelectionBackground};
  mixer[kColorTableBackgroundSelectedUnfocused] = {
      kColorTableBackgroundSelectedFocused};
  mixer[kColorTableForeground] = {kColorPrimaryForeground};
  mixer[kColorTableForegroundSelectedFocused] = {kColorTableForeground};
  mixer[kColorTableForegroundSelectedUnfocused] = {
      kColorTableForegroundSelectedFocused};
  mixer[kColorTableGroupingIndicator] = {kColorTableBackgroundSelectedFocused};
  mixer[kColorTableHeaderBackground] = {kColorTableBackground};
  mixer[kColorTableHeaderForeground] = {kColorTableForeground};
  mixer[kColorTableHeaderSeparator] = {kColorMidground};
  mixer[kColorTextfieldBackground] =
      GetColorWithMaxContrast(kColorTextfieldForeground);
  mixer[kColorTextfieldBackgroundDisabled] = {kColorPrimaryBackground};
  mixer[kColorTextfieldForeground] = {kColorPrimaryForeground};
  mixer[kColorTextfieldForegroundDisabled] = {kColorDisabledForeground};
  mixer[kColorTextfieldForegroundPlaceholder] = {
      kColorTextfieldForegroundDisabled};
  mixer[kColorTextfieldSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorTextfieldSelectionForeground] = {kColorTextSelectionForeground};
  mixer[kColorThrobber] = {kColorAccent};
  mixer[kColorTooltipBackground] = SetAlpha(kColorPrimaryBackground, 0xCC);
  mixer[kColorTooltipForeground] = SetAlpha(kColorPrimaryForeground, 0xDE);
  mixer[kColorTreeBackground] = {kColorPrimaryBackground};
  mixer[kColorTreeNodeBackgroundSelectedFocused] = {
      kColorItemSelectionBackground};
  mixer[kColorTreeNodeBackgroundSelectedUnfocused] = {
      kColorTreeNodeBackgroundSelectedFocused};
  mixer[kColorTreeNodeForeground] = {kColorPrimaryForeground};
  mixer[kColorTreeNodeForegroundSelectedFocused] = {kColorTreeNodeForeground};
  mixer[kColorTreeNodeForegroundSelectedUnfocused] = {
      kColorTreeNodeForegroundSelectedFocused};
  mixer[kColorWindowBackground] = {kColorPrimaryBackground};
}

}  // namespace ui
