// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/grid_layout.h"

#include <cmath>
#include <numeric>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "ui/views/border.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
namespace {

// Used when calculating the minimum size to build up how much each column is
// shrunk.
struct ColumnMinResizeData {
  // The column being resized.
  Column* column;

  // The remaining amount of space available (the difference between the
  // preferred and minimum).
  int available = 0;

  // How much to shrink the preferred by.
  int delta = 0;
};

}  // namespace

// LayoutElement ------------------------------------------------------

// A LayoutElement has a size and location along one axis. It contains
// methods that are used along both axis.
class LayoutElement {
 public:
  // Invokes ResetSize on all the layout elements.
  template <class T>
  static void ResetSizes(std::vector<std::unique_ptr<T>>* elements) {
    // Reset the layout width of each column.
    for (const auto& element : *elements)
      element->ResetSize();
  }

  // Sets the location of each element to be the sum of the sizes of the
  // preceding elements.
  template <class T>
  static void CalculateLocationsFromSize(
      std::vector<std::unique_ptr<T>>* elements) {
    // Reset the layout width of each column.
    int location = 0;
    for (const auto& element : *elements) {
      element->SetLocation(location);
      location += element->Size();
    }
  }

  // Distributes delta among the resizable elements.
  // Each resizable element is given ResizePercent / total_percent * delta
  // pixels extra of space.
  template <class T>
  static void DistributeDelta(int delta,
                              std::vector<std::unique_ptr<T>>* elements) {
    if (delta == 0)
      return;

    float total_percent = 0;
    int resize_count = 0;
    for (const auto& element : *elements) {
      total_percent += element->ResizePercent();
      if (element->ResizePercent() > 0)
        resize_count++;
    }
    if (total_percent == 0) {
      // None of the elements are resizable, return.
      return;
    }
    int remaining = delta;
    int resized = resize_count;
    for (const auto& element : *elements) {
      if (element->ResizePercent() > 0) {
        int to_give;
        if (--resized == 0) {
          to_give = remaining;
        } else {
          to_give = static_cast<int>(delta *
                                    (element->resize_percent_ / total_percent));
          remaining -= to_give;
        }
        element->SetSize(element->Size() + to_give);
      }
    }
  }

  // Returns the sum of the size of the elements from start to start + length.
  template <class T>
  static int TotalSize(int start,
                       int length,
                       std::vector<std::unique_ptr<T>>* elements) {
    DCHECK_GE(start, 0);
    DCHECK_GT(length, 0);
    DCHECK_LE(size_t{start + length}, elements->size());
    return std::accumulate(
        elements->cbegin() + start, elements->cbegin() + start + length, 0,
        [](int size, const auto& elem) { return size + elem->Size(); });
  }

  explicit LayoutElement(float resize_percent)
      : resize_percent_(resize_percent) {
    DCHECK(resize_percent >= 0);
  }

  virtual ~LayoutElement() = default;

  void SetLocation(int location) {
    location_ = location;
  }

  int Location() {
    return location_;
  }

  // Adjusts the size of this LayoutElement to be the max of the current size
  // and the specified size.
  virtual void AdjustSize(int size) {
    size_ = std::max(size_, size);
  }

  // Resets the size to the initial size. This sets the size to 0, but
  // subclasses that have a different initial size should override.
  virtual void ResetSize() {
    SetSize(0);
  }

  void SetSize(int size) {
    size_ = size;
  }

  int Size() {
    return size_;
  }

  void SetResizePercent(float percent) {
    resize_percent_ = percent;
  }

  float ResizePercent() {
    return resize_percent_;
  }

  bool IsResizable() {
    return resize_percent_ > 0;
  }

 private:
  float resize_percent_;
  int location_;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(LayoutElement);
};

// Column -------------------------------------------------------------

// As the name implies, this represents a Column. Column contains default
// values for views originating in this column.
class Column : public LayoutElement {
 public:
  Column(GridLayout::Alignment h_align,
         GridLayout::Alignment v_align,
         float resize_percent,
         GridLayout::SizeType size_type,
         int fixed_width,
         int min_width,
         bool is_padding)
      : LayoutElement(resize_percent),
        h_align_(h_align),
        v_align_(v_align),
        size_type_(size_type),
        same_size_column_(-1),
        fixed_width_(fixed_width),
        min_width_(min_width),
        is_padding_(is_padding),
        master_column_(nullptr) {}

  ~Column() override = default;

  GridLayout::Alignment h_align() { return h_align_; }
  GridLayout::Alignment v_align() { return v_align_; }

  // LayoutElement:
  void ResetSize() override;

 private:
  friend class ColumnSet;
  friend class GridLayout;

  Column* GetLastMasterColumn();

  // Determines the max size of all linked columns, and sets each column
  // to that size. This should only be used for the master column.
  void UnifyLinkedColumnSizes(int size_limit);

