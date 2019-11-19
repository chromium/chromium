// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_view.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
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
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"

// Put the tests in the views namespace to make it easier to declare them as
// friend classes.
namespace views {

class TableViewTestHelper {
 public:
  explicit TableViewTestHelper(TableView* table) : table_(table) {}

  std::string GetPaintRegion(const gfx::Rect& bounds) {
    TableView::PaintRegion region(table_->GetPaintRegion(bounds));
    return "rows=" + base::NumberToString(region.min_row) + " " +
           base::NumberToString(region.max_row) +
           " cols=" + base::NumberToString(region.min_column) + " " +
           base::NumberToString(region.max_column);
  }

  size_t visible_col_count() {
    return table_->visible_columns().size();
  }

  int GetActiveVisibleColumnIndex() {
    return table_->GetActiveVisibleColumnIndex();
  }

  TableHeader* header() { return table_->header_; }

  void SetSelectionModel(const ui::ListSelectionModel& new_selection) {
    table_->SetSelectionModel(new_selection);
  }

  const gfx::FontList& font_list() { return table_->font_list_; }

  AXVirtualView* GetVirtualAccessibilityRow(int row) {
    return table_->GetVirtualAccessibilityRow(row);
  }

  AXVirtualView* GetVirtualAccessibilityCell(int row,
                                             int visible_column_index) {
    return table_->GetVirtualAccessibilityCell(row, visible_column_index);
  }

 private:
  TableView* table_;

  DISALLOW_COPY_AND_ASSIGN(TableViewTestHelper);
};

namespace {

#if defined(OS_MACOSX)
constexpr int kCtrlOrCmdMask = ui::EF_COMMAND_DOWN;
#else
constexpr int kCtrlOrCmdMask = ui::EF_CONTROL_DOWN;
#endif

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

  // Adds a new row at index |row| with values |c1_value| and |c2_value|.
  void AddRow(int row, int c1_value, int c2_value);

  // Removes the row at index |row|.
  void RemoveRow(int row);

  // Changes the values of the row at |row|.
  void ChangeRow(int row, int c1_value, int c2_value);

  // Reorders rows in the model.
  void MoveRows(int row_from, int length, int row_to);

  // ui::TableModel:
  int RowCount() override;
  base::string16 GetText(int row, int column_id) override;
  base::string16 GetTooltip(int row) override;
  void SetObserver(ui::TableModelObserver* observer) override;
  int CompareValues(int row1, int row2, int column_id) override;

 private:
  ui::TableModelObserver* observer_ = nullptr;

  // The data.
  std::vector<std::vector<int> > rows_;

  DISALLOW_COPY_AND_ASSIGN(TestTableModel2);
};

TestTableModel2::TestTableModel2() {
  AddRow(0, 0, 1);
  AddRow(1, 1, 1);
  AddRow(2, 2, 2);
  AddRow(3, 3, 0);
}

void TestTableModel2::AddRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  std::vector<int> new_row;
  new_row.push_back(c1_value);
  new_row.push_back(c2_value);
  rows_.insert(rows_.begin() + row, new_row);
  if (observer_)
    observer_->OnItemsAdded(row, 1);
}
void TestTableModel2::RemoveRow(int row) {
  DCHECK(row >= 0 && row <= static_cast<int>(rows_.size()));
  rows_.erase(rows_.begin() + row);
  if (observer_)
    observer_->OnItemsRemoved(row, 1);
}

void TestTableModel2::ChangeRow(int row, int c1_value, int c2_value) {
  DCHECK(row >= 0 && row < static_cast<int>(rows_.size()));
  rows_[row][0] = c1_value;
  rows_[row][1] = c2_value;
  if (observer_)
    observer_->OnItemsChanged(row, 1);
}

void TestTableModel2::MoveRows(int row_from, int length, int row_to) {
  DCHECK_GT(length, 0);
  DCHECK_GE(row_from, 0);
  DCHECK_LE(row_from + length, static_cast<int>(rows_.size()));
  DCHECK_GE(row_to, 0);
  DCHECK_LE(row_to + length, static_cast<int>(rows_.size()));

  auto old_start = rows_.begin() + row_from;
  std::vector<std::vector<int>> temp(old_start, old_start + length);
  rows_.erase(old_start, old_start + length);
  rows_.insert(rows_.begin() + row_to, temp.begin(), temp.end());
  if (observer_)
    observer_->OnItemsMoved(row_from, length, row_to);
}

int TestTableModel2::RowCount() {
  return static_cast<int>(rows_.size());
}

base::string16 TestTableModel2::GetText(int row, int column_id) {
  return base::NumberToString16(rows_[row][column_id]);
}

