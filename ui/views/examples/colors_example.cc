// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/colors_example.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/examples/grit/views_examples_resources.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/table_layout.h"

namespace views::examples {

namespace {

// Argument utility macro that expands |label| to both a UTF16 string as the
// first argument and the corresponding ui::ColorId as the second argument.
#define COLOR_LABEL_ARGS(label) u## #label, ui::label

// Starts a new row and adds two columns to |layout|, the first displaying
// |label_string| and the second displaying |color_id| with its color and
// equivalent components as text.
void InsertColorRow(View* parent,
                    std::u16string_view label_string,
                    ui::ColorId color_id) {
  auto* label_view = parent->AddChildView(
      std::make_unique<Label>(std::u16string(label_string)));
  label_view->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_view->SetSelectable(true);

  auto* color_view = parent->AddChildView(std::make_unique<Label>());
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
  container->SetLayoutManager(std::make_unique<TableLayout>())
      ->AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 1.0,
                  TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch, 1.0,
                 TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(71, TableLayout::kFixedSize);
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorWindowBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorDialogBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorDialogForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorBubbleBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorFocusableBorderFocused));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorFocusableBorderUnfocused));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorButtonForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorButtonForegroundDisabled));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorRadioButtonForegroundUnchecked));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorButtonBackgroundProminent));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorButtonBackgroundProminentFocused));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorButtonBackgroundProminentDisabled));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorButtonForegroundProminent));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorButtonBorder));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorMenuItemForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorMenuItemForegroundDisabled));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorMenuItemForegroundSelected));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorMenuItemBackgroundSelected));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorMenuItemForegroundSecondary));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorMenuSeparator));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorMenuBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorMenuBorder));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorMenuIcon));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorMenuItemBackgroundHighlighted));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorMenuItemForegroundHighlighted));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorMenuItemBackgroundAlertedInitial));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorMenuItemBackgroundAlertedTarget));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorLabelForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorLabelForegroundDisabled));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorLabelForegroundSecondary));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorLabelSelectionForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorLabelSelectionBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorLinkForegroundDisabled));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorLinkForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorLinkForegroundPressed));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorSeparator));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTabForegroundSelected));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTabForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTabContentSeparator));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTextfieldForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTextfieldBackground));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTextfieldForegroundPlaceholder));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTextfieldForegroundDisabled));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTextfieldBackgroundDisabled));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTextfieldSelectionForeground));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTextfieldSelectionBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTooltipBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTooltipForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTreeBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTreeNodeForeground));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTreeNodeForegroundSelectedFocused));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTreeNodeForegroundSelectedUnfocused));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTreeNodeBackgroundSelectedFocused));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTreeNodeBackgroundSelectedUnfocused));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTableBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTableForeground));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTableForegroundSelectedFocused));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTableForegroundSelectedUnfocused));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTableBackgroundSelectedFocused));
  InsertColorRow(container,
                 COLOR_LABEL_ARGS(kColorTableBackgroundSelectedUnfocused));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTableGroupingIndicator));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTableHeaderForeground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTableHeaderBackground));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorTableHeaderSeparator));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorThrobber));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorThrobberPreconnect));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorAlertLowSeverity));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorAlertMediumSeverityIcon));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorAlertMediumSeverityText));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorAlertHighSeverity));
  InsertColorRow(container, COLOR_LABEL_ARGS(kColorIcon));
  // Expands the view to allow for scrolling.
  container->SizeToPreferredSize();
}

class AllColorsScrollView : public ScrollView {
  METADATA_HEADER(AllColorsScrollView, ScrollView)

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

BEGIN_METADATA(AllColorsScrollView)
END_METADATA

}  // namespace

ColorsExample::ColorsExample()
    : ExampleBase(l10n_util::GetStringUTF8(IDS_THEME_SELECT_LABEL).c_str()) {}

ColorsExample::~ColorsExample() = default;

void ColorsExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<FillLayout>());
  container->AddChildView(std::make_unique<AllColorsScrollView>());
}

}  // namespace views::examples