  void AdjustSize(int size) override;

  const GridLayout::Alignment h_align_;
  const GridLayout::Alignment v_align_;
  const GridLayout::SizeType size_type_;
  int same_size_column_;
  const int fixed_width_;
  const int min_width_;

  const bool is_padding_;

  // If multiple columns have their sizes linked, one is the
  // master column. The master column is identified by the
  // master_column field being equal to itself. The master columns
  // same_size_columns field contains the set of Columns with the
  // the same size. Columns who are linked to other columns, but
  // are not the master column have their master_column pointing to
  // one of the other linked columns. Use the method GetLastMasterColumn
  // to resolve the true master column.
  std::vector<Column*> same_size_columns_;
  Column* master_column_;

  DISALLOW_COPY_AND_ASSIGN(Column);
};

void Column::ResetSize() {
  if (size_type_ == GridLayout::FIXED) {
    SetSize(fixed_width_);
  } else {
    SetSize(min_width_);
  }
}

Column* Column::GetLastMasterColumn() {
  if (master_column_ == nullptr) {
    return nullptr;
  }
  if (master_column_ == this) {
    return this;
  }
  return master_column_->GetLastMasterColumn();
}

void Column::UnifyLinkedColumnSizes(int size_limit) {
  DCHECK(master_column_ == this);

  // Accumulate the size first.
  int size = 0;
  for (auto* column : same_size_columns_) {
    if (column->Size() <= size_limit)
      size = std::max(size, column->Size());
  }

  // Then apply it.
  for (auto* column : same_size_columns_)
    column->SetSize(std::max(size, column->Size()));
}

void Column::AdjustSize(int size) {
  if (size_type_ == GridLayout::USE_PREF)
    LayoutElement::AdjustSize(size);
}

// Row -------------------------------------------------------------

class Row : public LayoutElement {
 public:
  Row(int height, float resize_percent, ColumnSet* column_set)
    : LayoutElement(resize_percent),
      height_(height),
      column_set_(column_set),
      max_ascent_(0),
      max_descent_(0) {
  }

  ~Row() override = default;

  void ResetSize() override {
    max_ascent_ = max_descent_ = 0;
    SetSize(height_);
  }

  ColumnSet* column_set() {
    return column_set_;
  }

  // Adjusts the size to accommodate the specified ascent/descent.
  void AdjustSizeForBaseline(int ascent, int descent) {
    max_ascent_ = std::max(ascent, max_ascent_);
    max_descent_ = std::max(descent, max_descent_);
    AdjustSize(max_ascent_ + max_descent_);
  }

  int max_ascent() const {
    return max_ascent_;
  }

  int max_descent() const {
    return max_descent_;
  }

 private:
  const int height_;
  // The column set used for this row; null for padding rows.
  ColumnSet* column_set_;

  int max_ascent_;
  int max_descent_;

  DISALLOW_COPY_AND_ASSIGN(Row);
};

// ViewState -------------------------------------------------------------

// Identifies the location in the grid of a particular view, along with
// placement information and size information.
struct ViewState {
  ViewState(ColumnSet* column_set,
            View* view,
            int start_col,
            int start_row,
            int col_span,
            int row_span,
            GridLayout::Alignment h_align,
            GridLayout::Alignment v_align,
            int pref_width,
            int pref_height)
      : column_set(column_set),
        view(view),
        start_col(start_col),
        start_row(start_row),
        col_span(col_span),
        row_span(row_span),
        h_align(h_align),
        v_align(v_align),
        pref_width_fixed(pref_width > 0),
        pref_height_fixed(pref_height > 0),
        width(pref_width),
        height(pref_height) {
    DCHECK(view && start_col >= 0 && start_row >= 0 && col_span > 0 &&
           row_span > 0 && start_col < column_set->num_columns() &&
           (start_col + col_span) <= column_set->num_columns());
  }

  ColumnSet* const column_set;
  View* const view;
  const int start_col;
  const int start_row;
  const int col_span;
  const int row_span;
  const GridLayout::Alignment h_align;
  const GridLayout::Alignment v_align;

  // If true, the height/width were explicitly set and the view's preferred and
  // minimum size is ignored.
  const bool pref_width_fixed;
  const bool pref_height_fixed;

  // The preferred size, only set during the preferred size pass
  // (SizeCalculationType::PREFERRED).
  gfx::Size pref_size;

  // The width/height. This is one of possible three values:
  // . an explicitly set value (if pref_X_fixed is true). If an explicitly set
  //   value was provided, then this value never changes.
  // . the preferred width.
  // . the minimum width.
  // If the value wasn't explicitly set, then whether the value is the preferred
  // or minimum depends upon the pass.
  int width;
  int height;

  // Used during layout. Gives how much width/height has not yet been
  // distributed to the columns/rows the view is in.
  int remaining_width = 0;
  int remaining_height = 0;

