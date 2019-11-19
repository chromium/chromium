// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"

namespace ui {

void AddUiColorMixers(ColorProvider* provider) {
  ColorMixer& mixer = provider->AddMixer();

  mixer[kColorBubbleBackground] = {kColorPrimaryBackground};
  mixer[kColorBubbleFooterBackground] = {kColorSecondaryBackgroundSubtle};
  mixer[kColorButtonBackground] = {kColorPrimaryBackground};
  mixer[kColorButtonBorder] = {kColorSecondaryBackground};
  mixer[kColorButtonDisabledForeground] = {kColorSecondaryForeground};
  mixer[kColorButtonForeground] = {kColorAccent};
  mixer[kColorButtonProminentBackground] = {kColorAccent};
  mixer[kColorButtonProminentDisabledBackground] =
      AlphaBlend(kColorSecondaryBackground, kColorButtonBackground,
                 gfx::kDisabledControlAlpha);
  mixer[kColorButtonProminentFocusedBackground] =
      BlendForMinContrastWithSelf(kColorButtonProminentBackground, 1.3f);
  mixer[kColorButtonProminentForeground] =
      GetColorWithMaxContrast(kColorButtonProminentBackground);
  mixer[kColorDialogBackground] = {kColorPrimaryBackground};
  mixer[kColorDialogForeground] = {kColorBodyForeground};
  mixer[kColorFocusableBorderFocused] = SetAlpha(kColorAccent, 0x4D);
  mixer[kColorFocusableBorderUnfocused] = {kColorSecondaryBackground};
  mixer[kColorIcon] = {kColorBodyForeground};
  mixer[kColorLabelDisabledForeground] =
      SetAlpha(kColorLabelForeground, gfx::kDisabledControlAlpha);
  mixer[kColorLabelForeground] = {kColorPrimaryForeground};
  mixer[kColorLabelSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorLabelSelectionForeground] = {kColorLabelForeground};
  mixer[kColorLinkDisabledForeground] = {kColorPrimaryForeground};
  mixer[kColorLinkPressedForeground] = {kColorLinkForeground};
  mixer[kColorMenuBackground] = {kColorPrimaryBackground};
  mixer[kColorMenuBorder] = {kColorSecondaryBackground};
  mixer[kColorMenuItemAlertedBackground] = {kColorAccent};
  mixer[kColorMenuItemDisabledForeground] = {kColorSecondaryForeground};
  mixer[kColorMenuItemForeground] = {kColorPrimaryForeground};
  mixer[kColorMenuItemHighlightedBackground] = {
      kColorSecondaryBackgroundSubtle};
  mixer[kColorMenuItemHighlightedForeground] = {kColorMenuItemForeground};
  mixer[kColorMenuItemSecondaryForeground] = {kColorSecondaryForeground};
  mixer[kColorMenuItemSelectedBackground] = {kColorSecondaryBackground};
  mixer[kColorMenuItemSelectedForeground] = {kColorMenuItemForeground};
  mixer[kColorMenuSeparator] = {kColorSeparatorForeground};
  mixer[kColorTabContentSeparator] = {kColorSecondaryBackground};
  mixer[kColorTabForeground] = {kColorSecondaryForeground};
  mixer[kColorTabSelectedForeground] = {kColorAccent};
  mixer[kColorTableBackground] = {kColorPrimaryBackground};
  mixer[kColorTableForeground] = {kColorPrimaryForeground};
  mixer[kColorTableGroupingIndicator] = {kColorTableSelectedFocusedBackground};
  mixer[kColorTableHeaderBackground] = {kColorTableBackground};
  mixer[kColorTableHeaderForeground] = {kColorTableForeground};
  mixer[kColorTableHeaderSeparator] = {kColorSeparatorForeground};
  mixer[kColorTableSelectedFocusedBackground] = {kColorSecondaryBackground};
  mixer[kColorTableSelectedFocusedForeground] = {kColorTableForeground};
  mixer[kColorTableSelectedUnfocusedBackground] = {
      kColorTableSelectedFocusedBackground};
  mixer[kColorTableSelectedUnfocusedForeground] = {
      kColorTableSelectedFocusedForeground};
  mixer[kColorTextfieldBackground] = {kColorPrimaryBackground};
  mixer[kColorTextfieldDisabledBackground] = {kColorTextfieldBackground};
  mixer[kColorTextfieldDisabledForeground] =
      SetAlpha(kColorTextfieldForeground, gfx::kDisabledControlAlpha);
  mixer[kColorTextfieldForeground] = {kColorPrimaryForeground};
  mixer[kColorTextfieldSelectionBackground] = {kColorTextSelectionBackground};
  mixer[kColorTextfieldSelectionForeground] = {kColorTextfieldForeground};
  mixer[kColorThrobber] = {kColorAccent};
  mixer[kColorTooltipBackground] =
      SetAlpha(GetColorWithMaxContrast(kColorPrimaryBackground), 0xE9);
  mixer[kColorTooltipForeground] =
      SetAlpha(GetColorWithMaxContrast(kColorTooltipBackground), 0xDE);
  mixer[kColorTreeBackground] = {kColorPrimaryBackground};
  mixer[kColorTreeNodeForeground] = {kColorPrimaryForeground};
  mixer[kColorTreeNodeSelectedFocusedBackground] = {kColorSecondaryBackground};
  mixer[kColorTreeNodeSelectedFocusedForeground] = {kColorTreeNodeForeground};
  mixer[kColorTreeNodeSelectedUnfocusedBackground] = {
      kColorTreeNodeSelectedFocusedBackground};
  mixer[kColorTreeNodeSelectedUnfocusedForeground] = {
      kColorTreeNodeSelectedFocusedForeground};
  mixer[kColorWindowBackground] = {kColorPrimaryBackground};
}

}  // namespace ui
