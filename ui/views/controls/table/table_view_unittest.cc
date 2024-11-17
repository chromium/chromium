// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_view.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_header.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/focus_manager_test.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"

// Put the tests in the views namespace to make it easier to declare them as
// friend classes.
namespace views {

constexpr int kGroupingIndicatorSize = 6;

class TableViewTestHelper {
 public:
  explicit TableViewTestHelper(TableView* table) : table_(table) {}

  TableViewTestHelper(const TableViewTestHelper&) = delete;
  TableViewTestHelper& operator=(const TableViewTestHelper&) = delete;

  std::string GetPaintRegion(const gfx::Rect& bounds) {
    TableView::PaintRegion region(table_->GetPaintRegion(bounds));
    return "rows=" + base::NumberToString(region.min_row) + " " +
           base::NumberToString(region.max_row) +
           " cols=" + base::NumberToString(region.min_column) + " " +
           base::NumberToString(region.max_column);
  }

  size_t visible_col_count() { return table_->visible_columns().size(); }

  std::optional<size_t> GetActiveVisibleColumnIndex() {
    return table_->GetActiveVisibleColumnIndex();
  }

  TableHeader* header() { return table_->header_; }

  void SetSelectionModel(const ui::ListSelectionModel& new_selection) {
    table_->SetSelectionModel(new_selection);
  }

  const gfx::FontList& font_list() { return table_->font_list_; }

  AXVirtualView* GetVirtualAccessibilityBodyRow(size_t row) {
    return table_->GetVirtualAccessibilityBodyRow(row);
  }

  AXVirtualView* GetVirtualAccessibilityHeaderRow() {
    return table_->GetVirtualAccessibilityHeaderRow();
  }

  AXVirtualView* GetVirtualAccessibilityHeaderCell(
      size_t visible_column_index) {
    return table_->GetVirtualAccessibilityCellImpl(
        GetVirtualAccessibilityHeaderRow(), visible_column_index);
  }

  AXVirtualView* GetVirtualAccessibilityCell(size_t row,
                                             size_t visible_column_index) {
    return table_->GetVirtualAccessibilityCell(row, visible_column_index);
  }

  gfx::Rect GetCellBounds(size_t row, size_t visible_column_index) const {
    return table_->GetCellBounds(row, visible_column_index);
  }

  gfx::Rect GetActiveCellBounds() const {
    return table_->GetActiveCellBounds();
  }

  void SelectRowsInRangeFrom(size_t view_index,
                             bool select,
                             ui::ListSelectionModel* model) {
    table_->SelectRowsInRangeFrom(view_index, select, model);
  }

  std::vector<std::vector<gfx::Rect>> GenerateExpectedBounds() {
    // Generates the expected bounds for |table_|'s rows and cells. Each vector
    // represents a row. The first entry in each child vector is the bounds for
    // the entire row. The following entries in that vector are the bounds for
    // each individual cell contained in that row.
    auto expected_bounds = std::vector<std::vector<gfx::Rect>>();

    // Generate the bounds for the header row and cells.
    auto header_row = std::vector<gfx::Rect>();
    gfx::Rect header_row_bounds =
        table_->CalculateHeaderRowAccessibilityBounds();
    View::ConvertRectToScreen(table_, &header_row_bounds);
    header_row.push_back(header_row_bounds);
    for (size_t column_index = 0; column_index < visible_col_count();
         column_index++) {
      gfx::Rect header_cell_bounds =
          table_->CalculateHeaderCellAccessibilityBounds(column_index);
      View::ConvertRectToScreen(table_, &header_cell_bounds);
      header_row.push_back(header_cell_bounds);
    }
    expected_bounds.push_back(header_row);

    // Generate the bounds for the table rows and cells.
    for (size_t row_index = 0; row_index < table_->GetRowCount(); row_index++) {
      auto table_row = std::vector<gfx::Rect>();
      gfx::Rect table_row_bounds =
          table_->CalculateTableRowAccessibilityBounds(row_index);
      View::ConvertRectToScreen(table_, &table_row_bounds);
      table_row.push_back(table_row_bounds);
      for (size_t column_index = 0; column_index < visible_col_count();
           column_index++) {
        gfx::Rect table_cell_bounds =
            table_->CalculateTableCellAccessibilityBounds(row_index,
                                                          column_index);
        View::ConvertRectToScreen(table_, &table_cell_bounds);
        table_row.push_back(table_cell_bounds);
      }
      expected_bounds.push_back(table_row);
    }

    return expected_bounds;
  }

  int GetCellMargin() const { return table_->GetCellMargin(); }

  int GetCellElementSpacing() const { return table_->GetCellElementSpacing(); }

  gfx::Rect GetPaintIconSrcBounds(const gfx::Size& image_size,
                                  int image_dest_width) {
    return table_->GetPaintIconSrcBounds(image_size, image_dest_width);
  }

  gfx::Rect GetPaintIconDestBounds(const gfx::Rect& cell_bounds,
                                   int text_bounds_x) {
    return table_->GetPaintIconDestBounds(cell_bounds, text_bounds_x);
  }

 private:
  const raw_ptr<TableView> table_;
};

namespace {

// TestTableModel2 -------------------------------------------------------------

// Trivial TableModel implementation that is backed by a vector of vectors.
// Provides methods for adding/removing/changing the contents that notify the
// observer appropriately.
//
// Initial contents are:
// 0, 1
// 1, 1
// 2, 2
// 3, 0
class TestTableModel2 : public ui::TableModel {
 public:
  TestTableModel2();

  TestTableModel2(const TestTableModel2&) = delete;
  TestTableModel2& operator=(const TestTableModel2&) = delete;

  // Clears the model entirely, leaving it empty.
  void Clear();

  // Adds a new row at index |row| with values |c1_value| and |c2_value|.
  void AddRow(size_t row, int c1_value, int c2_value);

  // Adds new rows starting from |row| to |row| + |length| with the value
  // of |row| times the |value_multiplier|. The |value_multiplier| can be used
  // to distinguish these rows from the rest.
  void AddRows(size_t row, size_t length, int value_multiplier);

  // Removes the row at index |row|.
  void RemoveRow(size_t row);

  // Removes all the rows starting from |row| to |row| + |length|.
  void RemoveRows(size_t row, size_t length);

  // Changes the values of the row at |row|.
  void ChangeRow(size_t row, int c1_value, int c2_value);

  // Reorders rows in the model.
  void MoveRows(size_t row_from, size_t length, size_t row_to);

  // Allows overriding the tooltip for testing.
  void SetTooltip(const std::u16string& tooltip);

  // ui::TableModel:
  size_t RowCount() override;
  std::u16string GetText(size_t row, int column_id) override;
  std::u16string GetTooltip(size_t row) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  int CompareValues(size_t row1, size_t row2, int column_id) override;

 private:
  raw_ptr<ui::TableModelObserver> observer_ = nullptr;

  std::optional<std::u16string> tooltip_;

