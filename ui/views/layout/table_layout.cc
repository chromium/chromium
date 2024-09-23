// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/table_layout.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {

namespace {

// A LayoutElement has a size and location along one axis. It contains methods
// that are used along both axes.
class LayoutElement {
 public:
  explicit LayoutElement(float resize) : resize_(resize) {
    DCHECK_GE(resize, 0) << "Can't give a row or column negative resize";
  }
  LayoutElement(const LayoutElement&) = default;
  LayoutElement(LayoutElement&&) = default;
  LayoutElement& operator=(const LayoutElement&) = default;
  LayoutElement& operator=(LayoutElement&&) = default;
  virtual ~LayoutElement() = default;

  // Sets the size of this element given a preferred `size`.
  virtual void AdjustSize(int size) { size_ = std::max(size_, size); }

  // Resets the size to the initial size.
  virtual void ResetSize() = 0;

  // Sets the location of each element to be the sum of the sizes of the
  // preceding elements.
  template <class T>
  static void CalculateLocationsFromSize(std::vector<T>& elements) {
    int location = 0;
    for (auto& element : elements) {
      element.location_ = location;
      location += element.size();
    }
  }

  float resize() const { return resize_; }
  bool resizable() const { return resize_ > 0; }
  int location() const { return location_; }
  void set_size(int size) { size_ = size; }
  int size() const { return size_; }

 private:
  float resize_;
  int location_ = 0;
  int size_ = 0;
};

// Invokes ResetSize on all the layout elements.
template <class T>
void ResetSizes(std::vector<T>& elements) {
  for (auto& element : elements)
    element.ResetSize();
}

// Distributes delta among the resizable elements. Each resizable element is
// given (resize() / total_resize * delta) DIP of extra space.
template <class T>
void DistributeDelta(int delta, std::vector<T>& elements) {
  if (delta == 0)
    return;

  float total_resize = 0;
  int resize_count = 0;
  for (auto& element : elements) {
    total_resize += element.resize();
    if (element.resize() > 0)
      ++resize_count;
  }
  if (total_resize == 0)
    return;
  int remaining_delta = delta;
  for (auto& element : elements) {
    if (element.resize() > 0) {
      int element_delta = remaining_delta;
      if (--resize_count != 0) {
        element_delta =
            base::ClampFloor(delta * (element.resize() / total_resize));
        remaining_delta -= element_delta;
      }
      element.set_size(element.size() + element_delta);
    }
  }
}

// Returns the sum of the size of the elements from `start` to
// `start` + `length`.
template <class T>
int TotalSize(size_t start, size_t length, const std::vector<T>& elements) {
  DCHECK_GT(length, 0u);
  DCHECK_LE(start + length, elements.size());
  const auto begin = elements.cbegin() + static_cast<ptrdiff_t>(start);
  return std::accumulate(
      begin, begin + static_cast<ptrdiff_t>(length), 0,
      [](int size, const auto& elem) { return size + elem.size(); });
}

// Advances `index` past any padding elements.
template <class T>
void SkipPadding(size_t& index, const std::vector<T>& elements) {
  while (index < elements.size() && elements[index].is_padding())
    ++index;
}

void CalculateLocationAndSize(int pref_size,
                              LayoutAlignment alignment,
                              int* location,
                              int* size) {
  if (alignment != LayoutAlignment::kStretch) {
    int available_size = *size;
    *size = std::min(*size, pref_size);
    switch (alignment) {
      case LayoutAlignment::kStart:
        // Nothing to do, location already points to start.
        break;
      case LayoutAlignment::kBaseline:  // If we were asked to align on
                                        // baseline, but the view doesn't have a
                                        // baseline, fall back to center.
      case LayoutAlignment::kCenter:
        *location += (available_size - *size) / 2;
        break;
      case LayoutAlignment::kEnd:
        *location = *location + available_size - *size;
        break;
      default:
        NOTREACHED();
    }
  }
}

}  // namespace

constexpr float TableLayout::kFixedSize;