base::string16 TestTableModel2::GetTooltip(int row) {
  return base::ASCIIToUTF16("Tooltip") + base::NumberToString16(row);
}

void TestTableModel2::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

int TestTableModel2::CompareValues(int row1, int row2, int column_id) {
  return rows_[row1][column_id] - rows_[row2][column_id];
}

// Returns the view to model mapping as a string.
std::string GetViewToModelAsString(TableView* table) {
  std::string result;
  for (int i = 0; i < table->GetRowCount(); ++i) {
    if (i != 0)
      result += " ";
    result += base::NumberToString(table->ViewToModel(i));
  }
  return result;
}

// Returns the model to view mapping as a string.
std::string GetModelToViewAsString(TableView* table) {
  std::string result;
  for (int i = 0; i < table->GetRowCount(); ++i) {
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
  for (int i = 0; i < table->GetRowCount(); ++i) {
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

bool PressLeftMouseAt(views::View* target, const gfx::Point& point) {
  const ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  return target->OnMousePressed(pressed);
}

void ReleaseLeftMouseAt(views::View* target, const gfx::Point& point) {
  const ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  target->OnMouseReleased(release);
}

bool DragLeftMouseTo(views::View* target, const gfx::Point& point) {
  const ui::MouseEvent dragged(ui::ET_MOUSE_DRAGGED, point, point,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               0);
  return target->OnMouseDragged(dragged);
}

}  // namespace

class TableViewTest : public ViewsTestBase {
 public:
  TableViewTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    model_ = std::make_unique<TestTableModel2>();
    std::vector<ui::TableColumn> columns(2);
    columns[0].title = base::ASCIIToUTF16("Title Column 0");
    columns[0].sortable = true;
    columns[1].title = base::ASCIIToUTF16("Title Column 1");
    columns[1].id = 1;
    columns[1].sortable = true;
    auto table =
        std::make_unique<TableView>(model_.get(), columns, TEXT_ONLY, false);
    table_ = table.get();
    auto scroll_view = TableView::CreateScrollViewWithTable(std::move(table));
    scroll_view->SetBounds(0, 0, 10000, 10000);
    scroll_view->Layout();
    helper_ = std::make_unique<TableViewTestHelper>(table_);

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    params.delegate = GetWidgetDelegate(widget_.get());
    widget_->Init(std::move(params));
    widget_->GetContentsView()->AddChildView(std::move(scroll_view));
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void ClickOnRow(int row, int flags) {
    ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
    generator.set_assume_window_at_origin(false);
    generator.set_flags(flags);
    generator.set_current_screen_location(GetPointForRow(row));
    generator.PressLeftButton();
  }

  void TapOnRow(int row) {
    ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
    generator.GestureTapAt(GetPointForRow(row));
  }

  // Returns the state of the selection model as a string. The format is:
  // 'active=X anchor=X selection=X X X...'.
  std::string SelectionStateAsString() const {
    const ui::ListSelectionModel& model(table_->selection_model());
    std::string result = "active=" + base::NumberToString(model.active()) +
                         " anchor=" + base::NumberToString(model.anchor()) +
                         " selection=";
    const ui::ListSelectionModel::SelectedIndices& selection(
        model.selected_indices());
    for (size_t i = 0; i < selection.size(); ++i) {
      if (i != 0)
        result += " ";
      result += base::NumberToString(selection[i]);
    }
    return result;
  }

  void PressKey(ui::KeyboardCode code) { PressKey(code, ui::EF_NONE); }

  void PressKey(ui::KeyboardCode code, int flags) {
    ui::test::EventGenerator generator(GetRootWindow(widget_.get()));
    generator.PressKey(code, flags);
  }

 protected:
  virtual WidgetDelegate* GetWidgetDelegate(Widget* widget) { return nullptr; }

  std::unique_ptr<TestTableModel2> model_;

  // Owned by |parent_|.
  TableView* table_ = nullptr;

  std::unique_ptr<TableViewTestHelper> helper_;

  std::unique_ptr<Widget> widget_;

 private:
  gfx::Point GetPointForRow(int row) {
    const int y = (row + 0.5) * table_->GetRowHeight();
    return table_->GetBoundsInScreen().origin() + gfx::Vector2d(5, y);
  }

  DISALLOW_COPY_AND_ASSIGN(TableViewTest);
};

// Verifies GetPaintRegion.
TEST_F(TableViewTest, GetPaintRegion) {
  // Two columns should be visible.
  EXPECT_EQ(2u, helper_->visible_col_count());

  EXPECT_EQ("rows=0 4 cols=0 2", helper_->GetPaintRegion(table_->bounds()));
  EXPECT_EQ("rows=0 4 cols=0 1",
            helper_->GetPaintRegion(gfx::Rect(0, 0, 1, table_->height())));
}

TEST_F(TableViewTest, UpdateVirtualAccessibilityChildren) {
  const ViewAccessibility& view_accessibility = table_->GetViewAccessibility();
  ui::AXNodeData data;
  view_accessibility.GetAccessibleNodeData(&data);
  EXPECT_EQ(ax::mojom::Role::kListGrid, data.role);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));
  EXPECT_EQ(ax::mojom::Restriction::kReadOnly, data.GetRestriction());
  EXPECT_EQ(table_->GetRowCount(),
            static_cast<int>(
                data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount)));
  EXPECT_EQ(helper_->visible_col_count(),
            static_cast<size_t>(data.GetIntAttribute(
                ax::mojom::IntAttribute::kTableColumnCount)));

  // The header takes up another row.
  ASSERT_EQ(size_t{table_->GetRowCount() + 1},
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

  int i = 0;
  for (auto child_iter = view_accessibility.virtual_children().begin() + 1;
       i < table_->GetRowCount(); ++child_iter, ++i) {
    const auto& row = *child_iter;
    ASSERT_TRUE(row);
    const ui::AXNodeData& row_data = row->GetData();
    EXPECT_EQ(ax::mojom::Role::kRow, row_data.role);
    EXPECT_EQ(
        i, row_data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex));

    ASSERT_EQ(helper_->visible_col_count(), row->children().size());
    j = 0;
    for (const auto& cell : row->children()) {
      ASSERT_TRUE(cell);
      const ui::AXNodeData& cell_data = cell->GetData();
      EXPECT_EQ(ax::mojom::Role::kCell, cell_data.role);
      EXPECT_EQ(i, cell_data.GetIntAttribute(
                       ax::mojom::IntAttribute::kTableCellRowIndex));
      EXPECT_EQ(j++, cell_data.GetIntAttribute(
                         ax::mojom::IntAttribute::kTableCellColumnIndex));
    }
  }
}