  // The data.
  std::vector<std::vector<int>> rows_;
};

TestTableModel2::TestTableModel2() {
  AddRow(0, 0, 1);
  AddRow(1, 1, 1);
  AddRow(2, 2, 2);
  AddRow(3, 3, 0);
}

void TestTableModel2::Clear() {
  RemoveRows(0, rows_.size());
}

void TestTableModel2::AddRow(size_t row, int c1_value, int c2_value) {
  DCHECK(row <= rows_.size());
  std::vector<int> new_row;
  new_row.push_back(c1_value);
  new_row.push_back(c2_value);
  rows_.insert(rows_.begin() + row, new_row);
  if (observer_)
    observer_->OnItemsAdded(row, 1);
}

void TestTableModel2::AddRows(size_t row, size_t length, int value_multiplier) {
  // Do not DCHECK here since we are testing the OnItemsAdded callback.
  if (row <= rows_.size()) {
    for (size_t i = row; i < row + length; i++) {
      std::vector<int> new_row;
      new_row.push_back(static_cast<int>(i) + value_multiplier);
      new_row.push_back(static_cast<int>(i) + value_multiplier);
      rows_.insert(rows_.begin() + i, new_row);
    }
  }

  if (observer_ && length > 0)
    observer_->OnItemsAdded(row, length);
}

void TestTableModel2::RemoveRow(size_t row) {
  DCHECK(row < rows_.size());
  rows_.erase(rows_.begin() + row);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void TestTableModel2::RemoveRows(size_t row, size_t length) {
  if (row <= rows_.size()) {
    rows_.erase(
        rows_.begin() + row,
        rows_.begin() + std::clamp(row + length, size_t{0}, rows_.size()));
  }

  if (observer_ && length > 0)
    observer_->OnItemsRemoved(row, length);
}

void TestTableModel2::ChangeRow(size_t row, int c1_value, int c2_value) {
  DCHECK(row < rows_.size());
  rows_[row][0] = c1_value;
  rows_[row][1] = c2_value;
  if (observer_)
    observer_->OnItemsChanged(row, 1);
}

void TestTableModel2::MoveRows(size_t row_from, size_t length, size_t row_to) {
  DCHECK_GT(length, 0u);
  DCHECK_LE(row_from + length, rows_.size());
  DCHECK_LE(row_to + length, rows_.size());

  auto old_start = rows_.begin() + row_from;
  std::vector<std::vector<int>> temp(old_start, old_start + length);
  rows_.erase(old_start, old_start + length);
  rows_.insert(rows_.begin() + row_to, temp.begin(), temp.end());
  if (observer_)
    observer_->OnItemsMoved(row_from, length, row_to);
}

void TestTableModel2::SetTooltip(const std::u16string& tooltip) {
  tooltip_ = tooltip;
}

size_t TestTableModel2::RowCount() {
  return rows_.size();
}

std::u16string TestTableModel2::GetText(size_t row, int column_id) {
  return base::NumberToString16(rows_[row][column_id]);
}

std::u16string TestTableModel2::GetTooltip(size_t row) {
  return tooltip_ ? *tooltip_ : u"Tooltip" + base::NumberToString16(row);
}

void TestTableModel2::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

int TestTableModel2::CompareValues(size_t row1, size_t row2, int column_id) {
  return rows_[row1][column_id] - rows_[row2][column_id];
}

// Returns the view to model mapping as a string.
std::string GetViewToModelAsString(TableView* table) {
  std::string result;
  for (size_t i = 0; i < table->GetRowCount(); ++i) {
    if (i != 0)
      result += " ";
    result += base::NumberToString(table->ViewToModel(i));
  }
  return result;
}

// Returns the model to view mapping as a string.
std::string GetModelToViewAsString(TableView* table) {
  std::string result;
  for (size_t i = 0; i < table->GetRowCount(); ++i) {
    if (i != 0)
      result += " ";
    result += base::NumberToString(table->ModelToView(i));
  }
  return result;
}

// Formats the whole table as a string, like: "[a, b, c], [d, e, f]". Rows
// scrolled out of view are included; hidden columns are excluded.
std::string GetRowsInViewOrderAsString(TableView* table) {
  std::string result;
  for (size_t i = 0; i < table->GetRowCount(); ++i) {
    if (i != 0)
      result += ", ";  // Comma between each row.

    // Format row |i| like this: "[value1, value2, value3]"
    result += "[";
    for (size_t j = 0; j < table->visible_columns().size(); ++j) {
      const ui::TableColumn& column = table->GetVisibleColumn(j).column;
      if (j != 0)
        result += ", ";  // Comma between each value in the row.

      result += base::UTF16ToUTF8(
          table->model()->GetText(table->ViewToModel(i), column.id));
    }
    result += "]";
  }
  return result;
}

// Formats the whole accessibility views as a string.
// Like: "[a, b, c], [d, e, f]".
std::string GetRowsInVirtualViewAsString(TableView* table) {
  auto& virtual_children = table->GetViewAccessibility().virtual_children();
  std::string result;

  for (size_t row_index = 0; row_index < virtual_children.size(); row_index++) {
    if (row_index != 0)
      result += ", ";  // Comma between each row.

    const auto& row = virtual_children[row_index];

    result += "[";

    for (size_t cell_index = 0; cell_index < row->children().size();
         cell_index++) {
      if (cell_index != 0)
        result += ", ";  // Comma between each value in the row.

      const auto& cell = row->children()[cell_index];
      const ui::AXNodeData& cell_data = cell->GetData();

      result += cell_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
    }

    result += "]";
  }

  return result;
}

std::string GetHeaderRowAsString(TableView* table) {
  std::string result = "[";

  for (size_t col_index = 0; col_index < table->visible_columns().size();
       ++col_index) {
    if (col_index != 0)
      result += ", ";  // Comma between each column.

    result +=
        base::UTF16ToUTF8(table->GetVisibleColumn(col_index).column.title);
  }

  result += "]";

  return result;
}

bool PressLeftMouseAt(views::View* target, const gfx::Point& point) {
  const ui::MouseEvent pressed(ui::EventType::kMousePressed, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  return target->OnMousePressed(pressed);
}

void ReleaseLeftMouseAt(views::View* target, const gfx::Point& point) {
  const ui::MouseEvent release(ui::EventType::kMouseReleased, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  target->OnMouseReleased(release);
}

bool DragLeftMouseTo(views::View* target, const gfx::Point& point) {
  const ui::MouseEvent dragged(ui::EventType::kMouseDragged, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               0);
  return target->OnMouseDragged(dragged);
}

}  // namespace

// The test parameter is used to control whether or not to test the TableView
// using the default construction path.
class TableViewTest : public ViewsTestBase,
                      public ::testing::WithParamInterface<
                          std::tuple</*use_default_construction=*/bool,
                                     /*use_rtl=*/bool>> {
 public:
  TableViewTest() = default;

  TableViewTest(const TableViewTest&) = delete;
  TableViewTest& operator=(const TableViewTest&) = delete;

  void SetUp() override {
    ViewsTestBase::SetUp();

    model_ = std::make_unique<TestTableModel2>();
    std::vector<ui::TableColumn> columns(2);
    columns[0].title = u"Title Column 0";
    columns[0].sortable = true;
    columns[1].title = u"Title Column 1";
    columns[1].id = 1;
    columns[1].sortable = true;

    std::unique_ptr<TableView> table;

    // Run the tests using both default and non-default TableView construction.
    if (use_default_construction()) {
      table = std::make_unique<TableView>();
      table->Init(model_.get(), columns, TableType::kTextOnly, false);
    } else {
      table = std::make_unique<TableView>(model_.get(), columns,
                                          TableType::kTextOnly, false);
    }
    table_ = table.get();
    auto scroll_view = TableView::CreateScrollViewWithTable(std::move(table));
    scroll_view_ = scroll_view.get();
    scroll_view->SetBounds(0, 0, 10000, 10000);
    helper_ = std::make_unique<TableViewTestHelper>(table_);

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    params.delegate = ConfigureWidgetDelegate();
    widget_->Init(std::move(params));
    test::RunScheduledLayout(
        widget_->GetRootView()->AddChildView(std::move(scroll_view)));
    widget_->Show();
  }

  void TearDown() override {
    table_ = nullptr;
    scroll_view_ = nullptr;
    helper_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void ClickOnRow(int row, int flags) {
    ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
    generator.set_flags(flags);
    generator.set_current_screen_location(GetPointForRow(row));
    generator.PressLeftButton();
  }

  void TapOnRow(int row) {
    ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
    generator.GestureTapAt(GetPointForRow(row));
  }

  std::string SelectionStateAsString() const {
    return table_->selection_model().ToString();
  }

  void PressKey(ui::KeyboardCode code) { PressKey(code, ui::EF_NONE); }

  void PressKey(ui::KeyboardCode code, int flags) {
    ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
    generator.PressKey(code, flags);
  }

  void VerifyTableViewAndAXOrder(std::string expected_view_order) {
    VerifyAXRowIndexes();

    // The table views should match the expected view order.
    EXPECT_EQ(expected_view_order, GetRowsInViewOrderAsString(table_));

    // Update the expected view order to have header information if exists.
    if (helper_->header()) {
      expected_view_order =
          GetHeaderRowAsString(table_) + ", " + expected_view_order;
    }

    EXPECT_EQ(expected_view_order, GetRowsInVirtualViewAsString(table_));
  }

  // Verifies that there is an unique, properly-indexed virtual row for every
  // row.
  void VerifyAXRowIndexes() {
    auto& virtual_children = table_->GetViewAccessibility().virtual_children();

    // Makes sure the virtual row count factors in the presence of the header.
    const int first_row_index = helper_->header() ? 1 : 0;
    const int virtual_row_count = table_->GetRowCount() + first_row_index;
    EXPECT_EQ(virtual_row_count, static_cast<int>(virtual_children.size()));

    // Make sure every virtual row is valid.
    for (int index = first_row_index; index < virtual_row_count; index++) {
      const auto& row = virtual_children[index];
      ASSERT_TRUE(row);

      // Normalize the row index to account for the presence of a header if
      // necessary.
      const int normalized_index = index - first_row_index;

      // Make sure the stored row index matches the row index in the table.
      const ui::AXNodeData& row_data = row->GetCustomData();
      const int stored_index =
          row_data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex);
      EXPECT_EQ(stored_index, normalized_index);
    }
  }

  // Helper function for comparing the bounds of |table_|'s virtual
  // accessibility child rows and cells with a set of expected bounds.
  void VerifyTableAccChildrenBounds(
      const ViewAccessibility& view_accessibility,
      const std::vector<std::vector<gfx::Rect>>& expected_bounds) {
    auto& virtual_children = view_accessibility.virtual_children();
    EXPECT_EQ(virtual_children.size(), expected_bounds.size());
    EXPECT_EQ(table_->GetRowCount() + 1U, expected_bounds.size());

    for (size_t row_index = 0; row_index < virtual_children.size();
         row_index++) {
      const auto& row = virtual_children[row_index];
      ASSERT_TRUE(row);
      const ui::AXNodeData& row_data = row->GetData();
      EXPECT_EQ(ax::mojom::Role::kRow, row_data.role);

      ui::AXOffscreenResult offscreen_result = ui::AXOffscreenResult();
      gfx::Rect row_custom_bounds = row->GetBoundsRect(
          ui::AXCoordinateSystem::kScreenDIPs,
          ui::AXClippingBehavior::kUnclipped, &offscreen_result);
      EXPECT_EQ(row_custom_bounds, expected_bounds[row_index][0]);
      if (table_->GetVisibleBounds().Intersects(
              expected_bounds[row_index][0])) {
        EXPECT_FALSE(row_data.HasState(ax::mojom::State::kInvisible));
      } else {
        EXPECT_TRUE(row_data.HasState(ax::mojom::State::kInvisible));
      }

      EXPECT_EQ(row->children().size(), expected_bounds[row_index].size() - 1U);
      EXPECT_EQ(row->children().size(), helper_->visible_col_count());
      for (size_t cell_index = 0; cell_index < row->children().size();
           cell_index++) {
        const auto& cell = row->children()[cell_index];
        ASSERT_TRUE(cell);
        const ui::AXNodeData& cell_data = cell->GetData();

        if (row_index == 0)
          EXPECT_EQ(ax::mojom::Role::kColumnHeader, cell_data.role);
        else
          EXPECT_EQ(ax::mojom::Role::kGridCell, cell_data.role);

        // Add 1 to get the cell's index into |expected_bounds| since the first
        // entry is the row's bounds.
        const int expected_bounds_index = cell_index + 1;
        gfx::Rect cell_custom_bounds = cell->GetBoundsRect(
            ui::AXCoordinateSystem::kScreenDIPs,
            ui::AXClippingBehavior::kUnclipped, &offscreen_result);
        EXPECT_EQ(cell_custom_bounds,
                  expected_bounds[row_index][expected_bounds_index]);
        if (table_->GetVisibleBounds().Intersects(
                expected_bounds[row_index][expected_bounds_index])) {
          EXPECT_FALSE(cell_data.HasState(ax::mojom::State::kInvisible));
        } else {
          EXPECT_TRUE(cell_data.HasState(ax::mojom::State::kInvisible));
        }
      }
    }
  }

  void SetColumnWidthForHorizontalScrollBarVisibility() {
    EXPECT_EQ(2u, helper_->visible_col_count());
    EXPECT_EQ(4u, table_->GetRowCount());
    // Initially no active visible column.
    EXPECT_FALSE(helper_->GetActiveVisibleColumnIndex().has_value());
    EXPECT_FALSE(scroll_view_->horizontal_scroll_bar()->GetVisible());
    scroll_view_->SetBounds(0, 0, 800, 800);
    // Set the column width to make the horizontal scroll bar visible.
    constexpr int kColumn0Width = 500;
    constexpr int kColumn1Width = 1000;
    table_->SetVisibleColumnWidth(0, kColumn0Width);
    table_->SetVisibleColumnWidth(1, kColumn1Width);
    test::RunScheduledLayout(scroll_view_);
    EXPECT_EQ(table_->GetVisibleColumn(0).width, kColumn0Width);
    EXPECT_EQ(table_->GetVisibleColumn(1).width, kColumn1Width);
    EXPECT_TRUE(scroll_view_->horizontal_scroll_bar()->GetVisible());
  }

  bool use_default_construction() const { return std::get<0>(GetParam()); }
  bool use_rtl() const { return std::get<1>(GetParam()); }

 protected:
  virtual WidgetDelegate* ConfigureWidgetDelegate() { return nullptr; }

  std::unique_ptr<TestTableModel2> model_;

  // Owned by the scroll view owned by `widget_`.
  raw_ptr<TableView> table_ = nullptr;
  raw_ptr<ScrollView> scroll_view_ = nullptr;

  std::unique_ptr<TableViewTestHelper> helper_;

  std::unique_ptr<Widget> widget_;

 private:
  gfx::Point GetPointForRow(int row) {
    const int y = (row + 0.5) * table_->GetRowHeight();
    return table_->GetBoundsInScreen().origin() + gfx::Vector2d(5, y);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    TableViewTest,
    testing::Combine(/*use_default_construction=*/testing::Bool(),
                     /*use_rtl=*/testing::Bool()));

// Using one of the arrow keys (which normally change selection) with an empty
// table must leave the selection state empty.
// Regression test for https://issues.chromium.org/issues/342341277
TEST_P(TableViewTest, SelectedIndexWithNoRows) {
  model_->Clear();
  table_->RequestFocus();
  EXPECT_TRUE(table_->selection_model().empty());
  table_->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_DOWN, 0));
  EXPECT_TRUE(table_->selection_model().empty());
}

// Verifies GetPaintRegion.
TEST_P(TableViewTest, GetPaintRegion) {
  // Two columns should be visible.
  EXPECT_EQ(2u, helper_->visible_col_count());

  EXPECT_EQ("rows=0 4 cols=0 2", helper_->GetPaintRegion(table_->bounds()));
  EXPECT_EQ("rows=0 4 cols=0 1",
            helper_->GetPaintRegion(gfx::Rect(0, 0, 1, table_->height())));
}

TEST_P(TableViewTest, RebuildVirtualAccessibilityChildren) {
  const ViewAccessibility& view_accessibility = table_->GetViewAccessibility();
  ui::AXNodeData data;
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kListGrid, data.role);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kReadOnly, data.GetRestriction());
  EXPECT_EQ(table_->GetRowCount(),
            static_cast<size_t>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount)));
  EXPECT_EQ(helper_->visible_col_count(),
            static_cast<size_t>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kTableColumnCount)));

  // The header takes up another row.
  ASSERT_EQ(static_cast<size_t>(table_->GetRowCount() + 1),
            view_accessibility.virtual_children().size());
  const auto& header = view_accessibility.virtual_children().front();
  ASSERT_TRUE(header);
  EXPECT_EQ(ax::mojom::Role::kRow, header->GetData().role);

  ASSERT_EQ(helper_->visible_col_count(), header->children().size());
  int j = 0;
  for (const auto& header_cell : header->children()) {
    ASSERT_TRUE(header_cell);
    const ui::AXNodeData& header_cell_data = header_cell->GetData();
    EXPECT_EQ(ax::mojom::Role::kColumnHeader, header_cell_data.role);
    EXPECT_EQ(j++, header_cell_data.GetIntAttribute(
                       ax::mojom::IntAttribute::kTableCellColumnIndex));
  }

  size_t i = 0;
  for (auto child_iter = view_accessibility.virtual_children().begin() + 1;
       i < table_->GetRowCount(); ++child_iter, ++i) {
    const auto& row = *child_iter;
    ASSERT_TRUE(row);
    const ui::AXNodeData& row_data = row->GetData();
    EXPECT_EQ(ax::mojom::Role::kRow, row_data.role);
    EXPECT_EQ(i, static_cast<size_t>(row_data.GetIntAttribute(
                     ax::mojom::IntAttribute::kTableRowIndex)));
    ASSERT_FALSE(row_data.HasState(ax::mojom::State::kInvisible));

    ASSERT_EQ(helper_->visible_col_count(), row->children().size());
    j = 0;
    for (const auto& cell : row->children()) {
      ASSERT_TRUE(cell);
      const ui::AXNodeData& cell_data = cell->GetData();
      EXPECT_EQ(ax::mojom::Role::kGridCell, cell_data.role);
      EXPECT_EQ(i, static_cast<size_t>(cell_data.GetIntAttribute(
                       ax::mojom::IntAttribute::kTableCellRowIndex)));
      EXPECT_EQ(j++, cell_data.GetIntAttribute(
                         ax::mojom::IntAttribute::kTableCellColumnIndex));
      ASSERT_FALSE(cell_data.HasState(ax::mojom::State::kInvisible));
    }
  }
}