// As the name implies, this represents a Column. Column contains default values
// for views originating in this column.
class TableLayout::Column : public LayoutElement {
 public:
  Column(LayoutAlignment h_align,
         LayoutAlignment v_align,
         float horizontal_resize,
         ColumnSize size_type,
         int fixed_width,
         int min_width,
         bool is_padding)
      : LayoutElement(horizontal_resize),
        h_align_(h_align),
        v_align_(v_align),
        size_type_(size_type),
        fixed_width_(fixed_width),
        min_width_(min_width),
        is_padding_(is_padding) {}
  Column(const Column&) = default;
  Column(Column&&) = default;
  Column& operator=(const Column&) = default;
  Column& operator=(Column&&) = default;
  ~Column() override = default;

  void AdjustSize(int size) override {
    if (size_type_ == ColumnSize::kUsePreferred)
      LayoutElement::AdjustSize(size);
  }

  void ResetSize() override {
    set_size((size_type_ == ColumnSize::kFixed) ? fixed_width_ : min_width_);
  }

  // Determines the max size of all linked columns, and sets each column to that
  // size.
  void UnifyLinkedColumnSizes(const std::optional<int>& size_limit) {
    if (linked_columns_.empty() || linked_columns_.front() != this)
      return;

    // Accumulate the size first.
    int size = 0;
    for (views::TableLayout::Column* column : linked_columns_) {
      if (!size_limit || column->size() <= *size_limit)
        size = std::max(size, column->size());
    }

    // Then apply it.
    for (views::TableLayout::Column* column : linked_columns_) {
      column->set_size(std::max(size, column->size()));
    }
  }

  void set_linked_columns(
      const std::vector<raw_ptr<Column, VectorExperimental>>& linked_columns) {
    DCHECK(linked_columns_.empty()) << "Cannot link a column twice";
    linked_columns_ = linked_columns;
  }

  LayoutAlignment h_align() const { return h_align_; }
  LayoutAlignment v_align() const { return v_align_; }
  ColumnSize size_type() const { return size_type_; }
  int min_width() const { return min_width_; }
  bool is_padding() const { return is_padding_; }

 private:
  LayoutAlignment h_align_;
  LayoutAlignment v_align_;
  ColumnSize size_type_;
  int fixed_width_;
  int min_width_;
  bool is_padding_;
  std::vector<raw_ptr<Column, VectorExperimental>> linked_columns_;
};

class TableLayout::Row : public LayoutElement {
 public:
  Row(float vertical_resize, int height, bool is_padding)
      : LayoutElement(vertical_resize),
        height_(height),
        is_padding_(is_padding) {}
  Row(const Row&) = default;
  Row(Row&&) = default;
  Row& operator=(const Row&) = default;
  Row& operator=(Row&&) = default;
  ~Row() override = default;

  void ResetSize() override {
    max_ascent_ = max_descent_ = 0;
    set_size(height_);
  }

  // Adjusts the size to accommodate the specified `ascent`/`descent`.
  void AdjustSizeForBaseline(int ascent, int descent) {
    max_ascent_ = std::max(ascent, max_ascent_);
    max_descent_ = std::max(descent, max_descent_);
    AdjustSize(max_ascent_ + max_descent_);
  }

  bool is_padding() const { return is_padding_; }
  int max_ascent() const { return max_ascent_; }

 private:
  int height_;
  bool is_padding_;
  int max_ascent_ = 0;
  int max_descent_ = 0;
};

// Identifies the location in the grid of a particular view, along with
// placement information and size information.
struct TableLayout::ViewState {
  ViewState() = default;
  ViewState(View* view,
            size_t start_col,
            size_t start_row,
            size_t col_span,
            size_t row_span,
            LayoutAlignment h_align,
            LayoutAlignment v_align)
      : view(view),
        start_col(start_col),
        start_row(start_row),
        col_span(col_span),
        row_span(row_span),
        h_align(h_align),
        v_align(v_align) {
    DCHECK(view);
    DCHECK_GT(col_span, 0u);
    DCHECK_GT(row_span, 0u);
  }

  raw_ptr<View, DanglingUntriaged> view = nullptr;
  size_t start_col = 0;
  size_t start_row = 0;
  size_t col_span = 0;
  size_t row_span = 0;
  LayoutAlignment h_align = LayoutAlignment::kStart;
  LayoutAlignment v_align = LayoutAlignment::kStart;

  // The preferred size, only set during the preferred size pass
  // (SizeCalculationType::kPreferred).
  gfx::Size pref_size;