  // The baseline. Only used if the view is vertically aligned along the
  // baseline.
  int baseline = -1;
};

static bool CompareByColumnSpan(const ViewState* v1, const ViewState* v2) {
  return v1->col_span < v2->col_span;
}

static bool CompareByRowSpan(const std::unique_ptr<ViewState>& v1,
                             const ViewState* v2) {
  return v1->row_span < v2->row_span;
}

// ColumnSet -------------------------------------------------------------

ColumnSet::ColumnSet(int id) : id_(id), linked_column_size_limit_(INT_MAX) {}

ColumnSet::~ColumnSet() = default;

void ColumnSet::AddPaddingColumn(float resize_percent, int width) {
  AddColumn(GridLayout::FILL, GridLayout::FILL, resize_percent,
            GridLayout::FIXED, width, width, true);
}

void ColumnSet::AddColumn(GridLayout::Alignment h_align,
                          GridLayout::Alignment v_align,
                          float resize_percent,
                          GridLayout::SizeType size_type,
                          int fixed_width,
                          int min_width) {
  AddColumn(h_align, v_align, resize_percent, size_type, fixed_width,
            min_width, false);
}

void ColumnSet::LinkColumnSizes(const std::vector<int>& columns) {
  if (columns.size() <= 1)
    return;
  int last = columns[0];
  for (size_t i = 1; i < columns.size(); ++i) {
    int next = columns[i];
    DCHECK_GE(next, 0);
    DCHECK_LT(next, num_columns());
    columns_[last]->same_size_column_ = next;
    last = next;
  }
}

void ColumnSet::AddColumn(GridLayout::Alignment h_align,
                          GridLayout::Alignment v_align,
                          float resize_percent,
                          GridLayout::SizeType size_type,
                          int fixed_width,
                          int min_width,
                          bool is_padding) {
  columns_.push_back(std::make_unique<Column>(h_align, v_align, resize_percent,
                                              size_type, fixed_width, min_width,
                                              is_padding));
}

void ColumnSet::AddViewState(ViewState* view_state) {
  // view_states are ordered by column_span (in ascending order).
  auto i = std::lower_bound(view_states_.begin(), view_states_.end(),
                            view_state, CompareByColumnSpan);
  view_states_.insert(i, view_state);
}

void ColumnSet::CalculateMasterColumns() {
  for (const auto& column : columns_) {
    int same_size_column_index = column->same_size_column_;
    if (same_size_column_index != -1) {
      DCHECK(same_size_column_index >= 0 &&
             same_size_column_index < static_cast<int>(columns_.size()));
      Column* master_column = column->master_column_;
      Column* same_size_column = columns_[same_size_column_index].get();
      Column* same_size_column_master = same_size_column->master_column_;
      if (master_column == nullptr) {
        // Current column is not linked to any other column.
        if (same_size_column_master == nullptr) {
          // Both columns are not linked.
          column->master_column_ = column.get();
          same_size_column->master_column_ = column.get();
          column->same_size_columns_.push_back(same_size_column);
          column->same_size_columns_.push_back(column.get());
        } else {
          // Column to link to is linked with other columns.
          // Add current column to list of linked columns in other columns
          // master column.
          same_size_column->GetLastMasterColumn()->same_size_columns_.push_back(
              column.get());
          // And update the master column for the current column to that
          // of the same sized column.
          column->master_column_ = same_size_column;
        }
      } else {
        // Current column is already linked with another column.
        if (same_size_column_master == nullptr) {
          // Column to link with is not linked to any other columns.
          // Update it's master_column.
          same_size_column->master_column_ = column.get();
          // Add linked column to list of linked column.
          column->GetLastMasterColumn()->same_size_columns_.
              push_back(same_size_column);
        } else if (column->GetLastMasterColumn() !=
                   same_size_column->GetLastMasterColumn()) {
          // The two columns are already linked with other columns.
          std::vector<Column*>* same_size_columns =
              &(column->GetLastMasterColumn()->same_size_columns_);
          std::vector<Column*>* other_same_size_columns =
              &(same_size_column->GetLastMasterColumn()->same_size_columns_);
          // Add all the columns from the others master to current columns
          // master.
          same_size_columns->insert(same_size_columns->end(),
                                     other_same_size_columns->begin(),
                                     other_same_size_columns->end());
          // The other master is no longer a master, clear its vector of
          // linked columns, and reset its master_column.
          other_same_size_columns->clear();
          same_size_column->GetLastMasterColumn()->master_column_ =
              column.get();
        }
      }
    }
  }
  AccumulateMasterColumns();
}

