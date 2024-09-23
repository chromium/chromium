// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/table_example.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/examples/examples_color_id.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

using base::ASCIIToUTF16;

namespace views::examples {

namespace {

ui::TableColumn TestTableColumn(
    int id,
    const std::u16string& title,
    ui::TableColumn::Alignment alignment = ui::TableColumn::LEFT,
    float percent = 0.0) {
  ui::TableColumn column;
  column.id = id;
  column.title = title;
  column.sortable = true;
  column.alignment = alignment;
  column.percent = percent;
  return column;
}

}  // namespace

TableExample::TableExample() : ExampleBase("Table") {}

TableExample::~TableExample() {
  observer_.Reset();
  // Delete the view before the model.
  if (table_ && table_->parent()) {
    table_->parent()->RemoveChildViewT(table_.ExtractAsDangling());
  }
}

void TableExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(LayoutOrientation::kVertical);
  observer_.Observe(container);

  auto full_flex = FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                     MaximumFlexSizeRule::kUnbounded)
                       .WithWeight(1);

  const auto make_checkbox =
      [](const std::u16string& label, int id, raw_ptr<TableView>* table,
         raw_ptr<Checkbox>* checkbox, FlexSpecification full_flex) {
        return Builder<Checkbox>()
            .CopyAddressTo(checkbox)
            .SetText(label)
            .SetCallback(base::BindRepeating(
                [](int id, raw_ptr<TableView>* table, Checkbox* checkbox) {
                  (*table)->SetColumnVisibility(id, checkbox->GetChecked());
                },
                id, table, *checkbox))
            .SetChecked(true)
            .SetProperty(kFlexBehaviorKey, full_flex);
      };

  // Make table
  Builder<View>(container)
      .AddChildren(
          TableView::CreateScrollViewBuilderWithTable(
              Builder<TableView>()
                  .CopyAddressTo(&table_)
                  .SetColumns(
                      {TestTableColumn(0, u"Fruit", ui::TableColumn::LEFT, 1.0),
                       TestTableColumn(1, u"Color"),
                       TestTableColumn(2, u"Origin"),
                       TestTableColumn(3, u"Price", ui::TableColumn::RIGHT)})
                  .SetTableType(TableType::kIconAndText)
                  .SetModel(this)
                  .SetGrouper(this)
                  .SetObserver(this))
              .SetProperty(kFlexBehaviorKey, full_flex),
          Builder<FlexLayoutView>()
              .SetOrientation(LayoutOrientation::kHorizontal)
              .AddChildren(
                  make_checkbox(u"Fruit column visible", 0, &table_,
                                &column1_visible_checkbox_, full_flex),
                  make_checkbox(u"Color column visible", 1, &table_,
                                &column2_visible_checkbox_, full_flex),
                  make_checkbox(u"Origin column visible", 2, &table_,
                                &column3_visible_checkbox_, full_flex),
                  make_checkbox(u"Price column visible", 3, &table_,
                                &column4_visible_checkbox_, full_flex)))
      .BuildChildren();
}

size_t TableExample::RowCount() {
  return 10;
}

std::u16string TableExample::GetText(size_t row, int column_id) {
  constexpr auto cells = std::to_array<std::array<const char* const, 4>>({
      {"Orange", "Orange", "South America", "$5"},
      {"Apple", "Green", "Canada", "$3"},
      {"Blueberries", "Blue", "Mexico", "$10.30"},
      {"Strawberries", "Red", "California", "$7"},
      {"Cantaloupe", "Orange", "South America", "$5"},
  });
  return ASCIIToUTF16(cells[row % 5][column_id]);
}

ui::ImageModel TableExample::GetIcon(size_t row) {
  SkBitmap row_icon = row % 2 ? icon1_ : icon2_;
  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia::CreateFrom1xBitmap(row_icon));
}

std::u16string TableExample::GetTooltip(size_t row) {
  constexpr auto tooltips =
      std::to_array({"Orange - Orange you glad I didn't say banana?",
                     "Apple - An apple a day keeps the doctor away",
                     "Blueberries - Bet you can't eat just one",
                     "Strawberries - Always better when homegrown",
                     "Cantaloupe - So nice when perfectly ripe"});

  return ASCIIToUTF16(tooltips[row % 5]);
}

void TableExample::SetObserver(ui::TableModelObserver* observer) {}

void TableExample::GetGroupRange(size_t model_index, GroupRange* range) {
  if (model_index < 2) {
    range->start = 0;
    range->length = 2;
  } else if (model_index > 6) {
    range->start = 7;
    range->length = 3;
  } else {
    range->start = model_index;
    range->length = 1;
  }
}

void TableExample::OnSelectionChanged() {
  PrintStatus("Selected: %s", SelectedColumnName().c_str());
}

void TableExample::OnDoubleClick() {
  PrintStatus("Double Click: %s", SelectedColumnName().c_str());
}

void TableExample::OnMiddleClick() {}

void TableExample::OnKeyDown(ui::KeyboardCode virtual_keycode) {}

void TableExample::OnViewThemeChanged(View* observed_view) {
  icon1_.allocN32Pixels(16, 16);
  icon2_.allocN32Pixels(16, 16);

  auto* const cp = observed_view->GetColorProvider();
  SkCanvas canvas1(icon1_, SkSurfaceProps{}), canvas2(icon2_, SkSurfaceProps{});
  canvas1.drawColor(
      cp->GetColor(ExamplesColorIds::kColorTableExampleEvenRowIcon));
  canvas2.drawColor(
      cp->GetColor(ExamplesColorIds::kColorTableExampleOddRowIcon));
}

void TableExample::OnViewIsDeleting(View* observed_view) {
  observer_.Reset();
}

std::string TableExample::SelectedColumnName() {
  return table_->selection_model().active().has_value()
             ? base::UTF16ToASCII(
                   GetText(table_->selection_model().active().value(), 0))
             : std::string("<None>");
}

}  // namespace views::examples