TEST_F(TableViewTest, GetVirtualAccessibilityRow) {
  for (int i = 0; i < table_->GetRowCount(); ++i) {
    const AXVirtualView* row = helper_->GetVirtualAccessibilityRow(i);
    ASSERT_TRUE(row);
    const ui::AXNodeData& row_data = row->GetData();
    EXPECT_EQ(ax::mojom::Role::kRow, row_data.role);
    EXPECT_EQ(i, static_cast<int>(row_data.GetIntAttribute(
                     ax::mojom::IntAttribute::kTableRowIndex)));
  }
}

TEST_F(TableViewTest, GetVirtualAccessibilityCell) {
  for (int i = 0; i < table_->GetRowCount(); ++i) {
    for (int j = 0; j < static_cast<int>(helper_->visible_col_count()); ++j) {
      const AXVirtualView* cell = helper_->GetVirtualAccessibilityCell(i, j);
      ASSERT_TRUE(cell);
      const ui::AXNodeData& cell_data = cell->GetData();
      EXPECT_EQ(ax::mojom::Role::kCell, cell_data.role);
      EXPECT_EQ(i, static_cast<int>(cell_data.GetIntAttribute(
                       ax::mojom::IntAttribute::kTableCellRowIndex)));
      EXPECT_EQ(j, static_cast<int>(cell_data.GetIntAttribute(
                       ax::mojom::IntAttribute::kTableCellColumnIndex)));
    }
  }
}