// Verifies the bounding rect of each virtual accessibility child of the
// TableView (rows and cells) is updated appropriately as the table changes. For
// example, verifies that if a column is resized or hidden, the bounds are
// updated.
TEST_P(TableViewTest, UpdateVirtualAccessibilityChildrenBounds) {
  // Verify the bounds are updated correctly when the TableView and its widget
  // have been shown. Initially some widths would be 0 until the TableView's
  // bounds are fully set up, so make sure the virtual children bounds have been
  // updated and now match the expected bounds.
  auto expected_bounds = helper_->GenerateExpectedBounds();
  VerifyTableAccChildrenBounds(table_->GetViewAccessibility(), expected_bounds);
}

TEST_P(TableViewTest, UpdateVirtualAccessibilityChildrenBoundsWithResize) {
  // Resize the first column 10 pixels smaller and check the bounds are updated.
  int x = table_->GetVisibleColumn(0).width;
  PressLeftMouseAt(helper_->header(), gfx::Point(x, 0));
  DragLeftMouseTo(helper_->header(), gfx::Point(x - 10, 0));

  auto expected_bounds_after_resize = helper_->GenerateExpectedBounds();
  VerifyTableAccChildrenBounds(table_->GetViewAccessibility(),
                               expected_bounds_after_resize);
}

TEST_P(TableViewTest, AccessibleTableColumnCount) {
  const ViewAccessibility& view_accessibility = table_->GetViewAccessibility();
  ui::AXNodeData data;
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(helper_->visible_col_count(),
            static_cast<size_t>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kTableColumnCount)));

  table_->SetColumnVisibility(1, false);

  data = ui::AXNodeData();
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(helper_->visible_col_count(),
            static_cast<size_t>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kTableColumnCount)));

  std::vector<ui::TableColumn> new_columns(3);

  data = ui::AXNodeData();
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(helper_->visible_col_count(),
            static_cast<size_t>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kTableColumnCount)));
}

TEST_P(TableViewTest, AccessibleTableRowCount) {
  const ViewAccessibility& view_accessibility = table_->GetViewAccessibility();
  ui::AXNodeData data;
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(table_->GetRowCount(),
            static_cast<size_t>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount)));

  // Move rows to validate OnItemsMoved.
  model_->MoveRows(0, 1, 1);
  data = ui::AXNodeData();
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(table_->GetRowCount(),
            static_cast<size_t>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount)));

  // Change rows to validate OnItemsChanged.
  model_->ChangeRow(3, -1, 0);
  data = ui::AXNodeData();
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(table_->GetRowCount(),
            static_cast<size_t>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount)));

  // Remove rows to validate OnItemsRemoved.
  model_->RemoveRow(0);
  data = ui::AXNodeData();
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(table_->GetRowCount(),
            static_cast<size_t>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount)));

  // Add rows to validate OnItemsAdded
  model_->AddRows(1, 2, /*value_multiplier=*/10);
  data = ui::AXNodeData();
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(table_->GetRowCount(),
            static_cast<size_t>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount)));

  table_->OnModelChanged();
  data = ui::AXNodeData();
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(table_->GetRowCount(),
            static_cast<size_t>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount)));
}

TEST_P(TableViewTest, UpdateVirtualAccessibilityChildrenBoundsHideColumn) {
  // Hide 1 column and check the bounds are updated.
  table_->SetColumnVisibility(1, false);
  auto expected_bounds_after_hiding = helper_->GenerateExpectedBounds();
  VerifyTableAccChildrenBounds(table_->GetViewAccessibility(),
                               expected_bounds_after_hiding);
}

TEST_P(TableViewTest, GetVirtualAccessibilityBodyRow) {
  for (size_t i = 0; i < table_->GetRowCount(); ++i) {
    const AXVirtualView* row = helper_->GetVirtualAccessibilityBodyRow(i);
    ASSERT_TRUE(row);
    const ui::AXNodeData& row_data = row->GetData();
    EXPECT_EQ(ax::mojom::Role::kRow, row_data.role);
    EXPECT_EQ(i, static_cast<size_t>(row_data.GetIntAttribute(
                     ax::mojom::IntAttribute::kTableRowIndex)));
  }
}

TEST_P(TableViewTest, GetVirtualAccessibilityCell) {
  for (size_t i = 0; i < table_->GetRowCount(); ++i) {
    for (size_t j = 0; j < helper_->visible_col_count(); ++j) {
      const AXVirtualView* cell = helper_->GetVirtualAccessibilityCell(i, j);
      ASSERT_TRUE(cell);
      const ui::AXNodeData& cell_data = cell->GetData();
      EXPECT_EQ(ax::mojom::Role::kGridCell, cell_data.role);
      EXPECT_EQ(i, static_cast<size_t>(cell_data.GetIntAttribute(
                       ax::mojom::IntAttribute::kTableCellRowIndex)));
      EXPECT_EQ(j, static_cast<size_t>(cell_data.GetIntAttribute(
                       ax::mojom::IntAttribute::kTableCellColumnIndex)));
    }
  }
}

TEST_P(TableViewTest, ChangingCellFiresAccessibilityEvent) {
  int text_changed_count = 0;
  table_->GetViewAccessibility().set_accessibility_events_callback(
      base::BindLambdaForTesting(
          [&](const ui::AXPlatformNodeDelegate*, const ax::mojom::Event event) {
            if (event == ax::mojom::Event::kTextChanged)
              ++text_changed_count;
          }));

  // First we make sure that simply accessing the data will not fire the event.
  const AXVirtualView* cell = helper_->GetVirtualAccessibilityCell(0, 0);
  ASSERT_TRUE(cell);
  ui::AXNodeData cell_data;
  for (int i = 0; i < 100; ++i)
    cell_data = cell->GetData();
  EXPECT_EQ(0, text_changed_count);

  // A kTextChanged event is fired when a cell's data is changed and
  // its computed accessible text isn't the same as the previously cached
  // value.
  // Change the [3, 0] cell to [-1, 0].
  model_->ChangeRow(3, -1, 0);
  EXPECT_EQ(1, text_changed_count);
  text_changed_count = 0;

  // Ensure that "changing" the cell to the same value doesn't fire the event.
  model_->ChangeRow(3, -1, 0);
  EXPECT_EQ(0, text_changed_count);
}

// Verifies SetColumnVisibility().
TEST_P(TableViewTest, ColumnVisibility) {
  // Two columns should be visible.
  EXPECT_EQ(2u, helper_->visible_col_count());

  // Should do nothing (column already visible).
  table_->SetColumnVisibility(0, true);
  EXPECT_EQ(2u, helper_->visible_col_count());

  // Hide the first column.
  table_->SetColumnVisibility(0, false);
  ASSERT_EQ(1u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->GetVisibleColumn(0).column.id);
  EXPECT_EQ("rows=0 4 cols=0 1", helper_->GetPaintRegion(table_->bounds()));

  // Hide the second column.
  table_->SetColumnVisibility(1, false);
  EXPECT_EQ(0u, helper_->visible_col_count());

  // Show the second column.
  table_->SetColumnVisibility(1, true);
  ASSERT_EQ(1u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->GetVisibleColumn(0).column.id);
  EXPECT_EQ("rows=0 4 cols=0 1", helper_->GetPaintRegion(table_->bounds()));

  // Show the first column.
  table_->SetColumnVisibility(0, true);
  ASSERT_EQ(2u, helper_->visible_col_count());
  EXPECT_EQ(1, table_->GetVisibleColumn(0).column.id);
  EXPECT_EQ(0, table_->GetVisibleColumn(1).column.id);
  EXPECT_EQ("rows=0 4 cols=0 2", helper_->GetPaintRegion(table_->bounds()));
}

// Regression tests for https://crbug.com/1283805, and
// https://crbug.com/1283807.
TEST_P(TableViewTest, NoCrashesWithAllColumnsHidden) {
  // Set both initially visible columns hidden.
  table_->SetColumnVisibility(0, false);
  table_->SetColumnVisibility(1, false);
  EXPECT_EQ(0u, helper_->visible_col_count());

  // Remove and add rows in this state, there should be no crashes.
  model_->RemoveRow(0);
  model_->AddRows(1, 2, /*value_multiplier=*/10);
}