  // The width/height. This is either the preferred width or the minimum width
  // depending on the pass.
  int width = 0;
  int height = 0;

  // Used during layout. Gives how much width/height has not yet been
  // distributed to the columns/rows the view is in.
  int remaining_width = 0;
  int remaining_height = 0;

  // The baseline. Only used if the view is vertically aligned along the
  // baseline.
  std::optional<int> baseline;
};

TableLayout::TableLayout() = default;

TableLayout::~TableLayout() = default;

TableLayout& TableLayout::AddColumn(LayoutAlignment h_align,
                                    LayoutAlignment v_align,
                                    float horizontal_resize,
                                    ColumnSize size_type,
                                    int fixed_width,
                                    int min_width) {
  columns_.emplace_back(h_align, v_align, horizontal_resize, size_type,
                        fixed_width, min_width, false);
  return *this;
}

TableLayout& TableLayout::AddPaddingColumn(float horizontal_resize, int width) {
  columns_.emplace_back(LayoutAlignment::kStretch, LayoutAlignment::kStretch,
                        horizontal_resize, ColumnSize::kFixed, width, width,
                        true);
  return *this;
}

TableLayout& TableLayout::AddRows(size_t n, float vertical_resize, int height) {
  for (size_t i = 0; i < n; ++i)
    rows_.emplace_back(vertical_resize, height, false);
  return *this;
}

TableLayout& TableLayout::AddPaddingRow(float vertical_resize, int height) {
  rows_.emplace_back(vertical_resize, height, true);
  return *this;
}

TableLayout& TableLayout::LinkColumnSizes(std::vector<size_t> columns) {
  if (columns.size() > 1) {
    base::ranges::sort(columns);
    DCHECK_LT(columns.back(), columns_.size())
        << "Cannot link an unspecified column";

    std::vector<raw_ptr<Column, VectorExperimental>> linked_columns;
    base::ranges::transform(columns, std::back_inserter(linked_columns),
                            [&](size_t index) { return &columns_[index]; });

    for (views::TableLayout::Column* column : linked_columns) {
      column->set_linked_columns(linked_columns);
    }
  }

  return *this;
}

TableLayout& TableLayout::SetLinkedColumnSizeLimit(int size_limit) {
  linked_column_size_limit_ = size_limit;
  OnLayoutChanged();
  return *this;
}

TableLayout& TableLayout::SetMinimumSize(const gfx::Size& size) {
  minimum_size_ = size;
  OnLayoutChanged();
  return *this;
}

ProposedLayout TableLayout::CalculateProposedLayout(
    const SizeBounds& size_bounds) const {
  ProposedLayout layout;
  layout.host_size = SizeRowsAndColumns(size_bounds);
  layout.host_size.SetToMax(minimum_size_);

  for (View* child : GetChildViewsInPaintOrder(host_view())) {
    if (!child->GetProperty(kViewIgnoredByLayoutKey)) {
      layout.child_layouts.push_back({child, true, {}, {}});
    }
  }

  // Size each view.
  for (const auto& view_state : view_states_by_row_span_) {
    View* view = view_state->view;
    DCHECK(view);
    const gfx::Insets& insets = host_view()->GetInsets();
    int x = columns_[view_state->start_col].location() + insets.left();
    int width =
        TotalSize(view_state->start_col, view_state->col_span, columns_);
    CalculateLocationAndSize(view_state->width, view_state->h_align, &x,
                             &width);
    int y = rows_[view_state->start_row].location() + insets.top();
    int height = TotalSize(view_state->start_row, view_state->row_span, rows_);
    if (view_state->v_align == LayoutAlignment::kBaseline &&
        view_state->baseline) {
      y += rows_[view_state->start_row].max_ascent() - *view_state->baseline;
      height = view_state->height;
    } else {
      CalculateLocationAndSize(view_state->height, view_state->v_align, &y,
                               &height);
    }

    auto it = base::ranges::find(layout.child_layouts, view,
                                 &ChildLayout::child_view);
    DCHECK(it != layout.child_layouts.cend());
    it->bounds = gfx::Rect(x, y, width, height);
    it->available_size = SizeBounds(width, height);
  }

  return layout;
}