// Verifies SetColumnVisibility().
TEST_F(TableViewTest, ColumnVisibility) {
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

// Verifies resizing a column using the mouse works.
TEST_F(TableViewTest, Resize) {
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
TEST_F(TableViewTest, ResizeViaGesture) {
  const int x = table_->GetVisibleColumn(0).width;
  EXPECT_NE(0, x);
  // Drag the mouse 1 pixel to the left.
  ui::GestureEvent scroll_begin(
      x, 0, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  helper_->header()->OnGestureEvent(&scroll_begin);
  ui::GestureEvent scroll_update(
      x - 1, 0, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE));
  helper_->header()->OnGestureEvent(&scroll_update);

  // This should shrink the first column and pull the second column in.
  EXPECT_EQ(x - 1, table_->GetVisibleColumn(0).width);
  EXPECT_EQ(x - 1, table_->GetVisibleColumn(1).x);
}

// Verifies resizing a column works with the keyboard.
// The resize keyboard amount is 5 pixels.
TEST_F(TableViewTest, ResizeViaKeyboard) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell)
    return;

  table_->RequestFocus();
  const int x = table_->GetVisibleColumn(0).width;
  EXPECT_NE(0, x);

  // Table starts off with no visible column being active.
  ASSERT_EQ(-1, helper_->GetActiveVisibleColumnIndex());

  ui::ListSelectionModel new_selection;
  new_selection.SetSelectedIndex(1);
  helper_->SetSelectionModel(new_selection);
  ASSERT_EQ(0, helper_->GetActiveVisibleColumnIndex());

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
TEST_F(TableViewTest, ResizeHonorsMinimum) {
  TableViewTestHelper helper(table_);
  const int x = table_->GetVisibleColumn(0).width;
  EXPECT_NE(0, x);

  PressLeftMouseAt(helper_->header(), gfx::Point(x, 0));
  DragLeftMouseTo(helper_->header(), gfx::Point(20, 0));

  int title_width = gfx::GetStringWidth(
      table_->GetVisibleColumn(0).column.title, helper.font_list());
  EXPECT_LT(title_width, table_->GetVisibleColumn(0).width);

  int old_width = table_->GetVisibleColumn(0).width;
  DragLeftMouseTo(helper_->header(), gfx::Point(old_width + 10, 0));
  EXPECT_EQ(old_width + 10, table_->GetVisibleColumn(0).width);
}

// Assertions for table sorting.
TEST_F(TableViewTest, Sort) {
  // Initial ordering.
  EXPECT_TRUE(table_->sort_descriptors().empty());
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));
  EXPECT_EQ("[0, 1], [1, 1], [2, 2], [3, 0]",
            GetRowsInViewOrderAsString(table_));

  // Toggle the sort order of the first column, shouldn't change anything.
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));
  EXPECT_EQ("[0, 1], [1, 1], [2, 2], [3, 0]",
            GetRowsInViewOrderAsString(table_));

  // Toggle the sort (first column descending).
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 2 1 0", GetViewToModelAsString(table_));
  EXPECT_EQ("3 2 1 0", GetModelToViewAsString(table_));
  EXPECT_EQ("[3, 0], [2, 2], [1, 1], [0, 1]",
            GetRowsInViewOrderAsString(table_));

  // Change the [3, 0] cell to [-1, 0]. This should move it to the back of
  // the current sort order.
  model_->ChangeRow(3, -1, 0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("2 1 0 3", GetViewToModelAsString(table_));
  EXPECT_EQ("2 1 0 3", GetModelToViewAsString(table_));
  EXPECT_EQ("[2, 2], [1, 1], [0, 1], [-1, 0]",
            GetRowsInViewOrderAsString(table_));

  // Toggle the sort again, to clear the sort and restore the model ordering.
  table_->ToggleSortOrder(0);
  EXPECT_TRUE(table_->sort_descriptors().empty());
  EXPECT_EQ("0 1 2 3", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3", GetModelToViewAsString(table_));
  EXPECT_EQ("[0, 1], [1, 1], [2, 2], [-1, 0]",
            GetRowsInViewOrderAsString(table_));

  // Toggle the sort again (first column ascending).
  table_->ToggleSortOrder(0);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("3 0 1 2", GetViewToModelAsString(table_));
  EXPECT_EQ("1 2 3 0", GetModelToViewAsString(table_));
  EXPECT_EQ("[-1, 0], [0, 1], [1, 1], [2, 2]",
            GetRowsInViewOrderAsString(table_));

  // Add a row that's second in the model order, but last in the active sort
  // order.
  model_->AddRow(1, 3, 4);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("4 0 2 3 1", GetViewToModelAsString(table_));
  EXPECT_EQ("1 4 2 3 0", GetModelToViewAsString(table_));
  EXPECT_EQ("[-1, 0], [0, 1], [1, 1], [2, 2], [3, 4]",
            GetRowsInViewOrderAsString(table_));

  // Add a row that's last in the model order but second in the the active sort
  // order.
  model_->AddRow(5, -1, 20);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ("4 5 0 2 3 1", GetViewToModelAsString(table_));
  EXPECT_EQ("2 5 3 4 0 1", GetModelToViewAsString(table_));
  EXPECT_EQ("[-1, 0], [-1, 20], [0, 1], [1, 1], [2, 2], [3, 4]",
            GetRowsInViewOrderAsString(table_));

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
  EXPECT_EQ("4 2 0 3 1 5", GetViewToModelAsString(table_));
  EXPECT_EQ("2 4 1 3 0 5", GetModelToViewAsString(table_));
  EXPECT_EQ("[-1, 0], [1, 1], [0, 1], [2, 2], [3, 4], [-1, 20]",
            GetRowsInViewOrderAsString(table_));

  // Toggle the current column to change from ascending to descending. This
  // should result in an almost-reversal of the previous order, except for the
  // two rows with the same value for the second column.
  table_->ToggleSortOrder(1);
  ASSERT_EQ(2u, table_->sort_descriptors().size());
  EXPECT_EQ(1, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ(0, table_->sort_descriptors()[1].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[1].ascending);
  EXPECT_EQ("5 1 3 2 0 4", GetViewToModelAsString(table_));
  EXPECT_EQ("4 1 3 2 5 0", GetModelToViewAsString(table_));
  EXPECT_EQ("[-1, 20], [3, 4], [2, 2], [1, 1], [0, 1], [-1, 0]",
            GetRowsInViewOrderAsString(table_));

  // Delete the [0, 1] row from the model. It's at model index zero.
  model_->RemoveRow(0);
  ASSERT_EQ(2u, table_->sort_descriptors().size());
  EXPECT_EQ(1, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);
  EXPECT_EQ(0, table_->sort_descriptors()[1].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[1].ascending);
  EXPECT_EQ("4 0 2 1 3", GetViewToModelAsString(table_));
  EXPECT_EQ("1 3 2 4 0", GetModelToViewAsString(table_));
  EXPECT_EQ("[-1, 20], [3, 4], [2, 2], [1, 1], [-1, 0]",
            GetRowsInViewOrderAsString(table_));

  // Toggle the current sort column again. This should clear both the primary
  // and secondary sort descriptor.
  table_->ToggleSortOrder(1);
  EXPECT_TRUE(table_->sort_descriptors().empty());
  EXPECT_EQ("0 1 2 3 4", GetViewToModelAsString(table_));
  EXPECT_EQ("0 1 2 3 4", GetModelToViewAsString(table_));
  EXPECT_EQ("[3, 4], [1, 1], [2, 2], [-1, 0], [-1, 20]",
            GetRowsInViewOrderAsString(table_));
}

// Verifies clicking on the header sorts.
TEST_F(TableViewTest, SortOnMouse) {
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
TEST_F(TableViewTest, SortOnSpaceBar) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell)
    return;

  table_->RequestFocus();
  ASSERT_TRUE(table_->sort_descriptors().empty());
  // Table starts off with no visible column being active.
  ASSERT_EQ(-1, helper_->GetActiveVisibleColumnIndex());

  ui::ListSelectionModel new_selection;
  new_selection.SetSelectedIndex(1);
  helper_->SetSelectionModel(new_selection);
  ASSERT_EQ(0, helper_->GetActiveVisibleColumnIndex());

  PressKey(ui::VKEY_SPACE);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);

  PressKey(ui::VKEY_SPACE);
  ASSERT_EQ(1u, table_->sort_descriptors().size());
  EXPECT_EQ(0, table_->sort_descriptors()[0].column_id);
  EXPECT_FALSE(table_->sort_descriptors()[0].ascending);

  PressKey(ui::VKEY_RIGHT);
  ASSERT_EQ(1, helper_->GetActiveVisibleColumnIndex());

  PressKey(ui::VKEY_SPACE);
  ASSERT_EQ(2u, table_->sort_descriptors().size());
  EXPECT_EQ(1, table_->sort_descriptors()[0].column_id);
  EXPECT_EQ(0, table_->sort_descriptors()[1].column_id);
  EXPECT_TRUE(table_->sort_descriptors()[0].ascending);
  EXPECT_FALSE(table_->sort_descriptors()[1].ascending);
}

