// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/native_theme_example.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace views {
namespace examples {

namespace {

// Argument utility macro that expands |label| to both a string as the first
// argument and the corresponding ui::NativeTheme::ColorId as the second
// argument.
#define COLOR_LABEL_ARGS(label) \
  base::ASCIIToUTF16(#label), ui::NativeTheme::ColorId::label

// Starts a new row and adds two columns to |layout|, the first displaying
// |label_string| and the second displaying |color_id| with its color and
// equivalent components as text.
void InsertColorRow(GridLayout* layout,
                    base::StringPiece16 label_string,
                    ui::NativeTheme::ColorId color_id) {
  auto label_view = std::make_unique<Label>(std::u16string(label_string));
  label_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_view->SetSelectable(true);

  auto color_view = std::make_unique<Label>();
  color_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  auto background_color =
      color_view->GetNativeTheme()->GetSystemColor(color_id);
  uint8_t red = SkColorGetR(background_color);
  uint8_t green = SkColorGetG(background_color);
  uint8_t blue = SkColorGetB(background_color);
  uint8_t alpha = SkColorGetA(background_color);
  std::string color_string =
      base::StringPrintf("#%02x%02x%02x Alpha: %d", red, green, blue, alpha);
  color_view->SetText(base::ASCIIToUTF16(color_string));
  color_view->SetBackgroundColor(background_color);
  color_view->SetBackground(CreateSolidBackground(background_color));
  color_view->SetSelectable(true);

  layout->StartRow(GridLayout::kFixedSize, 0);
  layout->AddView(std::move(label_view));
  layout->AddView(std::move(color_view));
}

// Returns a view of two columns where the first contains the identifier names
// of ui::NativeTheme::ColorId and the second contains the color.
std::unique_ptr<View> CreateAllColorsView() {
  auto container = std::make_unique<View>();
  auto* layout = container->SetLayoutManager(std::make_unique<GridLayout>());
  auto* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_WindowBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_DialogBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_DialogForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_BubbleBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_FocusedBorderColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_UnfocusedBorderColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_ButtonEnabledColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_ButtonDisabledColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_ButtonUncheckedColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_ProminentButtonColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_ProminentButtonFocusedColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_ProminentButtonDisabledColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TextOnProminentButtonColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_ButtonBorderColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_EnabledMenuItemForegroundColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_DisabledMenuItemForegroundColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_SelectedMenuItemForegroundColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_FocusedMenuItemBackgroundColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_MenuItemMinorTextColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_MenuSeparatorColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_MenuBackgroundColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_MenuBorderColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_MenuIconColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_HighlightedMenuItemBackgroundColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_HighlightedMenuItemForegroundColor));
  InsertColorRow(
      layout, COLOR_LABEL_ARGS(kColorId_MenuItemInitialAlertBackgroundColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_MenuItemTargetAlertBackgroundColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_LabelEnabledColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_LabelDisabledColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_LabelSecondaryColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_LabelTextSelectionColor));
  InsertColorRow(
      layout, COLOR_LABEL_ARGS(kColorId_LabelTextSelectionBackgroundFocused));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_LinkDisabled));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_LinkEnabled));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_LinkPressed));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_SeparatorColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TabTitleColorActive));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TabTitleColorInactive));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TabBottomBorder));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TextfieldDefaultColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TextfieldDefaultBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TextfieldPlaceholderColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TextfieldReadOnlyColor));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_TextfieldReadOnlyBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TextfieldSelectionColor));
  InsertColorRow(
      layout, COLOR_LABEL_ARGS(kColorId_TextfieldSelectionBackgroundFocused));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TooltipBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TooltipText));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TreeBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TreeText));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TreeSelectedText));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TreeSelectedTextUnfocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_TreeSelectionBackgroundFocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_TreeSelectionBackgroundUnfocused));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TableBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TableText));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TableSelectedText));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TableSelectedTextUnfocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_TableSelectionBackgroundFocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_TableSelectionBackgroundUnfocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorId_TableGroupingIndicatorColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TableHeaderText));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TableHeaderBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_TableHeaderSeparator));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_ThrobberSpinningColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_ThrobberWaitingColor));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_AlertSeverityLow));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_AlertSeverityMedium));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_AlertSeverityHigh));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorId_DefaultIconColor));
  // Expands the view to allow for scrolling.
  container->SizeToPreferredSize();
  return container;
}

}  // namespace

NativeThemeExample::NativeThemeExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_THEME_SELECT_LABEL).c_str()) {}

NativeThemeExample::~NativeThemeExample() = default;

void NativeThemeExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  auto scroll_view = std::make_unique<ScrollView>();
  scroll_view->SetContents(CreateAllColorsView());
  container->AddChildView(std::move(scroll_view));
}

}  // namespace examples
}  // namespace views
