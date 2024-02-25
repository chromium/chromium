// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_TABLE_LAYOUT_H_
#define UI_VIEWS_LAYOUT_TABLE_LAYOUT_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/layout/layout_types.h"

namespace views {

// TableLayout is a LayoutManager that positions child views in a table. You
// define the structure of the table independently of adding child views.
// The following creates a trivial table with two columns separated by a column
// with padding, and two rows:
//
// layout->AddColumn(LayoutAlignment::kStretch,   // Views are horizontally
//                                                // resized to fill column.
//                   LayoutAlignment::kStretch,   // Views starting in this
//                                                // column are vertically
//                                                // resized.
//                    1.0f,                       // This column has a resize
//                                                // weight of 1.
//                    ColumnSize::kUsePreferred,  // Use the preferred size of
//                                                // the view.
//                    0,                          // Ignored for kUsePreferred.
//                    0);                         // A minimum width of 0.
// layout->AddPaddingColumn(kFixedSize,           // The padding column is not
//                                                // resizable.
//                          10);                  // And has a width of 10 DIP.
// layout->AddColumn(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
//                   kFixedSize, ColumnSize::kUsePreferred, 0, 0);
// layout->AddRows(2, kFixedSize);                // These rows aren't
//                                                // vertically resizable.
//
// Now (or before setting up the table, either way works), add the views:
//
// host->AddChildView(v1);  // Will go in the first column, first row.
// host->AddChildView(v2);  // Will go in the last column, first row.
// host->AddChildView(v3);  // Will go in the first column, second row.
// host->AddChildView(v4);  // Will go in the last column, second row.
//
// Notice you need not skip over padding columns, that's done for you.
//
// When adding a column you give it the default alignment for all views
// originating in that column. You can override this for specific views by
// setting a class property on them. For example, the following forces a view to
// have a horizontal and vertical alignment of leading regardless of that
// defined for the column:
//
// view->SetProperty(kTableHorizAlignKey, LayoutAlignment::kStart);
// view->SetProperty(kTableVertAlignKey, LayoutAlignment::kStart);
//
// If the view using TableLayout is given a size bigger than the preferred,
// columns and rows with a resize percent > 0 are resized. Each column/row is
// given (resize / total_resize * delta) extra DIPs. Only views with an
// alignment of LayoutAlignment::kStretch are given extra space, others are
// aligned in the provided space.
//
// TableLayout allows you to force columns to have the same width. This is done
// using LinkColumnSizes().
//
// If the host view is sized smaller than the preferred width, TableLayout may
// use the minimum size. The minimum size is considered only for views whose
// preferred width was not explicitly specified and where the containing columns
// are resizable (resize > 0) and don't have a fixed width.
class VIEWS_EXPORT TableLayout : public LayoutManagerBase {
 public:
  // Use for `horizontal_resize` or `vertical_resize` when the column or row is
  // not resizable.
  static constexpr float kFixedSize = 0.0f;

  // An enumeration of the possible ways the size of a column may be obtained.
  enum class ColumnSize {
    // The column size is fixed.
    kFixed,

    // The preferred size of the view is used to determine the column size.
    kUsePreferred
  };

  TableLayout();

  TableLayout(const TableLayout&) = delete;
  TableLayout& operator=(const TableLayout&) = delete;

  ~TableLayout() override;

  // Adds a column. The alignment gives the default alignment for views added
  // with no explicit alignment. fixed_width gives a specific width for the
  // column, and is only used if size_type == kFixed. min_width gives the
  // minimum width for the column.
  //
  // If none of the columns are resizable, the views are only made as wide as
  // the widest views in each column, even if extra space is provided. In other
  // words, TableLayout does not automatically resize views unless the column is
  // marked as resizable.
  TableLayout& AddColumn(LayoutAlignment h_align,
                         LayoutAlignment v_align,
                         float horizontal_resize,
                         ColumnSize size_type,
                         int fixed_width,
                         int min_width);

  // Adds a padding column, used to provide horizontal white space between
  // views. Padding columns don't have any views, but are counted in column
  // spans.
  TableLayout& AddPaddingColumn(float horizontal_resize, int width);

  // Adds `n` new rows with the specified height (0 for unspecified height).
  TableLayout& AddRows(size_t n, float vertical_resize, int height = 0);

  // Adds a padding row, used to provide vertical white space between views.
  // Padding rows don't have any views, but are counted in row spans.
  TableLayout& AddPaddingRow(float vertical_resize, int height);

  // Forces the specified columns to have the same size. The size of
  // linked columns is that of the max of the specified columns.
  // For example, the following forces the first and
  // second column to have the same size: LinkColumnSizes({0, 1});
  TableLayout& LinkColumnSizes(std::vector<size_t> columns);

  // When sizing linked columns, columns wider than |size_limit| are ignored.
  TableLayout& SetLinkedColumnSizeLimit(int size_limit);

  TableLayout& SetMinimumSize(const gfx::Size& size);

 protected:
  ProposedLayout CalculateProposedLayout(
      const SizeBounds& size_bounds) const override;

 private:
  enum class SizeCalculationType {
    kPreferred,
    kMinimum,
  };
  class Column;
  class Row;
  struct ViewState;

  // Creates ViewStates for the children and populates the "view_states_by_..."
  // vectors with them.
  void SetViewStates() const;

  // Sizes the columns/rows as appropriate. Returns the preferred size of the
  // host view.
  gfx::Size SizeRowsAndColumns(const SizeBounds& bounds) const;

  // If `view_state`'s remaining height is > 0, it is distributed among the rows
  // it touches. This is used during layout to make sure the rows can
  // accommodate a view.
  void DistributeRemainingHeight(ViewState& view_state) const;

  // Sets the size of each linked column to be the same.
  void UnifyLinkedColumnSizes() const;

  // Makes sure the columns touched by view state are big enough for the view.
  void DistributeRemainingWidth(ViewState& view_state) const;

  // Returns the total width needed for these columns.
  int LayoutWidth() const;

  // Calculates the preferred width of each view, as well as updating the
  // ViewStates' `remaining_width`.
  void CalculateSize(SizeCalculationType type,
                     const std::vector<raw_ptr<ViewState, VectorExperimental>>&
                         view_states) const;

  // Distributes `delta` among the resizable columns.
  void Resize(int delta) const;

  // Used when TableLayout is given a size smaller than the preferred width.
  // `delta` is negative and the difference between the preferred width and the
  // target width.
  void ResizeUsingMin(int delta) const;

  // Only use the minimum size if any of the columns the view is in
  // is resizable. Fixed columns will retain their fixed width.
  bool CanUseMinimum(const ViewState& view_state) const;

  // Columns.
  mutable std::vector<Column> columns_;

  // Rows.
  mutable std::vector<Row> rows_;

  // Columns wider than this limit will be ignored when computing linked
  // columns' sizes.
  std::optional<int> linked_column_size_limit_;

  // Minimum preferred size.
  gfx::Size minimum_size_;

  // ViewStates sorted based on row_span in ascending order.
  mutable std::vector<std::unique_ptr<ViewState>> view_states_by_row_span_;

  // ViewStates sorted based on column_span in ascending order.
  mutable std::vector<raw_ptr<ViewState, VectorExperimental>>
      view_states_by_col_span_;
};

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_TABLE_LAYOUT_H_