void TableLayout::SetViewStates() const {
  view_states_by_row_span_.clear();
  view_states_by_col_span_.clear();

  size_t col = 0, row = 0;
  std::vector<ViewState*> row_spans;
  for (View* child : GetChildViewsInPaintOrder(host_view())) {
    if (!IsChildIncludedInLayout(child)) {
      continue;
    }

    // Move (col, row) to next open cell.
    for (; row < rows_.size(); ++row) {
      SkipPadding(row, rows_);
      SkipPadding(col, columns_);
      for (auto it = row_spans.begin(); it != row_spans.end();) {
        if (col < (*it)->start_col)
          break;
        const size_t last_row_of_span = (*it)->start_row + (*it)->row_span - 1;
        if (row <= last_row_of_span)
          col = std::max(col, (*it)->start_col + (*it)->col_span);
        if (row >= last_row_of_span)
          it = row_spans.erase(it);
        else
          ++it;
        SkipPadding(col, columns_);
      }
      if (col < columns_.size())
        break;
      col = 0;
    }
    CHECK_LT(row, rows_.size())
        << "There're not enough cells for layout. Did you forget to "
           "call AddRows()?";

    // Construct a ViewState for this `child`.
    const gfx::Size* span = child->GetProperty(kTableColAndRowSpanKey);
    const size_t col_span = span ? static_cast<size_t>(span->width()) : 1;
    const size_t row_span = span ? static_cast<size_t>(span->height()) : 1;
    LayoutAlignment* const child_h_align =
        child->GetProperty(kTableHorizAlignKey);
    const LayoutAlignment h_align =
        child_h_align ? *child_h_align : columns_[col].h_align();
    LayoutAlignment* const child_v_align =
        child->GetProperty(kTableVertAlignKey);
    const LayoutAlignment v_align =
        child_v_align ? *child_v_align : columns_[col].v_align();
    auto view_state = std::make_unique<ViewState>(child, col, row, col_span,
                                                  row_span, h_align, v_align);

    // Add `view_state` to the relevant vectors.
    ViewState* ptr;
    {
      auto it = base::ranges::lower_bound(view_states_by_row_span_,
                                          view_state->row_span, std::less<>(),
                                          &ViewState::row_span);
      ptr = view_states_by_row_span_.insert(it, std::move(view_state))->get();
    }
    {
      auto it =
          base::ranges::lower_bound(view_states_by_col_span_, ptr->col_span,
                                    std::less<>(), &ViewState::col_span);
      view_states_by_col_span_.insert(it, ptr);
    }
    if (ptr->row_span > 1) {
      DCHECK_LE(row + ptr->row_span, rows_.size())
          << "row_span extends past trailing edge";
      auto it = base::ranges::lower_bound(row_spans, ptr->start_col,
                                          std::less<>(), &ViewState::start_col);
      row_spans.insert(it, ptr);
    }

    // Move past the end of this child, to prepare for the next loop iteration.
    col += ptr->col_span;
    DCHECK_LE(col, columns_.size()) << "col_span extends past trailing edge";
  }
}