TEST_F(TableViewTest, Tooltip) {
  // Column 0 uses the TableModel's GetTooltipText override for tooltips.
  table_->SetVisibleColumnWidth(0, 10);
  auto local_point_for_row = [&](int row) {
    return gfx::Point(5, (row + 0.5) * table_->GetRowHeight());
  };
  auto expected = [](int row) {
    return base::ASCIIToUTF16("Tooltip") + base::NumberToString16(row);
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

  void SetRanges(const std::vector<int>& ranges) {
    ranges_ = ranges;
  }

  // TableGrouper overrides:
  void GetGroupRange(int model_index, GroupRange* range) override {
    int offset = 0;
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
  std::vector<int> ranges_;

  DISALLOW_COPY_AND_ASSIGN(TableGrouperImpl);
};

}  // namespace

// Assertions around grouping.
TEST_F(TableViewTest, Grouping) {
  // Configure the grouper so that there are two groups:
  // A 0
  //   1
  // B 2
  //   3
  TableGrouperImpl grouper;
  std::vector<int> ranges;
  ranges.push_back(2);
  ranges.push_back(2);
  grouper.SetRanges(ranges);
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

namespace {

class TableViewObserverImpl : public TableViewObserver {
 public:
  TableViewObserverImpl() = default;

  int GetChangedCountAndClear() {
    const int count = selection_changed_count_;
    selection_changed_count_ = 0;
    return count;
  }

  // TableViewObserver overrides:
  void OnSelectionChanged() override { selection_changed_count_++; }

 private:
  int selection_changed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TableViewObserverImpl);
};

}  // namespace

// Assertions around changing the selection.
TEST_F(TableViewTest, Selection) {
  TableViewObserverImpl observer;
  table_->set_observer(&observer);

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

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

  table_->set_observer(nullptr);
}

TEST_F(TableViewTest, RemoveUnselectedRows) {
  TableViewObserverImpl observer;
  table_->set_observer(&observer);

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

// 0 1 2 3:
// select 3 -> 0 1 2 [3]
// remove 3 -> 0 1 2 (none selected)
// select 1 -> 0 [1] 2
// remove 1 -> 0 1 (none selected)
// select 0 -> [0] 1
// remove 0 -> 0 (none selected)
TEST_F(TableViewTest, SelectionNoSelectOnRemove) {
  TableViewObserverImpl observer;
  table_->set_observer(&observer);
  table_->SetSelectOnRemove(false);

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  // Select row 3.
  table_->Select(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());

  // Remove the selected row, this should notify of a change and since the
  // select_on_remove_ is set false, and the removed item is the previously
  // selected item, so no item is selected.
  model_->RemoveRow(3);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  // Select row 1.
  table_->Select(1);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  // Remove the selected row.
  model_->RemoveRow(1);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  // Select row 0.
  table_->Select(0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  // Remove the selected row.
  model_->RemoveRow(0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  table_->set_observer(nullptr);
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !defined(OS_MACOSX)
// Verifies selection works by way of a gesture.
TEST_F(TableViewTest, SelectOnTap) {
  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  TableViewObserverImpl observer;
  table_->set_observer(&observer);

  // Tap on the first row, should select it and focus the table.
  EXPECT_FALSE(table_->HasFocus());
  TapOnRow(0);
  EXPECT_TRUE(table_->HasFocus());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  table_->set_observer(nullptr);
}
#endif

// Verifies up/down correctly navigate through groups.
TEST_F(TableViewTest, KeyUpDown) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  std::vector<int> ranges;
  ranges.push_back(2);
  ranges.push_back(1);
  ranges.push_back(2);
  grouper.SetRanges(ranges);
  table_->SetGrouper(&grouper);

  TableViewObserverImpl observer;
  table_->set_observer(&observer);
  table_->RequestFocus();

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=0 1", SelectionStateAsString());

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
  EXPECT_EQ("active=3 anchor=3 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
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

  table_->Select(-1);
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  observer.GetChangedCountAndClear();
  // Up with nothing selected selects the first row.
  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

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
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=3 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  PressKey(ui::VKEY_UP);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=2", SelectionStateAsString());

  table_->set_observer(nullptr);
}

// Verifies left/right correctly navigate through visible columns.
TEST_F(TableViewTest, KeyLeftRight) {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell)
    return;

  TableViewObserverImpl observer;
  table_->set_observer(&observer);
  table_->RequestFocus();

  // Initially no active visible column.
  EXPECT_EQ(-1, helper_->GetActiveVisibleColumnIndex());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  helper_->SetSelectionModel(ui::ListSelectionModel());
  EXPECT_EQ(-1, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(1, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(1, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0", SelectionStateAsString());

  ui::ListSelectionModel new_selection;
  new_selection.SetSelectedIndex(1);
  helper_->SetSelectionModel(new_selection);
  EXPECT_EQ(1, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  table_->SetColumnVisibility(0, false);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  // Since the first column was hidden, the active visible column should not
  // advance.
  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  // If visibility to the first column is restored, the active visible column
  // should be unchanged because columns are always added to the end.
  table_->SetColumnVisibility(0, true);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());
  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(1, helper_->GetActiveVisibleColumnIndex());

  // If visibility to the first column is removed, the active visible column
  // should be decreased by one.
  table_->SetColumnVisibility(0, false);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  PressKey(ui::VKEY_LEFT);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  table_->SetColumnVisibility(0, true);
  EXPECT_EQ(0, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  PressKey(ui::VKEY_RIGHT);
  EXPECT_EQ(1, helper_->GetActiveVisibleColumnIndex());
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=1 selection=1", SelectionStateAsString());

  table_->set_observer(nullptr);
}

// Verifies home/end do the right thing.
TEST_F(TableViewTest, HomeEnd) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  std::vector<int> ranges{2, 1, 2};
  grouper.SetRanges(ranges);
  table_->SetGrouper(&grouper);

  TableViewObserverImpl observer;
  table_->set_observer(&observer);
  table_->RequestFocus();

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  PressKey(ui::VKEY_END);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  PressKey(ui::VKEY_HOME);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=0 anchor=0 selection=0 1", SelectionStateAsString());

  table_->set_observer(nullptr);
}

// Verifies multiple selection gestures work (control-click, shift-click ...).
TEST_F(TableViewTest, Multiselection) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  std::vector<int> ranges;
  ranges.push_back(2);
  ranges.push_back(1);
  ranges.push_back(2);
  grouper.SetRanges(ranges);
  table_->SetGrouper(&grouper);

  // Initially no selection.
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  TableViewObserverImpl observer;
  table_->set_observer(&observer);

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
  ClickOnRow(2, kCtrlOrCmdMask);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=2 selection=3 4", SelectionStateAsString());

  // Control-shift click on second row, should extend selection to it.
  ClickOnRow(1, kCtrlOrCmdMask | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=2 selection=0 1 2 3 4", SelectionStateAsString());

  // Click on last row again.
  ClickOnRow(4, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  table_->set_observer(nullptr);
}

// Verifies multiple selection gestures work when sorted.
TEST_F(TableViewTest, MultiselectionWithSort) {
  // Configure the grouper so that there are three groups:
  // A 0
  //   1
  // B 5
  // C 2
  //   3
  model_->AddRow(2, 5, 0);
  TableGrouperImpl grouper;
  std::vector<int> ranges;
  ranges.push_back(2);
  ranges.push_back(1);
  ranges.push_back(2);
  grouper.SetRanges(ranges);
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
  EXPECT_EQ("active=-1 anchor=-1 selection=", SelectionStateAsString());

  TableViewObserverImpl observer;
  table_->set_observer(&observer);

  // Click on the third row, should select it and the second row.
  ClickOnRow(2, 0);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=4 selection=3 4", SelectionStateAsString());

  // Extend selection to first row.
  ClickOnRow(0, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=4 selection=2 3 4", SelectionStateAsString());
}

TEST_F(TableViewTest, MoveRowsWithMultipleSelection) {
  model_->AddRow(3, 77, 0);

  // Hide column 1.
  table_->SetColumnVisibility(1, false);

  TableViewObserverImpl observer;
  table_->set_observer(&observer);

  // Select three rows.
  ClickOnRow(2, 0);
  ClickOnRow(4, ui::EF_SHIFT_DOWN);
  EXPECT_EQ(2, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=2 selection=2 3 4", SelectionStateAsString());
  EXPECT_EQ("[0], [1], [2], [77], [3]", GetRowsInViewOrderAsString(table_));

  // Move the unselected rows to the middle of the current selection. None of
  // the move operations should affect the view order.
  model_->MoveRows(0, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=0 selection=0 3 4", SelectionStateAsString());
  EXPECT_EQ("[2], [0], [1], [77], [3]", GetRowsInViewOrderAsString(table_));

  // Move the unselected rows to the end of the current selection.
  model_->MoveRows(1, 2, 3);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=2 anchor=0 selection=0 1 2", SelectionStateAsString());
  EXPECT_EQ("[2], [77], [3], [0], [1]", GetRowsInViewOrderAsString(table_));

  // Move the unselected rows back to the middle of the selection.
  model_->MoveRows(3, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=0 selection=0 3 4", SelectionStateAsString());
  EXPECT_EQ("[2], [0], [1], [77], [3]", GetRowsInViewOrderAsString(table_));

  // Swap the unselected rows.
  model_->MoveRows(1, 1, 2);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=0 selection=0 3 4", SelectionStateAsString());
  EXPECT_EQ("[2], [1], [0], [77], [3]", GetRowsInViewOrderAsString(table_));

  // Move the second unselected row to be between two selected rows.
  model_->MoveRows(2, 1, 3);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=0 selection=0 2 4", SelectionStateAsString());
  EXPECT_EQ("[2], [1], [77], [0], [3]", GetRowsInViewOrderAsString(table_));

  // Move the three middle rows to the beginning, including one selected row.
  model_->MoveRows(1, 3, 0);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=4 anchor=3 selection=1 3 4", SelectionStateAsString());
  EXPECT_EQ("[1], [77], [0], [2], [3]", GetRowsInViewOrderAsString(table_));

  table_->set_observer(nullptr);
}

TEST_F(TableViewTest, MoveRowsWithMultipleSelectionAndSort) {
  model_->AddRow(3, 77, 0);

  // Sort ascending by column 0, and hide column 1. The view order should not
  // change during this test.
  table_->ToggleSortOrder(0);
  table_->SetColumnVisibility(1, false);
  const char* kViewOrder = "[0], [1], [2], [3], [77]";
  EXPECT_EQ(kViewOrder, GetRowsInViewOrderAsString(table_));

  TableViewObserverImpl observer;
  table_->set_observer(&observer);

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
  EXPECT_EQ(kViewOrder, GetRowsInViewOrderAsString(table_));

  // Move the unselected rows to the end of the current selection.
  model_->MoveRows(1, 2, 3);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=1 anchor=0 selection=0 1 2", SelectionStateAsString());
  EXPECT_EQ(kViewOrder, GetRowsInViewOrderAsString(table_));

  // Move the unselected rows back to the middle of the selection.
  model_->MoveRows(3, 2, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=0 selection=0 3 4", SelectionStateAsString());
  EXPECT_EQ(kViewOrder, GetRowsInViewOrderAsString(table_));

  // Swap the unselected rows.
  model_->MoveRows(1, 1, 2);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=0 selection=0 3 4", SelectionStateAsString());
  EXPECT_EQ(kViewOrder, GetRowsInViewOrderAsString(table_));

  // Swap the unselected rows again.
  model_->MoveRows(2, 1, 1);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=0 selection=0 3 4", SelectionStateAsString());
  EXPECT_EQ(kViewOrder, GetRowsInViewOrderAsString(table_));

  // Move the unselected rows back to the beginning.
  model_->MoveRows(1, 2, 0);
  EXPECT_EQ(0, observer.GetChangedCountAndClear());
  EXPECT_EQ("active=3 anchor=2 selection=2 3 4", SelectionStateAsString());
  EXPECT_EQ(kViewOrder, GetRowsInViewOrderAsString(table_));

  table_->set_observer(nullptr);
}

// Verifies we don't crash after removing the selected row when there is
// sorting and the anchor/active index also match the selected row.
TEST_F(TableViewTest, FocusAfterRemovingAnchor) {
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

TEST_F(TableViewTest, RemovingInvalidRowIsNoOp) {
  table_->Select(3);
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());
  table_->OnItemsRemoved(4, 1);
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());
  table_->OnItemsRemoved(2, 0);
  EXPECT_EQ("active=3 anchor=3 selection=3", SelectionStateAsString());
}

namespace {

class RemoveFocusChangeListenerDelegate : public WidgetDelegate {
 public:
  explicit RemoveFocusChangeListenerDelegate(Widget* widget)
      : widget_(widget), listener_(nullptr) {}
  ~RemoveFocusChangeListenerDelegate() override = default;

  // WidgetDelegate overrides:
  void DeleteDelegate() override;
  Widget* GetWidget() override { return widget_; }
  const Widget* GetWidget() const override { return widget_; }

  void SetFocusChangeListener(FocusChangeListener* listener);

 private:
  Widget* widget_;
  FocusChangeListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(RemoveFocusChangeListenerDelegate);
};

void RemoveFocusChangeListenerDelegate::DeleteDelegate() {
  widget_->GetFocusManager()->RemoveFocusChangeListener(listener_);
}

void RemoveFocusChangeListenerDelegate::SetFocusChangeListener(
    FocusChangeListener* listener) {
  listener_ = listener;
}

}  // namespace

class TableViewFocusTest : public TableViewTest {
 public:
  TableViewFocusTest() = default;

 protected:
  WidgetDelegate* GetWidgetDelegate(Widget* widget) override;

  RemoveFocusChangeListenerDelegate* GetFocusChangeListenerDelegate() {
    return delegate_.get();
  }

 private:
  std::unique_ptr<RemoveFocusChangeListenerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(TableViewFocusTest);
};

WidgetDelegate* TableViewFocusTest::GetWidgetDelegate(Widget* widget) {
  delegate_ = std::make_unique<RemoveFocusChangeListenerDelegate>(widget);
  return delegate_.get();
}

// Verifies that the active focus is cleared when the widget is destroyed.
// In MD mode, if that doesn't happen a DCHECK in View::DoRemoveChildView(...)
// will trigger due to an attempt to modify the child view list while iterating.
TEST_F(TableViewFocusTest, FocusClearedDuringWidgetDestruction) {
  TestFocusChangeListener listener;
  GetFocusChangeListenerDelegate()->SetFocusChangeListener(&listener);

  widget_->GetFocusManager()->AddFocusChangeListener(&listener);
  table_->RequestFocus();

  ASSERT_EQ(1u, listener.focus_changes().size());
  EXPECT_EQ(listener.focus_changes()[0], ViewPair(nullptr, table_));
  listener.ClearFocusChanges();

  // Now destroy the widget. This should *not* cause a DCHECK in
  // View::DoRemoveChildView(...).
  widget_.reset();
  ASSERT_EQ(1u, listener.focus_changes().size());
  EXPECT_EQ(listener.focus_changes()[0], ViewPair(table_, nullptr));
}

}  // namespace views