// Verifies resizing a column using the mouse works.
TEST_P(TableViewTest, Resize) {
  const int x = table_->GetVisibleColumn(0).width;
  EXPECT_NE(0, x);
  // Drag the mouse 1 pixel to the left.
  PressLeftMouseAt(helper_->header(), gfx::Point(x, 0));
  DragLeftMouseTo(helper_->header(), gfx::Point(x - 1, 0));

  // This should shrink the first column and pull the second column in.
  EXPECT_EQ(x - 1, table_->GetVisibleColumn(0).width);
  EXPECT_EQ(x - 1, table_->GetVisibleColumn(1).x);
}

// Verifies resizing a column works with a gesture.
TEST_P(TableViewTest, ResizeViaGesture) {
  const int x = table_->GetVisibleColumn(0).width;
  EXPECT_NE(0, x);
  // Drag the mouse 1 pixel to the left.
  ui::GestureEvent scroll_begin(
      x, 0, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
  helper_->header()->OnGestureEvent(&scroll_begin);
  ui::GestureEvent scroll_update(
      x - 1, 0, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate));
  helper_->header()->OnGestureEvent(&scroll_update);

  // This should shrink the first column and pull the second column in.
  EXPECT_EQ(x - 1, table_->GetVisibleColumn(0).width);
  EXPECT_EQ(x - 1, table_->GetVisibleColumn(1).x);
}