gfx::Size TableLayout::SizeRowsAndColumns(const SizeBounds& bounds) const {
  SetViewStates();

  gfx::Size pref;
  if (rows_.empty())
    return pref;

  // Calculate the preferred width of each of the columns. Some views'
  // preferred heights are derived from their width, as such we need to
  // calculate the size of the columns first.
  CalculateSize(SizeCalculationType::kPreferred, view_states_by_col_span_);
  const gfx::Insets& insets = host_view()->GetInsets();
  pref.set_width(LayoutWidth() + insets.width());

  // Go over the columns again and set them all to the size we settled for.
  const int bounded_width =
      bounds.width().is_bounded() ? bounds.width().value() : pref.width();
  Resize(bounded_width - pref.width());
  LayoutElement::CalculateLocationsFromSize(columns_);

  // Reset the height of each row.
  ResetSizes(rows_);

  for (auto& view_state : view_states_by_row_span_) {
    if (view_state->v_align == LayoutAlignment::kBaseline)
      view_state->baseline = view_state->view->GetBaseline();

    // If the view is given a different width than its preferred width, requery
    // for the preferred height. This is necessary as the preferred height may
    // depend upon the width.
    int actual_width =
        TotalSize(view_state->start_col, view_state->col_span, columns_);
    int x = 0;  // Not used in this stage.
    CalculateLocationAndSize(view_state->width, view_state->h_align, &x,
                             &actual_width);
    if (actual_width != view_state->width)
      view_state->height = view_state->view->GetHeightForWidth(actual_width);

    view_state->remaining_height = view_state->height;
  }

  // Update the height/ascent/descent of each row from the views.
  auto view_states_iterator = view_states_by_row_span_.begin();
  for (; view_states_iterator != view_states_by_row_span_.end() &&
         (*view_states_iterator)->row_span == 1;
       ++view_states_iterator) {
    auto& view_state = *view_states_iterator;
    Row& row = rows_[view_state->start_row];
    row.AdjustSize(view_state->remaining_height);
    if (view_state->baseline.has_value() &&
        *view_state->baseline <= view_state->height) {
      row.AdjustSizeForBaseline(*view_state->baseline,
                                view_state->height - *view_state->baseline);
    }
    view_state->remaining_height = 0;
  }

  // Distribute the height of each view with a row_span > 1.
  for (; view_states_iterator != view_states_by_row_span_.end();
       ++view_states_iterator) {
    auto& view_state = *view_states_iterator;
    view_state->remaining_height -=
        TotalSize(view_state->start_row, view_state->row_span, rows_);
    DistributeRemainingHeight(*view_state);
  }

  // Update the location of each of the rows.
  LayoutElement::CalculateLocationsFromSize(rows_);

  // We now know the preferred height, set it here.
  pref.set_height(rows_.back().location() + rows_.back().size() +
                  insets.height());

  if (bounds.height().is_bounded() && bounds.height() != pref.height()) {
    // Divvy up the extra space.
    DistributeDelta(bounds.height().value() - pref.height(), rows_);

    // Reset y locations.
    LayoutElement::CalculateLocationsFromSize(rows_);
  }

  return pref;
}

void TableLayout::DistributeRemainingHeight(ViewState& view_state) const {
  // Given the set S of rows in (view_state.start_row, view_state.row_span):
  //   If any member of S is resizable,
  //     space is distributed between the resizable members of S
  //   Otherwise, space is distributed between all members of S
  if (view_state.remaining_height <= 0) {
    return;
  }

  // Determine the number of resizable rows the view touches.
  const base::span<Row> rows_to_resize =
      base::span(rows_).subspan(view_state.start_row, view_state.row_span);
  const auto resizable_rows = static_cast<size_t>(
      base::ranges::count_if(rows_to_resize, &Row::resizable));
  size_t remaining_rows =
      resizable_rows ? resizable_rows : rows_to_resize.size();
  for (Row& row : rows_to_resize) {
    if (!resizable_rows || row.resizable()) {
      // We have to recompute the delta each pass through the loop, rather than
      // computing it up front. Although this math appears equivalent to giving
      // each view an equal share of the initial remaining height, if we did do
      // that, we'd end up with a rounding error. Recomputing the delta like
      // this avoids accumulating that rounding error. For example, if we have
      // n=4 rows and h=22 height to distribute:
      //   delta = ClampRound(22 / 4) = 6 -> h = 16, d = 3
      //   delta = ClampRound(16 / 3) = 5 -> h = 11, d = 2
      //   delta = ClampRound(11 / 2) = 6 -> h = 5, d = 1
      //   delta = ClampRound(5 / 1) = 5 -> h = 0, d = 0
      // which is an optimal distribution; if we instead computed the delta
      // upfront as ClampRound(22 / 4) = 5, we'd end up with d = 2 at the end,
      // and have to either leave a rounding error or stick that leftover into
      // the last row.
      const int delta = base::ClampRound(
          static_cast<float>(view_state.remaining_height) / remaining_rows);
      row.set_size(row.size() + delta);
      view_state.remaining_height -= delta;
      --remaining_rows;
    }
  }
}

void TableLayout::UnifyLinkedColumnSizes() const {
  for (auto& column : columns_)
    column.UnifyLinkedColumnSizes(linked_column_size_limit_);
}