void ColumnSet::AccumulateMasterColumns() {
  DCHECK(master_columns_.empty());
  for (const auto& column : columns_) {
    Column* master_column = column->GetLastMasterColumn();
    if (master_column && !base::Contains(master_columns_, master_column)) {
      master_columns_.push_back(master_column);
    }
    // At this point, GetLastMasterColumn may not == master_column
    // (may have to go through a few Columns)_. Reset master_column to
    // avoid hops.
    column->master_column_ = master_column;
  }
}

void ColumnSet::UnifyLinkedColumnSizes() {
  for (auto* column : master_columns_)
    column->UnifyLinkedColumnSizes(linked_column_size_limit_);
}

void ColumnSet::UpdateRemainingWidth(ViewState* view_state) {
  view_state->remaining_width -= LayoutElement::TotalSize(
      view_state->start_col, view_state->col_span, &columns_);
}

void ColumnSet::DistributeRemainingWidth(ViewState* view_state) {
  // This is nearly the same as that for rows, but differs in so far as how
  // Rows and Columns are treated. Rows have two states, resizable or not.
  // Columns have three, resizable, USE_PREF or not resizable. This results
  // in slightly different handling for distributing unaccounted size.
  int width = view_state->remaining_width;
  if (width <= 0) {
    // The columns this view is in are big enough to accommodate it.
    return;
  }

  // Determine which columns are resizable, and which have a size type
  // of USE_PREF.
  int resizable_columns = 0;
  int pref_size_columns = 0;
  int start_col = view_state->start_col;
  int max_col = view_state->start_col + view_state->col_span;
  float total_resize = 0;
  for (int i = start_col; i < max_col; ++i) {
    if (columns_[i]->IsResizable()) {
      total_resize += columns_[i]->ResizePercent();
      resizable_columns++;
    } else if (columns_[i]->size_type_ == GridLayout::USE_PREF) {
      pref_size_columns++;
    }
  }

  if (resizable_columns > 0) {
    // There are resizable columns, give them the remaining width. The extra
    // width is distributed using the resize values of each column.
    int remaining_width = width;
    for (int i = start_col, resize_i = 0; i < max_col; ++i) {
      if (columns_[i]->IsResizable()) {
        resize_i++;
        int delta = (resize_i == resizable_columns) ? remaining_width :
            static_cast<int>(width * columns_[i]->ResizePercent() /
                             total_resize);
        remaining_width -= delta;
        columns_[i]->SetSize(columns_[i]->Size() + delta);
      }
    }
  } else if (pref_size_columns > 0) {
    // None of the columns are resizable, distribute the width among those
    // that use the preferred size.
    int to_distribute = width / pref_size_columns;
    for (int i = start_col; i < max_col; ++i) {
      if (columns_[i]->size_type_ == GridLayout::USE_PREF) {
        width -= to_distribute;
        if (width < to_distribute)
          to_distribute += width;
        columns_[i]->SetSize(columns_[i]->Size() + to_distribute);
      }
    }
  }
}

int ColumnSet::LayoutWidth() {
  int width = 0;
  for (const auto& column : columns_)
    width += column->Size();

  return width;
}

int ColumnSet::GetColumnWidth(int start_col, int col_span) {
  return LayoutElement::TotalSize(start_col, col_span, &columns_);
}

void ColumnSet::ResetColumnXCoordinates() {
  LayoutElement::CalculateLocationsFromSize(&columns_);
}

void ColumnSet::CalculateSize(SizeCalculationType type) {
#if DCHECK_IS_ON()
  // SizeCalculationType::MINIMUM must be preceeded by a request for
  // SizeCalculationType::PREFERRED.
  DCHECK(type == SizeCalculationType::PREFERRED ||
         last_calculation_type_ == PREFERRED);
  last_calculation_type_ = type;
#endif
  // Reset the size and remaining sizes.
  for (auto* view_state : view_states_) {
    if (!view_state->pref_width_fixed || !view_state->pref_height_fixed) {
      gfx::Size size;
      if (type == SizeCalculationType::MINIMUM && CanUseMinimum(*view_state)) {
        // If the min size is bigger than the preferred, use the preferred.
        // This relies on MINIMUM being calculated immediately after PREFERRED,
        // which the rest of this code relies on as well.
        size = view_state->view->GetMinimumSize();
        if (size.width() > view_state->width)
          size.set_width(view_state->width);
        if (size.height() > view_state->height)
          size.set_height(view_state->height);
      } else {
        size = view_state->view->GetPreferredSize();
        view_state->pref_size = size;
      }
      if (!view_state->pref_width_fixed)
        view_state->width = size.width();
      if (!view_state->pref_height_fixed)
        view_state->height = size.height();
    }
    view_state->remaining_width = view_state->width;
    view_state->remaining_height = view_state->height;
  }

  LayoutElement::ResetSizes(&columns_);

  // Distribute the size of each view with a col span == 1.
  auto view_state_iterator = view_states_.begin();
  for (; view_state_iterator != view_states_.end() &&
         (*view_state_iterator)->col_span == 1; ++view_state_iterator) {
    ViewState* view_state = *view_state_iterator;
    Column* column = columns_[view_state->start_col].get();
    column->AdjustSize(view_state->width);
    view_state->remaining_width -= column->Size();
  }

  // Make sure all linked columns have the same size.
  UnifyLinkedColumnSizes();

  // Distribute the size of each view with a column span > 1.
  for (; view_state_iterator != view_states_.end(); ++view_state_iterator) {
    ViewState* view_state = *view_state_iterator;

    // Update the remaining_width from columns this view_state touches.
    UpdateRemainingWidth(view_state);

    // Distribute the remaining width.
    DistributeRemainingWidth(view_state);

    // Update the size of linked columns.
    // This may need to be combined with previous step.
    UnifyLinkedColumnSizes();
  }
}

