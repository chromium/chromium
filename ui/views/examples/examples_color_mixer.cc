// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_color_mixer.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/examples/examples_color_id.h"

namespace views::examples {

void AddExamplesColorMixers(ui::ColorProvider* color_provider,
                            const ui::ColorProviderKey& key) {
  const bool dark_mode =
      key.color_mode == ui::ColorProviderKey::ColorMode::kDark;

  using Ids = ExamplesColorIds;
  ui::ColorMixer& mixer = color_provider->AddMixer();
  mixer[Ids::kColorAnimatedImageViewExampleBorder] = {SK_ColorBLACK};
  mixer[Ids::kColorAnimationExampleForeground] = {SK_ColorBLACK};
  mixer[Ids::kColorAnimationExampleBackground] = {SK_ColorWHITE};
  mixer[Ids::kColorAccessibilityExampleBackground] = {SK_ColorWHITE};
  mixer[Ids::kColorBubbleExampleBackground1] = {SK_ColorWHITE};
  mixer[Ids::kColorBubbleExampleBackground2] = {SK_ColorGRAY};
  mixer[Ids::kColorBubbleExampleBackground3] = {SK_ColorCYAN};
  mixer[Ids::kColorBubbleExampleBackground4] = {
      SkColorSetRGB(0xC1, 0xB1, 0xE1)};
  mixer[Ids::kColorDesignerGrabHandle] = {gfx::kGoogleGrey500};
  mixer[Ids::kColorDesignerGrid] = {SK_ColorBLACK};
  mixer[Ids::kColorFadeAnimationExampleBorder] = {gfx::kGoogleGrey900};
  mixer[Ids::kColorFadeAnimationExampleBackground] = {SK_ColorWHITE};
  mixer[Ids::kColorFadeAnimationExampleForeground] = {gfx::kGoogleBlue800};
  mixer[Ids::kColorInkDropExampleBase] = {SK_ColorBLACK};
  mixer[Ids::kColorInkDropExampleBorder] = {SK_ColorBLACK};
  mixer[Ids::kColorLabelExampleBlueLabel] = {SK_ColorBLUE};
  mixer[Ids::kColorLabelExampleBorder] = {SK_ColorGRAY};
  mixer[Ids::kColorLabelExampleThickBorder] = {SK_ColorRED};
  mixer[Ids::kColorLabelExampleLowerShadow] = {SK_ColorGRAY};
  mixer[Ids::kColorLabelExampleUpperShadow] = {SK_ColorRED};
  mixer[Ids::kColorLabelExampleCustomBackground] = {SK_ColorLTGRAY};
  mixer[Ids::kColorLabelExampleCustomBorder] = {SK_ColorGRAY};
  mixer[Ids::kColorMenuButtonExampleBorder] = {SK_ColorGRAY};
  mixer[Ids::kColorMultilineExampleBorder] = {SK_ColorGRAY};
  mixer[Ids::kColorMultilineExampleColorRange] = {SK_ColorRED};
  mixer[Ids::kColorMultilineExampleForeground] = {SK_ColorBLACK};
  mixer[Ids::kColorMultilineExampleLabelBorder] = {SK_ColorCYAN};
  mixer[Ids::kColorMultilineExampleSelectionBackground] = {SK_ColorGRAY};
  mixer[Ids::kColorMultilineExampleSelectionForeground] = {SK_ColorBLACK};
  mixer[Ids::kColorNotificationExampleImage] = {SK_ColorGREEN};
  mixer[Ids::kColorScrollViewExampleBigSquareFrom] = {SK_ColorRED};
  mixer[Ids::kColorScrollViewExampleBigSquareTo] = {SK_ColorGREEN};
  mixer[Ids::kColorScrollViewExampleSmallSquareFrom] = {SK_ColorYELLOW};
  mixer[Ids::kColorScrollViewExampleSmallSquareTo] = {SK_ColorGREEN};
  mixer[Ids::kColorScrollViewExampleTallFrom] = {SK_ColorRED};
  mixer[Ids::kColorScrollViewExampleTallTo] = {SK_ColorCYAN};
  mixer[Ids::kColorScrollViewExampleWideFrom] = {SK_ColorYELLOW};
  mixer[Ids::kColorScrollViewExampleWideTo] = {SK_ColorCYAN};
  mixer[Ids::kColorTableExampleEvenRowIcon] = {SK_ColorRED};
  mixer[Ids::kColorTableExampleOddRowIcon] = {SK_ColorBLUE};
  mixer[Ids::kColorTextfieldExampleBigRange] = {SK_ColorBLUE};
  mixer[Ids::kColorTextfieldExampleName] = {SK_ColorGREEN};
  mixer[Ids::kColorTextfieldExampleSmallRange] = {SK_ColorRED};
  mixer[Ids::kColorVectorExampleImageBorder] = {SK_ColorBLACK};
  mixer[Ids::kColorWidgetExampleContentBorder] = {SK_ColorGRAY};
  mixer[Ids::kColorWidgetExampleDialogBorder] = {SK_ColorGRAY};
  mixer[Ids::kColorButtonBackgroundFab] = {dark_mode ? ui::kColorRefSecondary30
                                                     : ui::kColorRefPrimary90};
}

}  // namespace views::examples
