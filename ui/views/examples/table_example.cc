// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/table_example.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

using base::ASCIIToUTF16;

namespace views {
namespace examples {

namespace {

ui::TableColumn TestTableColumn(int id, const std::string& title) {
  ui::TableColumn column;
  column.id = id;
  column.title = ASCIIToUTF16(title.c_str());
  column.sortable = true;
  return column;
}

}  // namespace

TableExample::TableExample() : ExampleBase("Table") {}

TableExample::~TableExample() {
  // Delete the view before the model.
  delete table_;
  table_ = nullptr;
}

void TableExample::CreateExampleView(View* container) {
  container->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(LayoutOrientation::kVertical);

  std::vector<ui::TableColumn> columns;
  columns.push_back(TestTableColumn(0, "Fruit"));
  columns[0].percent = 1;
  columns.push_back(TestTableColumn(1, "Color"));
  columns.push_back(TestTableColumn(2, "Origin"));
  columns.push_back(TestTableColumn(3, "Price"));
  columns.back().alignment = ui::TableColumn::RIGHT;

  auto full_flex = FlexSpecification(MinimumFlexSizeRule::kScaleToZero,
                                     MaximumFlexSizeRule::kUnbounded)
                       .WithWeight(1);

  // Make table
  auto table = std::make_unique<TableView>(this, columns, ICON_AND_TEXT, true);
  table->SetGrouper(this);
  table->set_observer(this);
  table_ = table.get();
  container
      ->AddChildView(TableView::CreateScrollViewWithTable(std::move(table)))
      ->SetProperty(views::kFlexBehaviorKey, full_flex);

  icon1_.allocN32Pixels(16, 16);
  icon2_.allocN32Pixels(16, 16);

  SkCanvas canvas1(icon1_, SkSurfaceProps{}), canvas2(icon2_, SkSurfaceProps{});
  canvas1.drawColor(SK_ColorRED);
  canvas2.drawColor(SK_ColorBLUE);

  auto* button_panel = container->AddChildView(std::make_unique<View>());
  button_panel->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(LayoutOrientation::kHorizontal);

  const auto make_checkbox = [&](base::string16 label, int id) {
    auto* const checkbox =
        button_panel->AddChildView(std::make_unique<Checkbox>(
            std::move(label), Button::PressedCallback()));
    checkbox->SetCallback(base::BindRepeating(
        [](TableView* table, int id, Checkbox* checkbox) {
          table->SetColumnVisibility(id, checkbox->GetChecked());
        },
        base::Unretained(table_), id, checkbox));
    checkbox->SetChecked(true);
    return checkbox;
  };
  column1_visible_checkbox_ =
      make_checkbox(ASCIIToUTF16("Fruit column visible"), 0);
  column2_visible_checkbox_ =
      make_checkbox(ASCIIToUTF16("Color column visible"), 1);
  column3_visible_checkbox_ =
      make_checkbox(ASCIIToUTF16("Origin column visible"), 2);
  column4_visible_checkbox_ =
      make_checkbox(ASCIIToUTF16("Price column visible"), 3);

  for (View* child : button_panel->children())
    child->SetProperty(views::kFlexBehaviorKey, full_flex);
}

int TableExample::RowCount() {
  return 10;
}

base::string16 TableExample::GetText(int row, int column_id) {
  if (row == -1)
    return base::string16();

  const char* const cells[5][4] = {
      {"Orange", "Orange", "South america", "$5"},
      {"Apple", "Green", "Canada", "$3"},
      {"Blue berries", "Blue", "Mexico", "$10.3"},
      {"Strawberries", "Red", "California", "$7"},
      {"Cantaloupe", "Orange", "South america", "$5"},
  };
  return ASCIIToUTF16(cells[row % 5][column_id]);
}

gfx::ImageSkia TableExample::GetIcon(int row) {
  SkBitmap row_icon = row % 2 ? icon1_ : icon2_;
  return gfx::ImageSkia::CreateFrom1xBitmap(row_icon);
}

base::string16 TableExample::GetTooltip(int row) {
  if (row == -1)
    return base::string16();

  const char* const tooltips[5] = {
      "Orange - Orange you glad I didn't say banana?",
      "Apple - An apple a day keeps the doctor away",
      "Blue berries - Bet you can't eat just one",
      "Strawberries - Always better when homegrown",
      "Cantaloupe - So nice when perfectly ripe"};

  return ASCIIToUTF16(tooltips[row % 5]);
}

void TableExample::SetObserver(ui::TableModelObserver* observer) {}

void TableExample::GetGroupRange(int model_index, GroupRange* range) {
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
  PrintStatus("Selected: %s",
              base::UTF16ToASCII(GetText(table_->selection_model().active(), 0))
                  .c_str());
}

void TableExample::OnDoubleClick() {
  PrintStatus("Double Click: %s",
              base::UTF16ToASCII(GetText(table_->selection_model().active(), 0))
                  .c_str());
}

void TableExample::OnMiddleClick() {}

void TableExample::OnKeyDown(ui::KeyboardCode virtual_keycode) {}

}  // namespace examples
}  // namespace views