void TableLayout::DistributeRemainingWidth(ViewState& view_state) const {
  // This is nearly the same as DistributeRemainingHeight(), but not identical.
  // Rows have two states: resizable, or not. Columns have three: resizable,
  // kUsePreferred, or not resizable. This results in slightly different
  // handling for distributing unaccounted size.
  int width = view_state.remaining_width;
  if (width <= 0)
    return;

  // Determine which columns are resizable, and which have a size type of
  // kUsePreferred.
  size_t resizable_columns = 0;
  size_t pref_size_columns = 0;
  size_t start_col = view_state.start_col;
  size_t max_col = view_state.start_col + view_state.col_span;
  float total_resize = 0;
  for (size_t i = start_col; i < max_col; ++i) {
    if (columns_[i].resizable()) {
      total_resize += columns_[i].resize();
      ++resizable_columns;
    } else if (columns_[i].size_type() == ColumnSize::kUsePreferred) {
      ++pref_size_columns;
    }
  }

  if (resizable_columns > 0) {
    // There are resizable columns, give them the remaining width. The extra
    // width is distributed using the resize values of each column.
    int remaining_width = width;
    for (size_t i = start_col, resize_i = 0; i < max_col; ++i) {
      if (columns_[i].resizable()) {
        ++resize_i;
        const int column_delta =
            (resize_i == resizable_columns)
                ? remaining_width
                : base::ClampFloor(width * columns_[i].resize() / total_resize);
        remaining_width -= column_delta;
        columns_[i].set_size(columns_[i].size() + column_delta);
      }
    }
  } else if (pref_size_columns > 0) {
    // None of the columns are resizable, distribute the width among those
    // that use the preferred size.
    int column_delta = width / static_cast<int>(pref_size_columns);
    for (size_t i = start_col; i < max_col; ++i) {
      if (columns_[i].size_type() == ColumnSize::kUsePreferred) {
        width -= column_delta;
        // If there is slop, we're on the last row; give it all the slop.
        if (width < column_delta)
          column_delta += width;
        columns_[i].set_size(columns_[i].size() + column_delta);
      }
    }
  }
}

int TableLayout::LayoutWidth() const {
  return std::accumulate(
      columns_.cbegin(), columns_.cend(), 0,
      [](int size, const auto& elem) { return size + elem.size(); });
}

void TableLayout::CalculateSize(
    SizeCalculationType type,
    const std::vector<raw_ptr<ViewState, VectorExperimental>>& view_states)
    const {
  // Reset the size and remaining sizes.
  for (views::TableLayout::ViewState* view_state : view_states) {
    gfx::Size size;
    if (type == SizeCalculationType::kMinimum && CanUseMinimum(*view_state)) {
      // If the min size is bigger than the preferred, use the preferred.
      // This relies on MINIMUM being calculated immediately after PREFERRED,
      // which the rest of this code relies on as well.
      size = view_state->view->GetMinimumSize();
      if (size.width() > view_state->width)
        size.set_width(view_state->width);
      if (size.height() > view_state->height)
        size.set_height(view_state->height);
    } else {
      size = view_state->view->GetPreferredSize({/* Unbounded */});
      view_state->pref_size = size;
    }
    view_state->remaining_width = view_state->width = size.width();
    view_state->remaining_height = view_state->height = size.height();
  }

  ResetSizes(columns_);

  // Distribute the size of each view with a col span == 1.
  auto view_state_iterator = view_states.begin();
  for (; view_state_iterator != view_states.end() &&
         (*view_state_iterator)->col_span == 1;
       ++view_state_iterator) {
    ViewState* view_state = *view_state_iterator;
    Column& column = columns_[view_state->start_col];
    column.AdjustSize(view_state->width);
    view_state->remaining_width -= column.size();
  }

  // Make sure all linked columns have the same size.
  UnifyLinkedColumnSizes();

  // Distribute the size of each view with a column span > 1.
  for (; view_state_iterator != view_states.end(); ++view_state_iterator) {
    ViewState* view_state = *view_state_iterator;

    // Update the remaining_width from columns this view_state touches.
    view_state->remaining_width -=
        TotalSize(view_state->start_col, view_state->col_span, columns_);

    // Distribute the remaining width.
    DistributeRemainingWidth(*view_state);

    // Update the size of linked columns.
    // This may need to be combined with previous step.
    UnifyLinkedColumnSizes();
  }
}

