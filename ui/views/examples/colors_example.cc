// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/colors_example.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace views {
namespace examples {

namespace {

// Argument utility macro that expands |label| to both a UTF16 string as the
// first argument and the corresponding ui::ColorId as the second argument.
#define COLOR_LABEL_ARGS(label) u## #label, ui::label

// Starts a new row and adds two columns to |layout|, the first displaying
// |label_string| and the second displaying |color_id| with its color and
// equivalent components as text.
void InsertColorRow(GridLayout* layout,
                    base::StringPiece16 label_string,
                    ui::ColorId color_id) {
  layout->StartRow(GridLayout::kFixedSize, 0);
  auto* label_view =
      layout->AddView(std::make_unique<Label>(std::u16string(label_string)));
  label_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_view->SetSelectable(true);

  auto* color_view = layout->AddView(std::make_unique<Label>());
  color_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  auto background_color = color_view->GetColorProvider()->GetColor(color_id);
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
}

// Returns a view of two columns where the first contains the identifier names
// of ui::ColorId and the second contains the color.
void CreateAllColorsView(ScrollView* scroll_view) {
  auto* container = scroll_view->SetContents(std::make_unique<View>());
  auto* layout = container->SetLayoutManager(std::make_unique<GridLayout>());
  auto* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1.0,
                        GridLayout::ColumnSize::kUsePreferred, 0, 0);
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorWindowBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorDialogBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorDialogForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorBubbleBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorFocusableBorderFocused));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorFocusableBorderUnfocused));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorButtonForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorButtonForegroundDisabled));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorButtonForegroundUnchecked));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorButtonBackgroundProminent));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorButtonBackgroundProminentFocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorButtonBackgroundProminentDisabled));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorButtonForegroundProminent));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorButtonBorder));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuItemForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuItemForegroundDisabled));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuItemForegroundSelected));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuItemBackgroundSelected));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuItemForegroundSecondary));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuSeparator));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuBorder));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuIcon));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuItemBackgroundHighlighted));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorMenuItemForegroundHighlighted));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorMenuItemBackgroundAlertedInitial));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorMenuItemBackgroundAlertedTarget));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorLabelForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorLabelForegroundDisabled));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorLabelForegroundSecondary));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorLabelSelectionForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorLabelSelectionBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorLinkForegroundDisabled));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorLinkForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorLinkForegroundPressed));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorSeparator));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTabForegroundSelected));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTabForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTabContentSeparator));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTextfieldForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTextfieldBackground));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorTextfieldForegroundPlaceholder));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTextfieldForegroundDisabled));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTextfieldBackgroundDisabled));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTextfieldSelectionForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTextfieldSelectionBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTooltipBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTooltipForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTreeBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTreeNodeForeground));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorTreeNodeForegroundSelectedFocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorTreeNodeForegroundSelectedUnfocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorTreeNodeBackgroundSelectedFocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorTreeNodeBackgroundSelectedUnfocused));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTableBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTableForeground));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorTableForegroundSelectedFocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorTableForegroundSelectedUnfocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorTableBackgroundSelectedFocused));
  InsertColorRow(layout,
                 COLOR_LABEL_ARGS(kColorTableBackgroundSelectedUnfocused));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTableGroupingIndicator));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTableHeaderForeground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTableHeaderBackground));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorTableHeaderSeparator));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorThrobber));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorThrobberPreconnect));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorAlertLowSeverity));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorAlertMediumSeverity));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorAlertHighSeverity));
  InsertColorRow(layout, COLOR_LABEL_ARGS(kColorIcon));
  // Expands the view to allow for scrolling.
  container->SizeToPreferredSize();
}

class AllColorsScrollView : public ScrollView {
 public:
  AllColorsScrollView() {
    constexpr int kMaxHeight = 300;
    ClipHeightTo(0, kMaxHeight);
  }

 protected:
  void OnThemeChanged() override {
    ScrollView::OnThemeChanged();
    CreateAllColorsView(this);
  }
};

}  // namespace

ColorsExample::ColorsExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_THEME_SELECT_LABEL).c_str()) {}

ColorsExample::~ColorsExample() = default;

void ColorsExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(std::make_unique<AllColorsScrollView>());
}

}  // namespace examples
}  // namespace views