void ColumnSet::Resize(int delta, bool honors_min_width) {
  if (delta < 0 && honors_min_width) {
    // DistributeDelta() assumes resizable columns can equally be shrunk. That
    // isn't desired when given a size smaller than the prefered. Instead the
    // columns need to be resized but bounded by the minimum. ResizeUsingMin()
    // does this.
    ResizeUsingMin(delta);
    return;
  }

  LayoutElement::DistributeDelta(delta, &columns_);
}

void ColumnSet::ResizeUsingMin(int total_delta) {
  DCHECK_LE(total_delta, 0);

  // |total_delta| is negative, but easier to do operations when positive.
  total_delta = std::abs(total_delta);

  std::vector<int> preferred_column_sizes(columns_.size());
  for (size_t i = 0; i < columns_.size(); ++i)
    preferred_column_sizes[i] = columns_[i]->Size();

  // Recalculate the sizes using the min.
  CalculateSize(ColumnSet::SizeCalculationType::MINIMUM);

  // Build up the set of columns that can be shrunk in |resize_data|, this
  // iteration also resets the size of the column back to the preferred size.
  std::vector<ColumnMinResizeData> resize_data;
  float total_percent = 0;
  for (size_t i = 0; i < columns_.size(); ++i) {
    Column* column = columns_[i].get();
    const int available =
        std::max(0, preferred_column_sizes[i] -
                        std::max(column->min_width_, column->Size()));
    DCHECK_GE(available, 0);
    // Set the size back to preferred. We'll reset the size if necessary later.
    column->SetSize(preferred_column_sizes[i]);
    if (column->ResizePercent() <= 0 || available == 0)
      continue;
    resize_data.push_back({column, available, 0});
    total_percent += column->ResizePercent();
  }
  if (resize_data.empty())
    return;

  // Loop through the columns updating the amount available and the amount to
  // resize. This may take multiple iterations if the column min is hit.
  // Generally there are not that many columns in a GridLayout, so this code is
  // not optimized. Any time the column hits the min it is removed from
  // |resize_data|.
  while (!resize_data.empty() && total_delta > 0) {
    float next_iteration_total_percent = total_percent;
    int next_iteration_delta = total_delta;
#if DCHECK_IS_ON()
    const int initial_delta = total_delta;
#endif
    for (size_t i = resize_data.size(); i > 0; --i) {
      ColumnMinResizeData& data = resize_data[i - 1];
      int delta =
          std::min(data.available,
                   static_cast<int>(total_delta * data.column->ResizePercent() /
                                    total_percent));
      // Make sure at least one column in resized (rounding errors may prevent
      // that).
      if (i == 1 && delta == 0 && next_iteration_delta == total_delta)
        delta = 1;
      next_iteration_delta -= delta;
      data.delta += delta;
      data.available -= delta;
      if (data.available == 0) {
        data.column->SetSize(data.column->Size() - data.delta);
        next_iteration_total_percent -= data.column->ResizePercent();
        resize_data.erase(resize_data.begin() + (i - 1));
      }
    }
#if DCHECK_IS_ON()
    DCHECK(next_iteration_delta < initial_delta);
#endif
    total_delta = next_iteration_delta;
    total_percent = next_iteration_total_percent;
  }

  for (const ColumnMinResizeData& data : resize_data)
    data.column->SetSize(data.column->Size() - data.delta);
}

bool ColumnSet::CanUseMinimum(const ViewState& view_state) const {
  const auto resizable = [](const auto& col) {
    return col->ResizePercent() > 0 && col->size_type_ != GridLayout::FIXED;
  };
  return std::all_of(
      columns_.cbegin() + view_state.start_col,
      columns_.cbegin() + view_state.start_col + view_state.col_span,
      resizable);
}

// GridLayout -------------------------------------------------------------

GridLayout::GridLayout() = default;

GridLayout::~GridLayout() = default;

ColumnSet* GridLayout::AddColumnSet(int id) {
  DCHECK(GetColumnSet(id) == nullptr);
  column_sets_.push_back(base::WrapUnique(new ColumnSet(id)));
  return column_sets_.back().get();
}