void TableLayout::Resize(int delta) const {
  if (delta < 0) {
    // DistributeDelta() assumes resizable columns can equally be shrunk. That
    // isn't desired when given a size smaller than the prefered. Instead the
    // columns need to be resized but bounded by the minimum. ResizeUsingMin()
    // does this.
    ResizeUsingMin(delta);
  } else {
    DistributeDelta(delta, columns_);
  }
}

void TableLayout::ResizeUsingMin(int total_delta) const {
  struct ColumnMinResizeData {
    // The column being resized.
    raw_ptr<Column> column;

    // The remaining amount of space available (the difference between the
    // preferred and minimum).
    int available = 0;

    // How much to shrink the preferred by.
    int delta = 0;
  };

  DCHECK_LE(total_delta, 0);

  // |total_delta| is negative, but easier to do operations when positive.
  total_delta = std::abs(total_delta);

  std::vector<int> preferred_column_sizes(columns_.size());
  for (size_t i = 0; i < columns_.size(); ++i)
    preferred_column_sizes[i] = columns_[i].size();

  // Recalculate the sizes using the min.  We don't want to touch the proposed
  // widths and heights, so copy the ViewStates to a temporary location so
  // modifications to them aren't reflected in the members.
  const size_t num_states = view_states_by_col_span_.size();
  std::vector<ViewState> view_states(num_states);
  std::vector<raw_ptr<ViewState, VectorExperimental>> view_state_ptrs(
      num_states);
  for (size_t i = 0; i < num_states; ++i) {
    view_states[i] = *view_states_by_col_span_[i];
    view_state_ptrs[i] = &view_states[i];
  }
  CalculateSize(SizeCalculationType::kMinimum, view_state_ptrs);

  // Build up the set of columns that can be shrunk in |resize_data|, this
  // iteration also resets the size of the column back to the preferred size.
  std::vector<ColumnMinResizeData> resize_data;
  float total_resize = 0;
  for (size_t i = 0; i < columns_.size(); ++i) {
    Column& column = columns_[i];
    const int available =
        std::max(0, preferred_column_sizes[i] -
                        std::max(column.min_width(), column.size()));
    DCHECK_GE(available, 0);
    // Set the size back to preferred. We'll reset the size if necessary later.
    column.set_size(preferred_column_sizes[i]);
    if (!column.resizable() || available == 0)
      continue;
    resize_data.push_back({&column, available, 0});
    total_resize += column.resize();
  }
  if (resize_data.empty())
    return;

  // Loop through the columns updating the amount available and the amount to
  // resize. This may take multiple iterations if the column min is hit.
  // Generally there are not that many columns in a table, so this code is
  // not optimized. Any time the column hits the min it is removed from
  // |resize_data|.
  while (!resize_data.empty() && total_delta > 0) {
    float next_iteration_total_resize = total_resize;
    int next_iteration_delta = total_delta;
    for (size_t i = resize_data.size(); i > 0; --i) {
      ColumnMinResizeData& data = resize_data[i - 1];
      int delta = std::min(
          data.available,
          base::ClampFloor(total_delta * data.column->resize() / total_resize));
      // Make sure at least one column is resized (rounding errors may prevent
      // that).
      if (i == 1 && delta == 0 && next_iteration_delta == total_delta)
        delta = 1;
      next_iteration_delta -= delta;
      data.delta += delta;
      data.available -= delta;
      if (data.available == 0) {
        data.column->set_size(data.column->size() - data.delta);
        next_iteration_total_resize -= data.column->resize();
        resize_data.erase(resize_data.begin() + static_cast<ptrdiff_t>(i - 1));
      }
    }
    DCHECK_LT(next_iteration_delta, total_delta);
    total_delta = next_iteration_delta;
    total_resize = next_iteration_total_resize;
  }

  for (const ColumnMinResizeData& data : resize_data)
    data.column->set_size(data.column->size() - data.delta);
}

bool TableLayout::CanUseMinimum(const ViewState& view_state) const {
  const auto begin =
      columns_.cbegin() + static_cast<ptrdiff_t>(view_state.start_col);
  return std::any_of(begin, begin + static_cast<ptrdiff_t>(view_state.col_span),
                     [](const auto& col) {
                       return col.resizable() &&
                              col.size_type() != ColumnSize::kFixed;
                     });
}

}  // namespace views
