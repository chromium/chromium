// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/models/table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/gfx/font_list.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace ui {

struct AXActionData;
struct AXNodeData;

}  // namespace ui

// A TableView is a view that displays multiple rows with any number of columns.
// TableView is driven by a TableModel. The model returns the contents
// to display. TableModel also has an Observer which is used to notify
// TableView of changes to the model so that the display may be updated
// appropriately.
//
// TableView itself has an observer that is notified when the selection
// changes.
//
// When a table is sorted the model coordinates do not necessarily match the
// view coordinates. All table methods are in terms of the model. If you need to
// convert to view coordinates use ModelToView().
//
// Sorting is done by a locale sensitive string sort. You can customize the
// sort by way of overriding TableModel::CompareValues().
namespace views {

class AXVirtualView;
class FocusRing;
struct GroupRange;
class ScrollView;
class TableGrouper;
class TableHeader;
class TableViewObserver;
class TableViewTestHelper;

// The cell's in the first column of a table can contain:
// - only text
// - a small icon (16x16) and some text
// - a check box and some text
enum TableTypes {
  TEXT_ONLY = 0,
  ICON_AND_TEXT,
};

class VIEWS_EXPORT TableView : public views::View,
                               public ui::TableModelObserver {
 public:
  METADATA_HEADER(TableView);

  // Used by AdvanceActiveVisibleColumn(), AdvanceSelection() and
  // ResizeColumnViaKeyboard() to determine the direction to change the
  // selection.
  enum class AdvanceDirection {
    kDecrement,
    kIncrement,
  };

  // Used to track a visible column. Useful only for the header.
  struct VIEWS_EXPORT VisibleColumn {
    VisibleColumn();
    ~VisibleColumn();

    // The column.
    ui::TableColumn column;

    // Starting x-coordinate of the column.
    int x = 0;

    // Width of the column.
    int width = 0;
  };

  // Describes a sorted column.
  struct VIEWS_EXPORT SortDescriptor {
    SortDescriptor() = default;
    SortDescriptor(int column_id, bool ascending)
        : column_id(column_id), ascending(ascending) {}

    // ID of the sorted column.
    int column_id = -1;

    // Is the sort ascending?
    bool ascending = true;
  };

  using SortDescriptors = std::vector<SortDescriptor>;

  // Creates a new table using the model and columns specified.
  // The table type applies to the content of the first column (text, icon and
  // text, checkbox and text).
  TableView();
  TableView(ui::TableModel* model,
            const std::vector<ui::TableColumn>& columns,
            TableTypes table_type,
            bool single_selection);
  ~TableView() override;

  // Returns a new ScrollView that contains the given |table|.
  static std::unique_ptr<ScrollView> CreateScrollViewWithTable(
      std::unique_ptr<TableView> table);

  // Initialize the table with the appropriate data.
  void Init(ui::TableModel* model,
            const std::vector<ui::TableColumn>& columns,
            TableTypes table_type,
            bool single_selection);

  // Assigns a new model to the table view, detaching the old one if present.
  // If |model| is NULL, the table view cannot be used after this call. This
  // should be called in the containing view's destructor to avoid destruction
  // issues when the model needs to be deleted before the table.
  void SetModel(ui::TableModel* model);
  ui::TableModel* model() const { return model_; }

  // Sets the TableGrouper. TableView does not own |grouper| (common use case is
  // to have TableModel implement TableGrouper).
  void SetGrouper(TableGrouper* grouper);

  // Returns the number of rows in the TableView.
  int GetRowCount() const;

  // Selects the specified item, making sure it's visible.
  void Select(int model_row);

  // Selects all items.
  void SetSelectionAll(bool select);

  // Returns the first selected row in terms of the model.
  int GetFirstSelectedRow() const;

  const ui::ListSelectionModel& selection_model() const {
    return selection_model_;
  }

  // Changes the visibility of the specified column (by id).
  void SetColumnVisibility(int id, bool is_visible);
  bool IsColumnVisible(int id) const;

  // Returns true if the column with the specified id is known (either visible
  // or not).
  bool HasColumn(int id) const;

  // Returns whether an active row and column have been set.
  bool GetHasFocusIndicator() const;

  void set_observer(TableViewObserver* observer) { observer_ = observer; }
  TableViewObserver* observer() const { return observer_; }

  int GetActiveVisibleColumnIndex() const;

  void SetActiveVisibleColumnIndex(int index);

  const std::vector<VisibleColumn>& visible_columns() const {
    return visible_columns_;
  }

  const VisibleColumn& GetVisibleColumn(int index);

  // Sets the width of the column. |index| is in terms of |visible_columns_|.
  void SetVisibleColumnWidth(int index, int width);

  // Modify the table sort order, depending on a clicked column and the previous
  // table sort order. Does nothing if this column is not sortable.
  //
  // When called repeatedly on the same sortable column, the sort order will
  // cycle through three states in order: sorted -> reverse-sorted -> unsorted.
  // When switching from one sort column to another, the previous sort column
  // will be remembered and used as a secondary sort key.
  void ToggleSortOrder(int visible_column_index);

  const SortDescriptors& sort_descriptors() const { return sort_descriptors_; }
  void SetSortDescriptors(const SortDescriptors& descriptors);
  bool GetIsSorted() const { return !sort_descriptors_.empty(); }

  // Maps from the index in terms of the model to that of the view.
  int ModelToView(int model_index) const;

  // Maps from the index in terms of the view to that of the model.
  int ViewToModel(int view_index) const;

  int GetRowHeight() const { return row_height_; }

  bool GetSelectOnRemove() const;
  void SetSelectOnRemove(bool select_on_remove);

  // WARNING: this function forces a sort on every paint, and is therefore
  // expensive! It assumes you are calling SchedulePaint() at intervals for
  // the whole table. If your model is properly notifying the table, this is
  // not needed. This is only used in th extremely rare case, where between the
  // time the SchedulePaint() is called and the paint is processed, the
  // underlying data may change. Also, this only works if the number of rows
  // remains the same.
  bool GetSortOnPaint() const;
  void SetSortOnPaint(bool sort_on_paint);

  // Returns the proper ax sort direction.
  ax::mojom::SortDirection GetFirstSortDescriptorDirection() const;

  TableTypes GetTableType() const;

  // Updates the relative bounds of the virtual accessibility children created
  // in RebuildVirtualAccessibilityChildren(). This function is public so that
  // the table's |header_| can trigger an update when its visible bounds are
  // changed, because its accessibility information is also contained in the
  // table's virtual accessibility children.
  void UpdateVirtualAccessibilityChildrenBounds();

  // Returns the virtual accessibility view corresponding to the specified cell.
  // |row| should be a view index, not a model index.
  // |visible_column_index| indexes into |visible_columns_|.
  AXVirtualView* GetVirtualAccessibilityCell(int row, int visible_column_index);

  // View overrides:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

  // ui::TableModelObserver overrides:
  void OnModelChanged() override;
  void OnItemsChanged(int start, int length) override;
  void OnItemsAdded(int start, int length) override;
  void OnItemsRemoved(int start, int length) override;
  void OnItemsMoved(int old_start, int length, int new_start) override;

 protected:
  // View overrides:
  gfx::Point GetKeyboardContextMenuLocation() override;
  void OnFocus() override;
  void OnBlur() override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  friend class TableViewTestHelper;

  class HighlightPathGenerator;
  struct GroupSortHelper;
  struct SortHelper;

  // Used during painting to determine the range of cell's that need to be
  // painted.
  // NOTE: the row indices returned by this are in terms of the view and column
  // indices in terms of |visible_columns_|.
  struct VIEWS_EXPORT PaintRegion {
    PaintRegion();
    ~PaintRegion();

    int min_row = 0;
    int max_row = 0;
    int min_column = 0;
    int max_column = 0;
  };

  void OnPaintImpl(gfx::Canvas* canvas);

  // Returns the horizontal margin between the bounds of a cell and its
  // contents.
  int GetCellMargin() const;

  // Returns the horizontal spacing between elements (grouper, icon, and text)
  // in a cell.
  int GetCellElementSpacing() const;

  // Does the actual sort and updates the mappings (|view_to_model_| and
  // |model_to_view_|) appropriately. If |schedule_paint| is true,
  // schedules a paint. This should be true, unless called from
  // OnPaint.
  void SortItemsAndUpdateMapping(bool schedule_paint);

  // Used to sort the two rows. Returns a value < 0, == 0 or > 0 indicating
  // whether the row2 comes before row1, row2 is the same as row1 or row1 comes
  // after row2. This invokes CompareValues on the model with the sorted column.
  int CompareRows(int model_row1, int model_row2);

  // Returns the bounds of the specified row.
  gfx::Rect GetRowBounds(int row) const;

  // Returns the bounds of the specified cell. |visible_column_index| indexes
  // into |visible_columns_|.
  gfx::Rect GetCellBounds(int row, int visible_column_index) const;

  // Returns the bounds of the active cell.
  gfx::Rect GetActiveCellBounds() const;

  // Adjusts |bounds| based on where the text should be painted. |bounds| comes
  // from GetCellBounds() and |visible_column_index| is the corresponding column
  // (in terms of |visible_columns_|).
  void AdjustCellBoundsForText(int visible_column_index,
                               gfx::Rect* bounds) const;

  // Creates |header_| if necessary.
  void CreateHeaderIfNecessary(ScrollView* scroll_view);

  // Updates the |x| and |width| of each of the columns in |visible_columns_|.
  void UpdateVisibleColumnSizes();

  // Returns the cell's that need to be painted for the specified region.
  // |bounds| is in terms of |this|.
  PaintRegion GetPaintRegion(const gfx::Rect& bounds) const;

  // Returns the bounds that need to be painted based on the clip set on
  // |canvas|.
  gfx::Rect GetPaintBounds(gfx::Canvas* canvas) const;

  // Invokes SchedulePaint() for the selected rows.
  void SchedulePaintForSelection();

  // Returns the TableColumn matching the specified id.
  ui::TableColumn FindColumnByID(int id) const;

  // Advances the active visible column (from the active visible column index)
  // in the specified direction.
  void AdvanceActiveVisibleColumn(AdvanceDirection direction);

  // Sets the selection to the specified index (in terms of the view).
  void SelectByViewIndex(int view_index);

  // Sets the selection model to |new_selection|.
  void SetSelectionModel(ui::ListSelectionModel new_selection);

  // Advances the selection (from the active index) in the specified direction.
  void AdvanceSelection(AdvanceDirection direction);

  // Sets |model| appropriately based on a event.
  void ConfigureSelectionModelForEvent(const ui::LocatedEvent& event,
                                       ui::ListSelectionModel* model) const;

  // Set the selection state of row at |view_index| to |select|, additionally
  // any other rows in the GroupRange containing |view_index| are updated as
  // well. This does not change the anchor or active index of |model|.
  void SelectRowsInRangeFrom(int view_index,
                             bool select,
                             ui::ListSelectionModel* model) const;

  // Returns the range of the specified model index. If a TableGrouper has not
  // been set this returns a group with a start of |model_index| and length of
  // 1.
  GroupRange GetGroupRange(int model_index) const;

  // Updates a set of accessibility views that expose the visible table contents
  // to assistive software.
  void RebuildVirtualAccessibilityChildren();

  // Clears the set of accessibility views set up in
  // RebuildVirtualAccessibilityChildren(). Useful when the model is in the
  // process of changing but the virtual accessibility children haven't been
  // updated yet, e.g. showing or hiding a column via SetColumnVisibility().
  void ClearVirtualAccessibilityChildren();

  // Helper functions used in UpdateVirtualAccessibilityChildrenBounds() for
  // calculating the accessibility bounds for the header and table rows and
  // cell's.
  gfx::Rect CalculateHeaderRowAccessibilityBounds() const;
  gfx::Rect CalculateHeaderCellAccessibilityBounds(
      const int visible_column_index) const;
  gfx::Rect CalculateTableRowAccessibilityBounds(const int row_index) const;
  gfx::Rect CalculateTableCellAccessibilityBounds(
      const int row_index,
      const int visible_column_index) const;

  // Updates the internal accessibility state and fires the required
  // accessibility events to indicate to assistive software which row is active
  // and which cell is focused, if any.
  void UpdateAccessibilityFocus();

  // Returns the virtual accessibility view corresponding to the specified row.
  // |row| should be a view index, not a model index.
  AXVirtualView* GetVirtualAccessibilityRow(int row);

  // Creates a virtual accessibility view that is used to expose information
  // about the row at |view_index| to assistive software.
  std::unique_ptr<AXVirtualView> CreateRowAccessibilityView(int view_index);

  // Creates a virtual accessibility view that is used to expose information
  // about the cell at the provided coordinates |row_index| and |column_index|
  // to assistive software.
  std::unique_ptr<AXVirtualView> CreateCellAccessibilityView(
      int row_index,
      size_t column_index);

  // Creates a virtual accessibility view that is used to expose information
  // about this header to assistive software.
  std::unique_ptr<AXVirtualView> CreateHeaderAccessibilityView();

  // Updates the accessibility data for |ax_row| to match the data in the view
  // at |view_index| in the table. Returns false if row data not changed.
  bool UpdateVirtualAccessibilityRowData(AXVirtualView* ax_row,
                                         int view_index,
                                         int model_index);

  // The accessibility view |ax_row| callback function that populates the
  // accessibility data for a table row.
  void PopulateAccessibilityRowData(AXVirtualView* ax_row,
                                    ui::AXNodeData* data);

  // The accessibility view |ax_cell| callback function that populates the
  // accessibility data for a table cell.
  void PopulateAccessibilityCellData(AXVirtualView* ax_cell,
                                     ui::AXNodeData* data);

  ui::TableModel* model_ = nullptr;

  std::vector<ui::TableColumn> columns_;

  // The set of visible columns. The values of these point to |columns_|. This
  // may contain a subset of |columns_|.
  std::vector<VisibleColumn> visible_columns_;

  // The active visible column. Used for keyboard access to functionality such
  // as sorting and resizing. -1 if no visible column is active.
  int active_visible_column_index_ = -1;

  // Used to draw a focus indicator around the active cell.
  FocusRing* focus_ring_ = nullptr;

  // The header. This is only created if more than one column is specified or
  // the first column has a non-empty title.
  TableHeader* header_ = nullptr;

  TableTypes table_type_ = TableTypes::TEXT_ONLY;

  bool single_selection_ = true;

  // If |select_on_remove_| is true: when a selected item is removed, if the
  // removed item is not the last item, select its next one; otherwise select
  // its previous one if there is an item.
  // If |select_on_remove_| is false: when a selected item is removed, no item
  // is selected then.
  bool select_on_remove_ = true;

  TableViewObserver* observer_ = nullptr;
  // If |sort_on_paint_| is true, table will sort before painting.
  bool sort_on_paint_ = false;

  // The selection, in terms of the model.
  ui::ListSelectionModel selection_model_;

  gfx::FontList font_list_;

  int row_height_;

  // Width of the ScrollView last time Layout() was invoked. Used to determine
  // when we should invoke UpdateVisibleColumnSizes().
  int last_parent_width_ = 0;

  // The width we layout to. This may differ from |last_parent_width_|.
  int layout_width_ = 0;

  // Current sort.
  SortDescriptors sort_descriptors_;

  // Mappings used when sorted.
  std::vector<int> view_to_model_;
  std::vector<int> model_to_view_;

  TableGrouper* grouper_ = nullptr;

  // True if in SetVisibleColumnWidth().
  bool in_set_visible_column_width_ = false;

  // Keeps track whether a focus change has occurred so that the accessibility
  // focus would be updated after all the virtual accessibility children. Some
  // screen readers don't process the accessibility focus event right away and
  // by the time they do the focused virtual accessibility child is no longer
  // there. We need to fire the accessibility focus event after the virtual
  // accessibility children have been updated.
  bool needs_update_accessibility_focus_ = false;

  DISALLOW_COPY_AND_ASSIGN(TableView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_H_