ColumnSet* GridLayout::GetColumnSet(int id) {
  const auto i = std::find_if(
      column_sets_.cbegin(), column_sets_.cend(),
      [id](const auto& column_set) { return column_set->id_ == id; });
  return (i == column_sets_.cend()) ? nullptr : i->get();
}

void GridLayout::StartRowWithPadding(float vertical_resize, int column_set_id,
                                     float padding_resize, int padding) {
  AddPaddingRow(padding_resize, padding);
  StartRow(vertical_resize, column_set_id);
}

void GridLayout::StartRow(float vertical_resize,
                          int column_set_id,
                          int height) {
  DCHECK_GE(height, 0);
  ColumnSet* column_set = GetColumnSet(column_set_id);
  DCHECK(column_set);
  AddRow(std::make_unique<Row>(height, vertical_resize, column_set));
}

void GridLayout::AddPaddingRow(float vertical_resize, int pixel_count) {
  AddRow(std::make_unique<Row>(pixel_count, vertical_resize, nullptr));
}

void GridLayout::SkipColumns(int col_count) {
  DCHECK(col_count > 0);
  next_column_ += col_count;
  DCHECK(current_row_col_set_ &&
         next_column_ <= current_row_col_set_->num_columns());
  SkipPaddingColumns();
}

void GridLayout::AddExistingView(View* view, int col_span, int row_span) {
  DCHECK(view->parent() && view->parent() == host_)
      << "Use AddView() to add a new View that isn't already parented to "
         "|host_|.";
  DCHECK(current_row_col_set_ &&
         next_column_ < current_row_col_set_->num_columns());
  Column* column = current_row_col_set_->columns_[next_column_].get();
  AddExistingView(view, col_span, row_span, column->h_align(),
                  column->v_align());
}

void GridLayout::AddViewImpl(std::unique_ptr<View> view,
                             int col_span,
                             int row_span) {
  DCHECK(current_row_col_set_ &&
         next_column_ < current_row_col_set_->num_columns());
  Column* column = current_row_col_set_->columns_[next_column_].get();
  AddViewImpl(std::move(view), col_span, row_span, column->h_align(),
              column->v_align(), 0, 0);
}

void GridLayout::AddExistingView(View* view,
                                 int col_span,
                                 int row_span,
                                 Alignment h_align,
                                 Alignment v_align,
                                 int pref_width,
                                 int pref_height) {
  DCHECK(view->parent() && view->parent() == host_)
      << "Use AddView() to add a new View that isn't already parented to "
         "|host_|.";
  DCHECK(current_row_col_set_ && col_span > 0 && row_span > 0 &&
         (next_column_ + col_span) <= current_row_col_set_->num_columns());
  // We don't support baseline alignment of views spanning rows. Please add if
  // you need it.
  DCHECK(v_align != BASELINE || row_span == 1);
  AddViewState(std::make_unique<ViewState>(
      current_row_col_set_, view, next_column_, current_row_, col_span,
      row_span, h_align, v_align, pref_width, pref_height));
}

void GridLayout::AddViewImpl(std::unique_ptr<View> view,
                             int col_span,
                             int row_span,
                             Alignment h_align,
                             Alignment v_align,
                             int pref_width,
                             int pref_height) {
  DCHECK(current_row_col_set_ && col_span > 0 && row_span > 0 &&
         (next_column_ + col_span) <= current_row_col_set_->num_columns());
  // We don't support baseline alignment of views spanning rows. Please add if
  // you need it.
  DCHECK(v_align != BASELINE || row_span == 1);
  adding_view_ = true;
  View* view_ptr = host_->AddChildView(std::move(view));
  adding_view_ = false;
  AddViewState(std::make_unique<ViewState>(
      current_row_col_set_, view_ptr, next_column_, current_row_, col_span,
      row_span, h_align, v_align, pref_width, pref_height));
}

static void CalculateSize(int pref_size, GridLayout::Alignment alignment,
                          int* location, int* size) {
  if (alignment != GridLayout::FILL) {
    int available_size = *size;
    *size = std::min(*size, pref_size);
    switch (alignment) {
      case GridLayout::LEADING:
        // Nothing to do, location already points to start.
        break;
      case GridLayout::BASELINE:  // If we were asked to align on baseline, but
                                  // the view doesn't have a baseline, fall back
                                  // to center.
      case GridLayout::CENTER:
        *location += (available_size - *size) / 2;
        break;
      case GridLayout::TRAILING:
        *location = *location + available_size - *size;
        break;
      default:
        NOTREACHED();
    }
  }
}

void GridLayout::Installed(View* host) {
  host_ = host;
}

void GridLayout::ViewAdded(View* host, View* view) {
  DCHECK(host_ == host && adding_view_);
}