// Verifies resizing a column works with the keyboard.
// The resize keyboard amount is 5 pixels.
TEST_P(TableViewTest, ResizeViaKeyboard) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    GTEST_SKIP() << "platform doesn't support table keyboard navigation";
  }

  table_->RequestFocus();
  const int x = table_->GetVisibleColumn(0).width;
  EXPECT_NE(0, x);

  // Table starts off with no visible column being active.
  ASSERT_FALSE(helper_->GetActiveVisibleColumnIndex().has_value());

  ui::ListSelectionModel new_selection;
  new_selection.SetSelectedIndex(1);
  helper_->SetSelectionModel(new_selection);
  ASSERT_EQ(0u, helper_->GetActiveVisibleColumnIndex());

  PressKey(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN);
  // This should shrink the first column and pull the second column in.
  EXPECT_EQ(x - 5, table_->GetVisibleColumn(0).width);
  EXPECT_EQ(x - 5, table_->GetVisibleColumn(1).x);

  PressKey(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  // This should restore the columns to their original sizes.
  EXPECT_EQ(x, table_->GetVisibleColumn(0).width);
  EXPECT_EQ(x, table_->GetVisibleColumn(1).x);

  PressKey(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  // This should expand the first column and push the second column out.
  EXPECT_EQ(x + 5, table_->GetVisibleColumn(0).width);
  EXPECT_EQ(x + 5, table_->GetVisibleColumn(1).x);
}

// Verifies resizing a column won't reduce the column width below the width of
// the column's title text.
TEST_P(TableViewTest, ResizeHonorsMinimum) {
  const int x = table_->GetVisibleColumn(0).width;
  EXPECT_NE(0, x);

  PressLeftMouseAt(helper_->header(), gfx::Point(x, 0));
  DragLeftMouseTo(helper_->header(), gfx::Point(20, 0));

  int title_width = gfx::GetStringWidth(
      table_->GetVisibleColumn(0).column.title, helper_->font_list());
  EXPECT_LT(title_width, table_->GetVisibleColumn(0).width);

  int old_width = table_->GetVisibleColumn(0).width;
  DragLeftMouseTo(helper_->header(), gfx::Point(old_width + 10, 0));
  EXPECT_EQ(old_width + 10, table_->GetVisibleColumn(0).width);
}

// Assertions for table sorting.
TEST_P(TableViewTest, Sort) {
  // Initial ordering.
  EXPECT_TRUE(table_->sort_descriptors().empty());
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder("[0, 1], [1, 1], [2, 2], [3, 0]");

  // Toggle the sort order of the first column, shouldn't change anything.
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder("[0, 1], [1, 1], [2, 2], [3, 0]");

  // Toggle the sort (first column descending).
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 2 1 0", GetViewToModelAsString(table_));
  EXPECT_EQ("3 2 1 0", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder("[3, 0], [2, 2], [1, 1], [0, 1]");

  // Change the [3, 0] cell to [-1, 0]. This should move it to the back of
  // the current sort order.
  model_->ChangeRow(3, -1, 0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 1 0 3", GetViewToModelAsString(table_));
  EXPECT_EQ("2 1 0 3", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder("[2, 2], [1, 1], [0, 1], [-1, 0]");

  // Toggle the sort again, to clear the sort and restore the model ordering.
  table_->ToggleSortOrder(0);
  EXPECT_TRUE(table_->sort_descriptors().empty());
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder("[0, 1], [1, 1], [2, 2], [-1, 0]");

  // Toggle the sort again (first column ascending).
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 0 1 2", GetViewToModelAsString(table_));
  EXPECT_EQ("1 2 3 0", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder("[-1, 0], [0, 1], [1, 1], [2, 2]");

  // Add two rows that's second in the model order, but last in the active sort
  // order.
  model_->AddRows(1, 2, 10 /* Multiplier */);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("5 0 3 4 1 2", GetViewToModelAsString(table_));
  EXPECT_EQ("1 4 5 2 3 0", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder(
      "[-1, 0], [0, 1], [1, 1], [2, 2], [11, 11], [12, 12]");

  // Add a row that's last in the model order but second in the the active sort
  // order.
  model_->AddRow(5, -1, 20);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("5 6 0 3 4 1 2", GetViewToModelAsString(table_));
  EXPECT_EQ("2 5 6 3 4 0 1", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder(
      "[-1, 20], [-1, 0], [0, 1], [1, 1], [2, 2], [11, 11], [12, 12]");

  // Click the first column again, then click the second column. This should
  // yield an ordering of second column ascending, with the first column
  // descending as a tiebreaker.
  table_->ToggleSortOrder(0);
  table_->ToggleSortOrder(1);
  ASSERT_EQ(2u, table_->sort_descriptors().size());
  EXPECT_EQ(1, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ(0, table_->sort_descriptors()[1].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[1].ascending);
  EXPECT_EQ("6 3 0 4 1 2 5", GetViewToModelAsString(table_));
  EXPECT_EQ("2 4 5 1 3 6 0", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder(
      "[-1, 0], [1, 1], [0, 1], [2, 2], [11, 11], [12, 12], [-1, 20]");

  // Toggle the current column to change from ascending to descending. This
  // should result in an almost-reversal of the previous order, except for the
  // two rows with the same value for the second column.
  table_->ToggleSortOrder(1);
  ASSERT_EQ(2u, table_->sort_descriptors().size());
  EXPECT_EQ(1, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ(0, table_->sort_descriptors()[1].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[1].ascending);
  EXPECT_EQ("5 2 1 4 3 0 6", GetViewToModelAsString(table_));
  EXPECT_EQ("5 2 1 4 3 0 6", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder(
      "[-1, 20], [12, 12], [11, 11], [2, 2], [1, 1], [0, 1], [-1, 0]");

  // Delete the [0, 1] row from the model. It's at model index zero.
  model_->RemoveRow(0);
  ASSERT_EQ(2u, table_->sort_descriptors().size());
  EXPECT_EQ(1, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ(0, table_->sort_descriptors()[1].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[1].ascending);
  EXPECT_EQ("4 1 0 3 2 5", GetViewToModelAsString(table_));
  EXPECT_EQ("2 1 4 3 0 5", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder(
      "[-1, 20], [12, 12], [11, 11], [2, 2], [1, 1], [-1, 0]");

  // Delete [-1, 20] and [10, 11] from the model.
  model_->RemoveRows(1, 2);
  ASSERT_EQ(2u, table_->sort_descriptors().size());
  EXPECT_EQ(1, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ(0, table_->sort_descriptors()[1].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[1].ascending);
  EXPECT_EQ("2 0 1 3", GetViewToModelAsString(table_));
  EXPECT_EQ("1 2 0 3", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder("[-1, 20], [11, 11], [2, 2], [-1, 0]");

  // Toggle the current sort column again. This should clear both the primary
  // and secondary sort descriptor.
  table_->ToggleSortOrder(1);
  EXPECT_TRUE(table_->sort_descriptors().empty());
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));
  VerifyTableViewAndAXOrder("[11, 11], [2, 2], [-1, 20], [-1, 0]");
}

// Verifies clicking on the header sorts.
TEST_P(TableViewTest, SortOnMouse) {
  EXPECT_TRUE(table_->sort_descriptors().empty());

  const int x = table_->GetVisibleColumn(0).width / 2;
  EXPECT_NE(0, x);
  // Press and release the mouse.
  // The header must return true, else it won't normally get the release.
  EXPECT_TRUE(PressLeftMouseAt(helper_->header(), gfx::Point(x, 0)));
  ReleaseLeftMouseAt(helper_->header(), gfx::Point(x, 0));

  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
}

// Verifies that pressing the space bar when a particular visible column is
// active will sort by that column.
TEST_P(TableViewTest, SortOnSpaceBar) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    GTEST_SKIP() << "platform doesn't support table keyboard navigation";
  }

  table_->RequestFocus();
  ASSERT_TRUE(table_->sort_descriptors().empty());
  // Table starts off with no visible column being active.
  ASSERT_FALSE(helper_->GetActiveVisibleColumnIndex().has_value());

  ui::ListSelectionModel new_selection;
  new_selection.SetSelectedIndex(1);
  helper_->SetSelectionModel(new_selection);
  ASSERT_EQ(0u, helper_->GetActiveVisibleColumnIndex());

  PressKey(ui::VKEY_SPACE);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);

  PressKey(ui::VKEY_SPACE);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);

  PressKey(ui::VKEY_RIGHT);
  ASSERT_EQ(1u, helper_->GetActiveVisibleColumnIndex());

  PressKey(ui::VKEY_SPACE);
  ASSERT_EQ(2u, table_->sort_descriptors().size());
  EXPECT_EQ(1, table_->sort_descriptors()[0].column_id);
  EXPECT_EQ(0, table_->sort_descriptors()[1].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_FALSE(table_->sort_descriptors()[1].ascending);
}

TEST_P(TableViewTest, ActiveCellBoundsFollowColumnSorting) {
  table_->RequestFocus();
  ASSERT_TRUE(table_->sort_descriptors().empty());

  ui::ListSelectionModel new_selection;
  new_selection.SetSelectedIndex(1);
  helper_->SetSelectionModel(new_selection);

  // Toggle the sort order of the first column. Shouldn't change the order.
  table_->ToggleSortOrder(0);
  ClickOnRow(0, 0);
  EXPECT_EQ(helper_->GetCellBounds(0, 0), helper_->GetActiveCellBounds());
  EXPECT_EQ(0u, table_->ViewToModel(0));

  ClickOnRow(1, 0);
  EXPECT_EQ(helper_->GetCellBounds(1, 0), helper_->GetActiveCellBounds());
  EXPECT_EQ(1u, table_->ViewToModel(1));

  ClickOnRow(2, 0);
  EXPECT_EQ(helper_->GetCellBounds(2, 0), helper_->GetActiveCellBounds());
  EXPECT_EQ(2u, table_->ViewToModel(2));

  // Toggle the sort order of the second column. The active row will stay in
  // sync with the view index, meanwhile the model's change which shows that
  // the list order has changed.
  table_->ToggleSortOrder(1);
  ClickOnRow(0, 0);
  EXPECT_EQ(helper_->GetCellBounds(0, 0), helper_->GetActiveCellBounds());
  EXPECT_EQ(3u, table_->ViewToModel(0));

  ClickOnRow(1, 0);
  EXPECT_EQ(helper_->GetCellBounds(1, 0), helper_->GetActiveCellBounds());
  EXPECT_EQ(0u, table_->ViewToModel(1));

  ClickOnRow(2, 0);
  EXPECT_EQ(helper_->GetCellBounds(2, 0), helper_->GetActiveCellBounds());
  EXPECT_EQ(1u, table_->ViewToModel(2));

  // Verifying invalid active indexes return an empty rect.
  new_selection.Clear();
  helper_->SetSelectionModel(new_selection);
  EXPECT_EQ(gfx::Rect(), helper_->GetActiveCellBounds());
}

TEST_P(TableViewTest, Tooltip) {
  // Column 0 uses the TableModel's GetTooltipText override for tooltips.
  table_->SetVisibleColumnWidth(0, 10);
  auto local_point_for_row = [&](int row) {
    return gfx::Point(5, (row + 0.5) * table_->GetRowHeight());
  };
  auto expected = [](int row) {
    return u"Tooltip" + base::NumberToString16(row);
  };
  EXPECT_EQ(expected(0), table_->GetTooltipText(local_point_for_row(0)));
  EXPECT_EQ(expected(1), table_->GetTooltipText(local_point_for_row(1)));
  EXPECT_EQ(expected(2), table_->GetTooltipText(local_point_for_row(2)));

  // Hovering another column will return that cell's text instead.
  const gfx::Point point(15, local_point_for_row(0).y());
  EXPECT_EQ(model_->GetText(0, 1), table_->GetTooltipText(point));
}

namespace {

class TableGrouperImpl : public TableGrouper {
 public:
  TableGrouperImpl() = default;

  TableGrouperImpl(const TableGrouperImpl&) = delete;
  TableGrouperImpl& operator=(const TableGrouperImpl&) = delete;

  void SetRanges(const std::vector<size_t>& ranges) { ranges_ = ranges; }

  // TableGrouper overrides:
  void GetGroupRange(size_t model_index, GroupRange* range) override {
    size_t offset = 0;
    size_t range_index = 0;
    for (; range_index < ranges_.size() && offset < model_index; ++range_index)
      offset += ranges_[range_index];

    if (offset == model_index) {
      range->start = model_index;
      range->length = ranges_[range_index];
    } else {
      range->start = offset - ranges_[range_index - 1];
      range->length = ranges_[range_index - 1];
    }
  }

 private:
  std::vector<size_t> ranges_;
};

}  // namespace

// Assertions around grouping.
TEST_P(TableViewTest, Grouping) {
  // Configure the grouper so that there are two groups:
  // A 0
  //   1
  // B 2
  //   3
  TableGrouperImpl grouper;
  grouper.SetRanges({2, 2});
  table_->SetGrouper(&grouper);

  // Toggle the sort order of the first column, shouldn't change anything.
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));

  // Sort descending, resulting:
  // B 2
  //   3
  // A 0
  //   1
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 3 0 1", GetViewToModelAsString(table_));
  EXPECT_EQ("2 3 0 1", GetModelToViewAsString(table_));

  // Change the entry in the 4th row to -1. The model now becomes:
  // A 0
  //   1
  // B 2
  //   -1
  // Since the first entry in the range didn't change the sort isn't impacted.
  model_->ChangeRow(3, -1, 0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 3 0 1", GetViewToModelAsString(table_));
  EXPECT_EQ("2 3 0 1", GetModelToViewAsString(table_));

  // Change the entry in the 3rd row to -1. The model now becomes:
  // A 0
  //   1
  // B -1
  //   -1
  model_->ChangeRow(2, -1, 0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));

  // Toggle to clear the sort.
  table_->ToggleSortOrder(0);
  EXPECT_TRUE(table_->sort_descriptors().empty());
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));

  // Toggle again to effect an ascending sort.
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 3 0 1", GetViewToModelAsString(table_));
  EXPECT_EQ("2 3 0 1", GetModelToViewAsString(table_));
}

TEST_P(TableViewTest, VirtualAccessibilitySetSelectionAll) {
  table_->SetSelectionAll(true);
  // Set only the first column as the active column.
  table_->SetActiveVisibleColumnIndex(0);

  for (size_t i = 0; i < table_->GetRowCount(); ++i) {
    const AXVirtualView* row = helper_->GetVirtualAccessibilityBodyRow(i);
    ASSERT_TRUE(row);
    const ui::AXNodeData& row_data = row->GetData();
    // Make sure all rows are selected.
    EXPECT_TRUE(row_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    for (size_t j = 0; j < helper_->visible_col_count(); ++j) {
      const AXVirtualView* cell = helper_->GetVirtualAccessibilityCell(i, j);
      ASSERT_TRUE(cell);
      const ui::AXNodeData& cell_data = cell->GetData();
      // Only the cells in the first column should be selected.
      if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell && j == 0) {
        EXPECT_TRUE(
            cell_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
      } else {
        EXPECT_FALSE(
            cell_data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected));
      }
    }
  }
}

TEST_P(TableViewTest, VirtualAccessibilitySetSelectionRowsInRange) {
  // Configure the grouper so that there are two groups:
  // A 0
  //   1
  // B 2
  //   3
  TableGrouperImpl grouper;
  grouper.SetRanges({2, 2});
  table_->SetGrouper(&grouper);
  ui::ListSelectionModel new_selection;
  // Should only select the second group.
  helper_->SelectRowsInRangeFrom(2, true, &new_selection);
  helper_->SetSelectionModel(new_selection);
  // Set only the second column as the active column.
  table_->SetActiveVisibleColumnIndex(1);

  for (size_t i = 0; i < table_->GetRowCount(); ++i) {
    const AXVirtualView* row = helper_->GetVirtualAccessibilityBodyRow(i);
    ASSERT_TRUE(row);
    const ui::AXNodeData& row_data = row->GetData();
    if (i == 2 || i == 3) {
      EXPECT_TRUE(
          row_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    } else {
      EXPECT_FALSE(
          row_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    }
    for (size_t j = 0; j < helper_->visible_col_count(); ++j) {
      const AXVirtualView* cell = helper_->GetVirtualAccessibilityCell(i, j);
      ASSERT_TRUE(cell);
      const ui::AXNodeData& cell_data = cell->GetData();
      if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell && j == 1 &&
          (i == 2 || i == 3)) {
        EXPECT_TRUE(
            cell_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
      } else {
        EXPECT_FALSE(
            cell_data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected));
      }
    }
  }

  ui::ListSelectionModel new_selection2;
  // Unselect the selected cells.
  helper_->SelectRowsInRangeFrom(2, false, &new_selection2);
  helper_->SetSelectionModel(new_selection2);

  for (size_t i = 0; i < table_->GetRowCount(); ++i) {
    const AXVirtualView* row = helper_->GetVirtualAccessibilityBodyRow(i);
    ASSERT_TRUE(row);
    const ui::AXNodeData& row_data = row->GetData();
    EXPECT_FALSE(
        row_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    for (size_t j = 0; j < helper_->visible_col_count(); ++j) {
      const AXVirtualView* cell = helper_->GetVirtualAccessibilityCell(i, j);
      ASSERT_TRUE(cell);
      const ui::AXNodeData& cell_data = cell->GetData();
      EXPECT_FALSE(
          cell_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    }
  }
}

TEST_P(TableViewTest, VirtualAccessibilitySelectOnRemove) {
  table_->Select(2);
  model_->RemoveRow(2);

  for (size_t i = 0; i < table_->GetRowCount(); ++i) {
    const AXVirtualView* row = helper_->GetVirtualAccessibilityBodyRow(i);
    ASSERT_TRUE(row);
    const ui::AXNodeData& row_data = row->GetData();
    // Only the new row at index 2 should be selected.
    if (i == 2) {
      EXPECT_TRUE(
          row_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    } else {
      EXPECT_FALSE(
          row_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
    }
  }
}

namespace {

class TableViewObserverImpl : public TableViewObserver {
 public:
  explicit TableViewObserverImpl(TableView* view) {
    observation_.Observe(view);
  }

  ~TableViewObserverImpl() override = default;

  TableViewObserverImpl(const TableViewObserverImpl&) = delete;
  TableViewObserverImpl& operator=(const TableViewObserverImpl&) = delete;

  int GetChangedCountAndClear() {
    const int count = selection_changed_count_;
    selection_changed_count_ = 0;
    return count;
  }

  // TableViewObserver overrides:
  void OnSelectionChanged() override { selection_changed_count_++; }

 private:
  const raw_ptr<TableView> view_;
  int selection_changed_count_ = 0;
  base::ScopedObservation<TableView, TableViewObserver> observation_{this};
};

}  // namespace

// Assertions around changing the selection.
TEST_P(TableViewTest, Selection) {
  TableViewObserverImpl observer(table_);

  // Initially no selection.
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  // Select the last row.
  table_->Select(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Change sort, shouldn't notify of change (toggle twice so that order
  // actually changes).
  table_->ToggleSortOrder(0);
  table_->ToggleSortOrder(0);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Remove the selected row, this should notify of a change and update the
  // selection.
  model_->RemoveRow(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  // Insert a row, since the selection in terms of the original model hasn't
  // changed the observer is not notified.
  model_->AddRow(0, 1, 2);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Swap the first two rows. This shouldn't affect selection.
  model_->MoveRows(0, 1, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Move the first row to after the selection. This will change the selection
  // state.
  model_->MoveRows(0, 1, 3);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  // Move the first two rows to be after the selection. This will change the
  // selection state.
  model_->MoveRows(0, 2, 2);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  // Move some rows after the selection.
  model_->MoveRows(2, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  // Move the selection itself.
  model_->MoveRows(0, 1, 3);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Move-left a range that ends at the selection
  model_->MoveRows(2, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  // Move-right a range that ends at the selection
  model_->MoveRows(1, 2, 2);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Add a row at the end.
  model_->AddRow(4, 7, 9);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Move-left a range that includes the selection.
  model_->MoveRows(2, 3, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  // Move-right a range that includes the selection.
  model_->MoveRows(0, 4, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Insert two rows to the selection.
  model_->AddRows(2, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=5 anchor=5 selection=5", SelectionStateAsString());

  // Remove two rows which include the selection.
  model_->RemoveRows(4, 2);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());
}

TEST_P(TableViewTest, SelectAll) {
  TableViewObserverImpl observer(table_);

  // Initially no selection.
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  table_->SetSelectionAll(/*select=*/true);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=<none> anchor=<none> selection=0 1 2 3",
            SelectionStateAsString());

  table_->Select(2);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  table_->SetSelectionAll(/*select=*/true);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=0 1 2 3", SelectionStateAsString());

  table_->SetSelectionAll(/*select=*/false);
  EXPECT_EQ("active=2 anchor=2 selection=", SelectionStateAsString());
}

TEST_P(TableViewTest, RemoveUnselectedRows) {
  TableViewObserverImpl observer(table_);

  // Select a middle row.
  table_->Select(2);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  // Remove the last row. This should notify of a change.
  model_->RemoveRow(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  // Remove the first row. This should also notify of a change.
  model_->RemoveRow(0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());
}

TEST_P(TableViewTest, AddingRemovingMultipleRows) {
  VerifyTableViewAndAXOrder("[0, 1], [1, 1], [2, 2], [3, 0]");

  model_->AddRows(0, 3, 10);

  VerifyTableViewAndAXOrder(
      "[10, 10], [11, 11], [12, 12], [0, 1], [1, 1], [2, 2], [3, 0]");

  model_->RemoveRows(4, 3);

  VerifyTableViewAndAXOrder("[10, 10], [11, 11], [12, 12], [0, 1]");
}

// 0 1 2 3:
// select 3 -> 0 1 2 [3]
// remove 3 -> 0 1 2 (none selected)
// select 1 -> 0 [1] 2
// remove 1 -> 0 1 (none selected)
// select 0 -> [0] 1
// remove 0 -> 0 (none selected)
TEST_P(TableViewTest, SelectionNoSelectOnRemove) {
  TableViewObserverImpl observer(table_);
  table_->SetSelectOnRemove(false);

  // Initially no selection.
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  // Select row 3.
  table_->Select(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Remove the selected row, this should notify of a change and since the
  // select_on_remove_ is set false, and the removed item is the previously
  // selected item, so no item is selected.
  model_->RemoveRow(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  // Select row 1.
  table_->Select(1);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  // Remove the selected row.
  model_->RemoveRow(1);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  // Select row 0.
  table_->Select(0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  // Remove the selected row.
  model_->RemoveRow(0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC)
// Verifies selection works by way of a gesture.
TEST_P(TableViewTest, SelectOnTap) {
  // Initially no selection.
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  TableViewObserverImpl observer(table_);

  // Tap on the first row, should select it and focus the table.
  EXPECT_FALSE(table_->HasFocus());
  TapOnRow(0);
  EXPECT_TRUE(table_->HasFocus());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());
}
#endif

// Verifies up/down correctly navigate through groups.
TEST_P(TableViewTest, KeyUpDown) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  grouper.SetRanges({2, 1, 2});
  table_->SetGrouper(&grouper);

  TableViewObserverImpl observer(table_);
  table_->RequestFocus();

  // Initially no selection.
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  // Key up at this point will clear selection and navigate into the header.
  EXPECT_FALSE(table_->header_row_is_active());
  PressKey(ui::VKEY_UP);
  EXPECT_TRUE(table_->header_row_is_active());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  // Sort the table descending by column 1, view now looks like:
  // B 5   model: 2
  // C 2          3
  //   3          4
  // A 0          0
  //   1          1
  table_->ToggleSortOrder(0);
  table_->ToggleSortOrder(0);

  EXPECT_EQ("2 3 4 0 1", GetViewToModelAsString(table_));

  table_->Select(std::nullopt);
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  observer.GetChangedCountAndClear();

  // Up with nothing selected moves selection into the table's header.
  EXPECT_FALSE(table_->header_row_is_active());
  PressKey(ui::VKEY_UP);
  EXPECT_TRUE(table_->header_row_is_active());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  // Key up at this point will clear selection and navigate into the header.
  EXPECT_FALSE(table_->header_row_is_active());
  PressKey(ui::VKEY_UP);
  EXPECT_TRUE(table_->header_row_is_active());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());
}

// Verifies left/right correctly navigate through visible columns.
TEST_P(TableViewTest, KeyLeftRight) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    GTEST_SKIP() << "platform doesn't support table keyboard navigation";
  }

  TableViewObserverImpl observer(table_);
  table_->RequestFocus();

  // Initially no active visible column.
  EXPECT_FALSE(helper_->GetActiveVisibleColumnIndex().has_value());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  helper_->SetSelectionModel(ui::ListSelectionModel());
  EXPECT_FALSE(helper_->GetActiveVisibleColumnIndex().has_value());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(1u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(1u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  ui::ListSelectionModel new_selection;
  new_selection.SetSelectedIndex(1);
  helper_->SetSelectionModel(new_selection);
  EXPECT_EQ(1u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  table_->SetColumnVisibility(0, false);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  // Since the first column was hidden, the active visible column should not
  // advance.
  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  // If visibility to the first column is restored, the active visible column
  // should be unchanged because columns are always added to the end.
  table_->SetColumnVisibility(0, true);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());
  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(1u, helper_->GetActiveVisibleColumnIndex());

  // If visibility to the first column is removed, the active visible column
  // should be decreased by one.
  table_->SetColumnVisibility(0, false);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  table_->SetColumnVisibility(0, true);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(1u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());
}

// Verify table view that the left/right navigation scrolls the visible rect
// correctly.
TEST_P(TableViewTest, KeyLeftRightScrollRectToVisibleInTableView) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    GTEST_SKIP() << "platform doesn't support table keyboard navigation";
  }

  table_->RequestFocus();
  EXPECT_EQ(2u, helper_->visible_col_count());
  // Initially no active visible column.
  EXPECT_FALSE(helper_->GetActiveVisibleColumnIndex().has_value());
  EXPECT_FALSE(scroll_view_->horizontal_scroll_bar()->GetVisible());
  scroll_view_->SetBounds(0, 0, 800, 800);
  // Set the column width to make the horizontal scroll bar visible.
  constexpr int kColumn0Width = 500;
  constexpr int kColumn1Width = 1000;
  table_->SetVisibleColumnWidth(0, kColumn0Width);
  table_->SetVisibleColumnWidth(1, kColumn1Width);
  test::RunScheduledLayout(scroll_view_);
  EXPECT_EQ(table_->GetVisibleColumn(0).width, kColumn0Width);
  EXPECT_EQ(table_->GetVisibleColumn(1).width, kColumn1Width);
  EXPECT_TRUE(scroll_view_->horizontal_scroll_bar()->GetVisible());

  gfx::Rect visible_bounds = table_->GetVisibleBounds();
  PressKey(ui::VKEY_RIGHT);
  test::RunScheduledLayout(scroll_view_);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());

  PressKey(ui::VKEY_RIGHT);
  test::RunScheduledLayout(scroll_view_);
  EXPECT_EQ(1u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(table_->GetVisibleBounds(),
            gfx::Rect(kColumn0Width, visible_bounds.y(), visible_bounds.width(),
                      visible_bounds.height()));

  PressKey(ui::VKEY_LEFT);
  test::RunScheduledLayout(scroll_view_);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());
}

// Verify table header that the left/right navigation scrolls the visible rect
// correctly.
TEST_P(TableViewTest, KeyLeftRightScrollRectToVisibleInTableHeader) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    GTEST_SKIP() << "platform doesn't support table keyboard navigation";
  }

  table_->RequestFocus();
  SetColumnWidthForHorizontalScrollBarVisibility();
  gfx::Rect visible_bounds = table_->GetVisibleBounds();

  // Navigate to the table header
  PressKey(ui::VKEY_RIGHT);
  EXPECT_FALSE(table_->header_row_is_active());
  PressKey(ui::VKEY_UP);
  EXPECT_TRUE(table_->header_row_is_active());

  test::RunScheduledLayout(scroll_view_);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());

  PressKey(ui::VKEY_RIGHT);
  test::RunScheduledLayout(scroll_view_);
  EXPECT_EQ(1u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(table_->GetVisibleBounds(),
            gfx::Rect(table_->GetVisibleColumn(0).width, visible_bounds.y(),
                      visible_bounds.width(), visible_bounds.height()));

  PressKey(ui::VKEY_LEFT);
  test::RunScheduledLayout(scroll_view_);
  EXPECT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());
}

// Verify that the table view visible bounds remains stable when up/down
// switching between different rows when the layout is RTL or LTR. use_rtl()
// returns true for testing the RTL layout and false for testing the LTR layout
TEST_P(TableViewTest, KeyUpDownHorizontalScrollbarStability) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    GTEST_SKIP() << "platform doesn't support table keyboard navigation";
  }
  EXPECT_FALSE(base::i18n::IsRTL());
  if (use_rtl()) {
    base::i18n::SetRTLForTesting(true);
    EXPECT_TRUE(base::i18n::IsRTL());
  }
  table_->RequestFocus();
  SetColumnWidthForHorizontalScrollBarVisibility();
  gfx::Rect visible_bounds = table_->GetVisibleBounds();
  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1u, table_->ViewToModel(1));
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(2u, table_->ViewToModel(2));
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1u, table_->ViewToModel(1));
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());
  if (use_rtl()) {
    base::i18n::SetRTLForTesting(false);
  }
  EXPECT_FALSE(base::i18n::IsRTL());
}

// Verify that the table view visible boundsr remains stable when clicking on
// different rows when the layout is RTL or LTR. use_rtl() returns true for
// testing the RTL layout and false for testing the LTR layout
TEST_P(TableViewTest, ClickRowHorizontalScrollbarStability) {
  EXPECT_FALSE(base::i18n::IsRTL());
  if (use_rtl()) {
    base::i18n::SetRTLForTesting(true);
    EXPECT_TRUE(base::i18n::IsRTL());
  }
  table_->RequestFocus();
  SetColumnWidthForHorizontalScrollBarVisibility();
  gfx::Rect visible_bounds = table_->GetVisibleBounds();
  ClickOnRow(1, 0);
  EXPECT_EQ(1u, table_->ViewToModel(1));
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());

  ClickOnRow(2, 0);
  EXPECT_EQ(2u, table_->ViewToModel(2));
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());

  ClickOnRow(3, 0);
  EXPECT_EQ(3u, table_->ViewToModel(3));
  EXPECT_EQ(visible_bounds, table_->GetVisibleBounds());
  if (use_rtl()) {
    base::i18n::SetRTLForTesting(false);
  }
  EXPECT_FALSE(base::i18n::IsRTL());
}

// Verifies home/end do the right thing.
TEST_P(TableViewTest, HomeEnd) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  grouper.SetRanges({2, 1, 2});
  table_->SetGrouper(&grouper);

  TableViewObserverImpl observer(table_);
  table_->RequestFocus();

  // Initially no selection.
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_END);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());
}

// Verifies multiple selection gestures work (control-click, shift-click ...).
TEST_P(TableViewTest, Multiselection) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  grouper.SetRanges({2, 1, 2});
  table_->SetGrouper(&grouper);

  // Initially no selection.
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  TableViewObserverImpl observer(table_);

  // Click on the first row, should select it and the second row.
  ClickOnRow(0, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  // Click on the last row, should select it and the row before it.
  ClickOnRow(4, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  // Shift click on the third row, should extend selection to it.
  ClickOnRow(2, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=4 selection=2 3 4", SelectionStateAsString());

  // Control click on third row, should toggle it.
  ClickOnRow(2, ui::EF_PLATFORM_ACCELERATOR);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=3 4", SelectionStateAsString());

  // Control-shift click on second row, should extend selection to it.
  ClickOnRow(1, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=2 selection=0 1 2 3 4", SelectionStateAsString());

  // Click on last row again.
  ClickOnRow(4, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());
}

// Verifies multiple selection gestures work when sorted.
TEST_P(TableViewTest, MultiselectionWithSort) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  grouper.SetRanges({2, 1, 2});
  table_->SetGrouper(&grouper);

  // Sort the table descending by column 1, view now looks like:
  // B 5   model: 2
  // C 2          3
  //   3          4
  // A 0          0
  //   1          1
  table_->ToggleSortOrder(0);
  table_->ToggleSortOrder(0);

  // Initially no selection.
  EXPECT_EQ("active=<none> anchor=<none> selection=", SelectionStateAsString());

  TableViewObserverImpl observer(table_);

  // Click on the third row, should select it and the second row.
  ClickOnRow(2, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  // Extend selection to first row.
  ClickOnRow(0, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=4 selection=2 3 4", SelectionStateAsString());
}

TEST_P(TableViewTest, MoveRowsWithMultipleSelection) {
  model_->AddRow(3, 77, 0);

  // Hide column 1.
  table_->SetColumnVisibility(1, false);

  TableViewObserverImpl observer(table_);

  // Select three rows.
  ClickOnRow(2, 0);
  ClickOnRow(4, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(2, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=2 selection=2 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder("[0], [1], [2], [77], [3]");

  // Move the unselected rows to the middle of the current selection. None of
  // the move operations should affect the view order.
  model_->MoveRows(0, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=0 selection=0 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder("[2], [0], [1], [77], [3]");

  // Move the unselected rows to the end of the current selection.
  model_->MoveRows(1, 2, 3);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=0 selection=0 1 2", SelectionStateAsString());
  VerifyTableViewAndAXOrder("[2], [77], [3], [0], [1]");

  // Move the unselected rows back to the middle of the selection.
  model_->MoveRows(3, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=0 selection=0 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder("[2], [0], [1], [77], [3]");

  // Swap the unselected rows.
  model_->MoveRows(1, 1, 2);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=0 selection=0 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder("[2], [1], [0], [77], [3]");

  // Move the second unselected row to be between two selected rows.
  model_->MoveRows(2, 1, 3);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=0 selection=0 2 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder("[2], [1], [77], [0], [3]");

  // Move the three middle rows to the beginning, including one selected row.
  model_->MoveRows(1, 3, 0);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=3 selection=1 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder("[1], [77], [0], [2], [3]");
}

TEST_P(TableViewTest, MoveRowsWithMultipleSelectionAndSort) {
  model_->AddRow(3, 77, 0);

  // Sort ascending by column 0, and hide column 1. The view order should not
  // change during this test.
  table_->ToggleSortOrder(0);
  table_->SetColumnVisibility(1, false);
  const char* kViewOrder = "[0], [1], [2], [3], [77]";
  VerifyTableViewAndAXOrder(kViewOrder);

  TableViewObserverImpl observer(table_);

  // Select three rows.
  ClickOnRow(2, 0);
  ClickOnRow(4, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(2, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=2 selection=2 3 4", SelectionStateAsString());

  // Move the unselected rows to the middle of the current selection. None of
  // the move operations should affect the view order.
  model_->MoveRows(0, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=0 selection=0 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder(kViewOrder);

  // Move the unselected rows to the end of the current selection.
  model_->MoveRows(1, 2, 3);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=0 selection=0 1 2", SelectionStateAsString());
  VerifyTableViewAndAXOrder(kViewOrder);

  // Move the unselected rows back to the middle of the selection.
  model_->MoveRows(3, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=0 selection=0 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder(kViewOrder);

  // Swap the unselected rows.
  model_->MoveRows(1, 1, 2);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=0 selection=0 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder(kViewOrder);

  // Swap the unselected rows again.
  model_->MoveRows(2, 1, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=0 selection=0 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder(kViewOrder);

  // Move the unselected rows back to the beginning.
  model_->MoveRows(1, 2, 0);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=2 selection=2 3 4", SelectionStateAsString());
  VerifyTableViewAndAXOrder(kViewOrder);
}

// Verifies we don't crash after removing the selected row when there is
// sorting and the anchor/active index also match the selected row.
TEST_P(TableViewTest, FocusAfterRemovingAnchor) {
  table_->ToggleSortOrder(0);

  ui::ListSelectionModel new_selection;
  new_selection.AddIndexToSelection(0);
  new_selection.AddIndexToSelection(1);
  new_selection.set_active(0);
  new_selection.set_anchor(0);
  helper_->SetSelectionModel(new_selection);
  model_->RemoveRow(0);
  table_->RequestFocus();
}

// OnItemsRemoved() should ensure view-model mappings are updated in response to
// the table model change before these view-model mappings are used.
// Test for (https://crbug.com/1173373).
TEST_P(TableViewTest, RemovingSortedRowsDoesNotCauseOverflow) {
  // Ensure the table has a sort descriptor set so that `view_to_model_` and
  // `model_to_view_` mappings are established and are in descending order. Do
  // this so the first view row maps to the last model row.
  table_->ToggleSortOrder(0);
  table_->ToggleSortOrder(0);
  ASSERT_TRUE(table_->GetIsSorted());
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 2 1 0", GetViewToModelAsString(table_));
  EXPECT_EQ("3 2 1 0", GetModelToViewAsString(table_));

  // Removing rows can result in callbacks to GetTooltipText(). Above mappings
  // will cause TableView to try to access the text for the first view row and
  // consequently attempt to access the last element in the model via the
  // `view_to_model_` mapping. This will result in a crash if the view-model
  // mappings have not been appropriately updated.
  model_->SetTooltip(u"");
  model_->RemoveRow(0);
  model_->RemoveRow(0);
  model_->RemoveRow(0);
  model_->RemoveRow(0);
}

// Ensure that the TableView's header row is keyboard accessible.
// Tests for crbug.com/1189851.
TEST_P(TableViewTest, TableHeaderRowAccessibleViewFocusable) {
  ASSERT_NE(nullptr, helper_->header());
  table_->RequestFocus();
  RunPendingMessages();

  // If no table body row has selection the TableView itself is focused and
  // there is no focused virtual view.
  EXPECT_TRUE(table_->HasFocus());
  EXPECT_FALSE(table_->header_row_is_active());
  EXPECT_EQ(nullptr, table_->GetViewAccessibility().FocusedVirtualChild());

  // Hitting the up arrow key should give the header focus and make it active.
  PressKey(ui::VKEY_UP);
  RunPendingMessages();
  EXPECT_TRUE(table_->HasFocus());
  EXPECT_TRUE(table_->header_row_is_active());
  EXPECT_EQ(helper_->GetVirtualAccessibilityHeaderRow(),
            table_->GetViewAccessibility().FocusedVirtualChild());

  // Hitting the down arrow key should move focus back into the body.
  PressKey(ui::VKEY_DOWN);
  RunPendingMessages();
  EXPECT_TRUE(table_->HasFocus());
  EXPECT_FALSE(table_->header_row_is_active());
  EXPECT_NE(helper_->GetVirtualAccessibilityHeaderRow(),
            table_->GetViewAccessibility().FocusedVirtualChild());
}

// Ensure that the TableView's header columns are keyboard accessible.
// Tests for crbug.com/1189851.
TEST_P(TableViewTest, TableHeaderColumnAccessibleViewsFocusable) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    GTEST_SKIP() << "platform doesn't support table keyboard navigation";
  }

  ASSERT_NE(nullptr, helper_->header());
  table_->RequestFocus();
  RunPendingMessages();

  // Hitting the up arrow key should give the header focus and make it active.
  auto& view_accessibility = table_->GetViewAccessibility();
  PressKey(ui::VKEY_UP);
  RunPendingMessages();
  EXPECT_TRUE(table_->HasFocus());
  EXPECT_TRUE(table_->header_row_is_active());
  EXPECT_EQ(helper_->GetVirtualAccessibilityHeaderRow(),
            view_accessibility.FocusedVirtualChild());

  // Navigating with arrow keys should move focus between TableView header
  // columns.
  PressKey(ui::VKEY_RIGHT);
  RunPendingMessages();
  ASSERT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(helper_->GetVirtualAccessibilityHeaderCell(0),
            view_accessibility.FocusedVirtualChild());

  PressKey(ui::VKEY_RIGHT);
  RunPendingMessages();
  ASSERT_EQ(1u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(helper_->GetVirtualAccessibilityHeaderCell(1),
            view_accessibility.FocusedVirtualChild());

  PressKey(ui::VKEY_LEFT);
  RunPendingMessages();
  ASSERT_EQ(0u, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(helper_->GetVirtualAccessibilityHeaderCell(0),
            view_accessibility.FocusedVirtualChild());
}

class TableViewFocusTest : public TableViewTest {
 public:
  TableViewFocusTest() = default;

  TableViewFocusTest(const TableViewFocusTest&) = delete;
  TableViewFocusTest& operator=(const TableViewFocusTest&) = delete;

  TestFocusChangeListener* listener() { return &listener_; }

 protected:
  WidgetDelegate* ConfigureWidgetDelegate() override {
    delegate_.RegisterDeleteDelegateCallback(base::BindOnce(
        &TableViewFocusTest::OnDeleteDelegate, base::Unretained(this)));
    delegate_.RegisterWidgetInitializedCallback(base::BindOnce(
        &TableViewFocusTest::OnWidgetInitialized, base::Unretained(this)));
    return &delegate_;
  }

  void OnWidgetInitialized() {
    delegate_.GetWidget()->GetFocusManager()->AddFocusChangeListener(
        &listener_);
  }

  void OnDeleteDelegate() {
    delegate_.GetWidget()->GetFocusManager()->RemoveFocusChangeListener(
        &listener_);
  }

 private:
  WidgetDelegate delegate_;
  TestFocusChangeListener listener_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    TableViewFocusTest,
    testing::Combine(/*use_default_construction=*/testing::Bool(),
                     /*use_rtl=*/testing::Bool()));

// Verifies that the active focus is cleared when the widget is destroyed.
// In MD mode, if that doesn't happen a DCHECK in View::DoRemoveChildView(...)
// will trigger due to an attempt to modify the child view list while iterating.
TEST_P(TableViewFocusTest, FocusClearedDuringWidgetDestruction) {
  table_->RequestFocus();

  ASSERT_EQ(1u, listener()->focus_changes().size());
  EXPECT_EQ(listener()->focus_changes()[0], ViewPair(nullptr, table_));
  listener()->ClearFocusChanges();

  // Now destroy the widget. This should *not* cause a DCHECK in
  // View::DoRemoveChildView(...).
  widget_->Close();
  ASSERT_EQ(1u, listener()->focus_changes().size());
  EXPECT_EQ(listener()->focus_changes()[0], ViewPair(table_, nullptr));
}

class TableViewDefaultConstructabilityTest : public ViewsTestBase {
 public:
  TableViewDefaultConstructabilityTest() = default;
  TableViewDefaultConstructabilityTest(
      const TableViewDefaultConstructabilityTest&) = delete;
  TableViewDefaultConstructabilityTest& operator=(
      const TableViewDefaultConstructabilityTest&) = delete;
  ~TableViewDefaultConstructabilityTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  Widget* widget() { return widget_.get(); }

 private:
  UniqueWidgetPtr widget_;
};

TEST_F(TableViewDefaultConstructabilityTest, TestFunctionalWithoutModel) {
  auto scroll_view =
      TableView::CreateScrollViewWithTable(std::make_unique<TableView>());
  scroll_view->SetBounds(0, 0, 10000, 10000);
  widget()->client_view()->AddChildView(std::move(scroll_view));
}

class TestTableModel3 : public TestTableModel2 {
 public:
  TestTableModel3() {
    icon_.allocN32Pixels(48, 48);
    SkCanvas canvas(icon_, SkSurfaceProps{});
    canvas.drawColor(SK_ColorRED);
  }

  TestTableModel3(const TestTableModel3&) = delete;
  TestTableModel3& operator=(const TestTableModel3&) = delete;
  ui::ImageModel GetIcon(size_t row) override {
    return ui::ImageModel::FromImageSkia(
        gfx::ImageSkia::CreateFrom1xBitmap(icon_));
  }

 private:
  SkBitmap icon_;
};

// The test calculation paint icon bounds.
class TableViewPaintIconBoundsTest : public ViewsTestBase {
 public:
  TableViewPaintIconBoundsTest() = default;
  TableViewPaintIconBoundsTest(const TableViewPaintIconBoundsTest&) = delete;
  TableViewPaintIconBoundsTest& operator=(const TableViewPaintIconBoundsTest&) =
      delete;
  ~TableViewPaintIconBoundsTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    model_ = std::make_unique<TestTableModel3>();
    std::vector<ui::TableColumn> columns(2);
    columns[0].title = u"A";
    columns[0].sortable = true;
    columns[1].title = u"B";
    columns[1].id = 1;
    columns[1].sortable = true;

    auto table = std::make_unique<TableView>(model_.get(), columns,
                                             TableType::kTextOnly, false);
    table_ = table.get();
    auto scroll_view = TableView::CreateScrollViewWithTable(std::move(table));
    scroll_view->SetBounds(0, 0, 1000, 1000);
    helper_ = std::make_unique<TableViewTestHelper>(table_);

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    test::RunScheduledLayout(
        widget_->GetRootView()->AddChildView(std::move(scroll_view)));
    widget_->Show();
  }

  void TearDown() override {
    table_ = nullptr;
    helper_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<TestTableModel2> model_;
  raw_ptr<TableView> table_ = nullptr;
  std::unique_ptr<TableViewTestHelper> helper_;
  UniqueWidgetPtr widget_;
};

TEST_F(TableViewPaintIconBoundsTest, TestPaintIconBoundsForNormally) {
  EXPECT_EQ(2u, helper_->visible_col_count());
  int cell_margin = helper_->GetCellMargin();
  int cell_element_spacing = helper_->GetCellElementSpacing();
  gfx::ImageSkia image =
      model_->GetIcon(0).Rasterize(table_->GetColorProvider());
  EXPECT_FALSE(image.isNull());
  int first_column_min_width =
      cell_margin + cell_element_spacing + kGroupingIndicatorSize;
  gfx::Rect cell_bounds = helper_->GetCellBounds(0, 0);
  // icon can be paint within the cell bounds.
  EXPECT_GT(cell_bounds.width(),
            first_column_min_width + ui::TableModel::kIconSize);
  gfx::Rect text_bounds = cell_bounds;
  text_bounds.Inset(gfx::Insets::VH(0, cell_margin));
  text_bounds.Inset(
      gfx::Insets().set_left(kGroupingIndicatorSize + cell_element_spacing));
  EXPECT_EQ(text_bounds.x(), first_column_min_width);

  // If the cell size is sufficient to draw the icon, whether it is LTR or RTL,
  // the `src_image_bounds` will be the size of the original image,
  // which is (0, 0, image.width(), image.height()).
  {
    EXPECT_FALSE(base::i18n::IsRTL());
    gfx::Rect dest_image_bounds =
        helper_->GetPaintIconDestBounds(cell_bounds, text_bounds.x());
    // If it is an LTR layout, the `dest_image_bounds.x()` will be the minimum
    // width of the first column, and the size of `dest_image_bounds` will be
    // ui::TableModel::kIconSize.
    EXPECT_EQ(
        dest_image_bounds,
        gfx::Rect(first_column_min_width,
                  cell_bounds.y() +
                      (cell_bounds.height() - ui::TableModel::kIconSize) / 2,
                  ui::TableModel::kIconSize, ui::TableModel::kIconSize));
    gfx::Rect src_image_bounds =
        helper_->GetPaintIconSrcBounds(image.size(), dest_image_bounds.width());

    EXPECT_EQ(src_image_bounds, gfx::Rect(image.size()));
  }
  {
    base::i18n::SetRTLForTesting(true);
    EXPECT_TRUE(base::i18n::IsRTL());
    gfx::Rect dest_image_bounds =
        helper_->GetPaintIconDestBounds(cell_bounds, text_bounds.x());
    // If it is an RTL layout, the `dest_image_bounds.x()` will be the
    // table_view's width minus the minimum width of the first column and
    // ui::TableModel::kIconSize, and the size of `dest_image_bounds` will be
    // ui::TableModel::kIconSize.
    EXPECT_EQ(
        dest_image_bounds,
        gfx::Rect(table_->width() - first_column_min_width -
                      ui::TableModel::kIconSize,
                  cell_bounds.y() +
                      (cell_bounds.height() - ui::TableModel::kIconSize) / 2,
                  ui::TableModel::kIconSize, ui::TableModel::kIconSize));
    gfx::Rect src_image_bounds =
        helper_->GetPaintIconSrcBounds(image.size(), dest_image_bounds.width());
    EXPECT_EQ(src_image_bounds, gfx::Rect(image.size()));
    base::i18n::SetRTLForTesting(false);
  }
}

TEST_F(TableViewPaintIconBoundsTest, TestPaintIconBoundsForClipped) {
  EXPECT_EQ(2u, helper_->visible_col_count());
  int cell_margin = helper_->GetCellMargin();
  int cell_element_spacing = helper_->GetCellElementSpacing();
  gfx::ImageSkia image =
      model_->GetIcon(0).Rasterize(table_->GetColorProvider());
  EXPECT_FALSE(image.isNull());
  int first_column_min_width =
      cell_margin + cell_element_spacing + kGroupingIndicatorSize;
  // Adjust the width of the first column so that only half of the icon can be
  // displayed
  int dest_image_width = ui::TableModel::kIconSize / 2;
  int x = table_->GetVisibleColumn(0).width;
  int resize_pixels = x - (first_column_min_width + dest_image_width);
  PressLeftMouseAt(helper_->header(), gfx::Point(x, 0));
  DragLeftMouseTo(helper_->header(), gfx::Point(x - resize_pixels, 0));

  gfx::Rect cell_bounds = helper_->GetCellBounds(0, 0);
  // If the cell size is only sufficient to draw half of the icon
  EXPECT_EQ(cell_bounds.width(), first_column_min_width + dest_image_width);
  gfx::Rect text_bounds = cell_bounds;
  text_bounds.Inset(gfx::Insets::VH(0, cell_margin));
  text_bounds.Inset(
      gfx::Insets().set_left(kGroupingIndicatorSize + cell_element_spacing));
  {
    // When the layout is LTR.
    EXPECT_FALSE(base::i18n::IsRTL());
    gfx::Rect dest_image_bounds =
        helper_->GetPaintIconDestBounds(cell_bounds, text_bounds.x());
    // The `dest_image_bounds.x()` should be equal to the minimum width of the
    // first column. The `dest_image_bounds.width()` should be equal to half of
    // ui::TableModel::kIconSize.
    EXPECT_EQ(
        dest_image_bounds,
        gfx::Rect(first_column_min_width,
                  cell_bounds.y() +
                      (cell_bounds.height() - ui::TableModel::kIconSize) / 2,
                  dest_image_width, ui::TableModel::kIconSize));
    // The right boundary of `dest_image_bounds` should be equal to the right
    // boundary of the cell when  clipped icon.
    EXPECT_EQ(dest_image_bounds.right(), cell_bounds.right());
    gfx::Rect src_image_bounds =
        helper_->GetPaintIconSrcBounds(image.size(), dest_image_bounds.width());

    // `src_image_bounds.x()` should be 0 (clipping starts from the left side of
    // the original image). The `src_image_bounds.width()` should be equal
    // to half of `image.width()`.
    EXPECT_EQ(src_image_bounds,
              gfx::Rect(0, 0, image.width() / 2, image.height()));
  }
  {
    // When the layout is RTL.
    base::i18n::SetRTLForTesting(true);
    EXPECT_TRUE(base::i18n::IsRTL());
    gfx::Rect dest_image_bounds =
        helper_->GetPaintIconDestBounds(cell_bounds, text_bounds.x());
    // The `dest_image_bounds.x()` should be equal to the table_view's width
    // minus the minimum width of the first column and half of
    // ui::TableModel::kIconSize.The `dest_image_bounds.width()` should be equal
    // to half of ui::TableModel::kIconSize.
    EXPECT_EQ(
        dest_image_bounds,
        gfx::Rect(table_->width() - (first_column_min_width + dest_image_width),
                  cell_bounds.y() +
                      (cell_bounds.height() - ui::TableModel::kIconSize) / 2,
                  dest_image_width, ui::TableModel::kIconSize));
    // The left boundary of `dest_image_bounds` should be equal to the left
    // boundary of the cell when  clipped icon.
    EXPECT_EQ(dest_image_bounds.x(), table_->GetMirroredRect(cell_bounds).x());
    gfx::Rect src_image_bounds =
        helper_->GetPaintIconSrcBounds(image.size(), dest_image_bounds.width());
    // `src_image_bounds.x()` should be equal to the width of the original image
    // minus the width of the clipped icon. The `src_image_bounds.width()`
    // should be equal to half of `image.width()`
    EXPECT_EQ(src_image_bounds,
              gfx::Rect(image.width() - src_image_bounds.width(), 0,
                        image.width() / 2, image.height()));
    base::i18n::SetRTLForTesting(false);
  }
}

TEST_F(TableViewPaintIconBoundsTest, TestPaintIconBoundsNotNeedDisplay) {
  EXPECT_EQ(2u, helper_->visible_col_count());
  int cell_margin = helper_->GetCellMargin();
  int cell_element_spacing = helper_->GetCellElementSpacing();
  gfx::ImageSkia image =
      model_->GetIcon(0).Rasterize(table_->GetColorProvider());
  EXPECT_FALSE(image.isNull());
  int first_column_min_width =
      cell_margin + cell_element_spacing + kGroupingIndicatorSize;
  // Adjust the width of the first column. icon not need display
  int x = table_->GetVisibleColumn(0).width;
  int resize_pixels = x - first_column_min_width;
  PressLeftMouseAt(helper_->header(), gfx::Point(x, 0));
  DragLeftMouseTo(helper_->header(), gfx::Point(x - resize_pixels, 0));

  gfx::Rect cell_bounds = helper_->GetCellBounds(0, 0);
  EXPECT_EQ(cell_bounds.width(), first_column_min_width);
  gfx::Rect text_bounds = cell_bounds;
  text_bounds.Inset(gfx::Insets::VH(0, cell_margin));
  text_bounds.Inset(
      gfx::Insets().set_left(kGroupingIndicatorSize + cell_element_spacing));
  // If the bounds of the cell is not sufficient to draw the icon, whether it is
  // LTR or RTL, the `dest_image_bounds` will be empty.
  {
    EXPECT_FALSE(base::i18n::IsRTL());
    gfx::Rect dest_image_bounds =
        helper_->GetPaintIconDestBounds(cell_bounds, text_bounds.x());
    EXPECT_TRUE(dest_image_bounds.IsEmpty());
  }
  {
    base::i18n::SetRTLForTesting(true);
    EXPECT_TRUE(base::i18n::IsRTL());
    gfx::Rect dest_image_bounds =
        helper_->GetPaintIconDestBounds(cell_bounds, text_bounds.x());
    EXPECT_TRUE(dest_image_bounds.IsEmpty());
    base::i18n::SetRTLForTesting(false);
  }
}
}  // namespace views
