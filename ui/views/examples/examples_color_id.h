// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_COLOR_ID_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_COLOR_ID_H_

#include "ui/color/color_id.h"

// clang-format off
#define EXAMPLES_COLOR_IDS \
  E_CPONLY(kColorAnimatedImageViewExampleBorder, , kColorExamplesStart) \
  E_CPONLY(kColorAnimationExampleForeground) \
  E_CPONLY(kColorAnimationExampleBackground) \
  E_CPONLY(kColorAccessibilityExampleBackground) \
  E_CPONLY(kColorBubbleExampleBackground1) \
  E_CPONLY(kColorBubbleExampleBackground2) \
  E_CPONLY(kColorBubbleExampleBackground3) \
  E_CPONLY(kColorBubbleExampleBackground4) \
  E_CPONLY(kColorButtonBackgroundFab) \
  E_CPONLY(kColorDesignerGrabHandle) \
  E_CPONLY(kColorDesignerGrid) \
  E_CPONLY(kColorFadeAnimationExampleBackground) \
  E_CPONLY(kColorFadeAnimationExampleBorder) \
  E_CPONLY(kColorFadeAnimationExampleForeground) \
  E_CPONLY(kColorInkDropExampleBase) \
  E_CPONLY(kColorInkDropExampleBorder) \
  E_CPONLY(kColorLabelExampleBorder) \
  E_CPONLY(kColorLabelExampleBlueLabel) \
  E_CPONLY(kColorLabelExampleLowerShadow) \
  E_CPONLY(kColorLabelExampleUpperShadow) \
  E_CPONLY(kColorLabelExampleCustomBackground) \
  E_CPONLY(kColorLabelExampleCustomBorder) \
  E_CPONLY(kColorLabelExampleThickBorder) \
  E_CPONLY(kColorMenuButtonExampleBorder) \
  E_CPONLY(kColorMultilineExampleBorder) \
  E_CPONLY(kColorMultilineExampleColorRange) \
  E_CPONLY(kColorMultilineExampleForeground) \
  E_CPONLY(kColorMultilineExampleLabelBorder) \
  E_CPONLY(kColorMultilineExampleSelectionBackground) \
  E_CPONLY(kColorMultilineExampleSelectionForeground) \
  E_CPONLY(kColorNotificationExampleImage) \
  E_CPONLY(kColorScrollViewExampleBigSquareFrom) \
  E_CPONLY(kColorScrollViewExampleBigSquareTo) \
  E_CPONLY(kColorScrollViewExampleSmallSquareFrom) \
  E_CPONLY(kColorScrollViewExampleSmallSquareTo) \
  E_CPONLY(kColorScrollViewExampleTallFrom) \
  E_CPONLY(kColorScrollViewExampleTallTo) \
  E_CPONLY(kColorScrollViewExampleWideFrom) \
  E_CPONLY(kColorScrollViewExampleWideTo) \
  E_CPONLY(kColorTableExampleEvenRowIcon) \
  E_CPONLY(kColorTableExampleOddRowIcon) \
  E_CPONLY(kColorTextfieldExampleBigRange) \
  E_CPONLY(kColorTextfieldExampleName) \
  E_CPONLY(kColorTextfieldExampleSmallRange) \
  E_CPONLY(kColorVectorExampleImageBorder) \
  E_CPONLY(kColorWidgetExampleContentBorder) \
  E_CPONLY(kColorWidgetExampleDialogBorder)
// clang-format on

namespace views::examples {

#include "ui/color/color_id_macros.inc"

// clang-format off
enum ExamplesColorIds : ui::ColorId {
  // This should move the example color ids out of the range of any production
  // ids.
  kColorExamplesStart = ui::kUiColorsEnd + 0x8000,

  EXAMPLES_COLOR_IDS

  kColorExamplesEnd,
};
// clang-format on

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_macros.inc"  // NOLINT(build/include)

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_COLOR_ID_H_