void GridLayout::ViewRemoved(View* host, View* view) {
  DCHECK(host_ == host);
}

void GridLayout::Layout(View* host) {
  DCHECK(host_ == host);
  // SizeRowsAndColumns sets the size and location of each row/column, but
  // not of the views.
  gfx::Size pref;
  SizeRowsAndColumns(true, host_->width(), host_->height(), &pref);

  // Size each view.
  for (const auto& view_state : view_states_) {
    ColumnSet* column_set = view_state->column_set;
    View* view = view_state->view;
    DCHECK(view);
    const gfx::Insets& insets = host_->GetInsets();
    int x =
        column_set->columns_[view_state->start_col]->Location() + insets.left();
    int width = column_set->GetColumnWidth(view_state->start_col,
                                           view_state->col_span);
    CalculateSize(view_state->width, view_state->h_align, &x, &width);
    int y = rows_[view_state->start_row]->Location() + insets.top();
    int height = LayoutElement::TotalSize(view_state->start_row,
                                          view_state->row_span, &rows_);
    if (view_state->v_align == BASELINE && view_state->baseline != -1) {
      y += rows_[view_state->start_row]->max_ascent() - view_state->baseline;
      height = view_state->height;
    } else {
      CalculateSize(view_state->height, view_state->v_align, &y, &height);
    }
    view->SetBounds(x, y, width, height);
  }
}

gfx::Size GridLayout::GetPreferredSize(const View* host) const {
  DCHECK(host_ == host);
  gfx::Size out;
  SizeRowsAndColumns(false, 0, 0, &out);
  out.SetSize(std::max(out.width(), minimum_size_.width()),
              std::max(out.height(), minimum_size_.height()));
  return out;
}

int GridLayout::GetPreferredHeightForWidth(const View* host, int width) const {
  DCHECK(host_ == host);
  gfx::Size pref;
  SizeRowsAndColumns(false, width, 0, &pref);
  return pref.height();
}

void GridLayout::SizeRowsAndColumns(bool layout, int width, int height,
                                    gfx::Size* pref) const {
  // Protect against clients asking for metrics during the addition of a View.
  // The View is in the hierarchy, but it will not be accounted for in the
  // layout calculations at this point, so the result will be incorrect.
  DCHECK(!adding_view_) << "GridLayout queried while adding a view.";

  // Make sure the master columns have been calculated.
  CalculateMasterColumnsIfNecessary();
  pref->SetSize(0, 0);
  if (rows_.empty())
    return;

  // Calculate the preferred width of each of the columns. Some views'
  // preferred heights are derived from their width, as such we need to
  // calculate the size of the columns first.
  for (const auto& column_set : column_sets_) {
    column_set->CalculateSize(ColumnSet::SizeCalculationType::PREFERRED);
    pref->set_width(std::max(pref->width(), column_set->LayoutWidth()));
  }
  const gfx::Insets& insets = host_->GetInsets();
  pref->set_width(pref->width() + insets.width());

  // Go over the columns again and set them all to the size we settled for.
  width = width ? width : pref->width();
  for (const auto& column_set : column_sets_) {
    // We're doing a layout, divvy up any extra space.
    column_set->Resize(width - column_set->LayoutWidth() - insets.width(),
                       honors_min_width_);
    // And reset the x coordinates.
    column_set->ResetColumnXCoordinates();
  }

  // Reset the height of each row.
  LayoutElement::ResetSizes(&rows_);

  // Do the following:
  // . If the view is aligned along it's baseline, obtain the baseline from the
  //   view and update the rows ascent/descent.
  // . Reset the remaining_height of each view state.
  // . If the width the view will be given is different than it's pref, ask
  //   for the height given the actual width.
  for (const auto& view_state : view_states_) {
    view_state->remaining_height = view_state->height;

    if (view_state->v_align == BASELINE)
      view_state->baseline = view_state->view->GetBaseline();

    if (!view_state->pref_height_fixed) {
      // If the view is given a different width than it's preferred width
      // requery for the preferred height. This is necessary as the preferred
      // height may depend upon the width.
      int actual_width = view_state->column_set->GetColumnWidth(
          view_state->start_col, view_state->col_span);
      int x = 0;  // Not used in this stage.
      CalculateSize(view_state->width, view_state->h_align, &x, &actual_width);
      if (actual_width != view_state->width) {
        // The width this view will get differs from its preferred. Some Views
        // pref height varies with its width; ask for the preferred again.
        view_state->height = view_state->view->GetHeightForWidth(actual_width);
        view_state->remaining_height = view_state->height;
      }
    }
  }

  // Update the height/ascent/descent of each row from the views.
  auto view_states_iterator = view_states_.begin();
  for (; view_states_iterator != view_states_.end() &&
      (*view_states_iterator)->row_span == 1; ++view_states_iterator) {
    ViewState* view_state = view_states_iterator->get();
    Row* row = rows_[view_state->start_row].get();
    row->AdjustSize(view_state->remaining_height);
    if (view_state->baseline != -1 &&
        view_state->baseline <= view_state->height) {
      row->AdjustSizeForBaseline(view_state->baseline,
                                 view_state->height - view_state->baseline);
    }
    view_state->remaining_height = 0;
  }

  // Distribute the height of each view with a row span > 1.
  for (; view_states_iterator != view_states_.end(); ++view_states_iterator) {
    ViewState* view_state = view_states_iterator->get();

    // Update the remaining_width from columns this view_state touches.
    UpdateRemainingHeightFromRows(view_state);

    // Distribute the remaining height.
    DistributeRemainingHeight(view_state);
  }

  // Update the location of each of the rows.
  LayoutElement::CalculateLocationsFromSize(&rows_);

  // We now know the preferred height, set it here.
  pref->set_height(rows_.back()->Location() + rows_.back()->Size() +
                   insets.height());

  if (layout && height != pref->height()) {
    // We're doing a layout, and the height differs from the preferred height,
    // divvy up the extra space.
    LayoutElement::DistributeDelta(height - pref->height(), &rows_);

    // Reset y locations.
    LayoutElement::CalculateLocationsFromSize(&rows_);
  }
}

void GridLayout::CalculateMasterColumnsIfNecessary() const {
  if (!calculated_master_columns_) {
    calculated_master_columns_ = true;
    for (const auto& column_set : column_sets_)
      column_set->CalculateMasterColumns();
  }
}

void GridLayout::AddViewState(std::unique_ptr<ViewState> view_state) {
  DCHECK(view_state->view && view_state->view->parent() == host_);
  remaining_row_span_ = std::max(remaining_row_span_, view_state->row_span);
  next_column_ += view_state->col_span;
  current_row_col_set_->AddViewState(view_state.get());
  // view_states are ordered by row_span (in ascending order).
  auto i = std::lower_bound(view_states_.begin(), view_states_.end(),
                            view_state.get(), CompareByRowSpan);
  view_states_.insert(i, std::move(view_state));
  SkipPaddingColumns();
}

void GridLayout::AddRow(std::unique_ptr<Row> row) {
  current_row_++;
  remaining_row_span_--;
  // GridLayout requires that if you add a View with a row span you use the same
  // column set for each of the rows the view lands it. This DCHECK verifies
  // that.
  DCHECK(remaining_row_span_ <= 0 || row->column_set() == nullptr ||
         row->column_set() == GetLastValidColumnSet());
  next_column_ = 0;
  current_row_col_set_ = row->column_set();
  rows_.push_back(std::move(row));
  SkipPaddingColumns();
}

void GridLayout::UpdateRemainingHeightFromRows(ViewState* view_state) const {
  view_state->remaining_height -= LayoutElement::TotalSize(
      view_state->start_row, view_state->row_span, &rows_);
}

void GridLayout::DistributeRemainingHeight(ViewState* view_state) const {
  int height = view_state->remaining_height;
  if (height <= 0)
    return;

  // Determine the number of resizable rows the view touches.
  int start_row = view_state->start_row;
  int max_row = view_state->start_row + view_state->row_span;
  const int resizable_rows =
      std::count_if(rows_.cbegin() + start_row, rows_.cbegin() + max_row,
                    [](const auto& row) { return row->IsResizable(); });

  if (resizable_rows > 0) {
    // There are resizable rows, give the remaining height to them.
    int to_distribute = height / resizable_rows;
    for (int i = start_row; i < max_row; ++i) {
      if (rows_[i]->IsResizable()) {
        height -= to_distribute;
        if (height < to_distribute) {
          // Give all slop to the last column.
          to_distribute += height;
        }
        rows_[i]->SetSize(rows_[i]->Size() + to_distribute);
      }
    }
  } else {
    // None of the rows are resizable, divvy the remaining height up equally
    // among all rows the view touches.
    int each_row_height = height / view_state->row_span;
    for (int i = start_row; i < max_row; ++i) {
      height -= each_row_height;
      if (height < each_row_height)
        each_row_height += height;
      rows_[i]->SetSize(rows_[i]->Size() + each_row_height);
    }
    view_state->remaining_height = 0;
  }
}

void GridLayout::SkipPaddingColumns() {
  if (!current_row_col_set_)
    return;
  while (next_column_ < current_row_col_set_->num_columns() &&
         current_row_col_set_->columns_[next_column_]->is_padding_) {
    next_column_++;
  }
}

ColumnSet* GridLayout::GetLastValidColumnSet() {
  const auto i =
      std::find_if(rows_.crend() - current_row_, rows_.crend(),
                   [](const auto& row) { return row->column_set(); });
  return (i == rows_.crend()) ? nullptr : (*i)->column_set();
}

}  // namespace views
