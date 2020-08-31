// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_view.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_utils.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_header.h"
#include "ui/views/controls/table/table_utils.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"

namespace views {

namespace {

constexpr int kGroupingIndicatorSize = 6;

// Returns result, unless ascending is false in which case -result is returned.
int SwapCompareResult(int result, bool ascending) {
  return ascending ? result : -result;
}

// Populates |model_index_to_range_start| based on the |grouper|.
void GetModelIndexToRangeStart(TableGrouper* grouper,
                               int row_count,
                               std::map<int, int>* model_index_to_range_start) {
  for (int model_index = 0; model_index < row_count;) {
    GroupRange range;
    grouper->GetGroupRange(model_index, &range);
    DCHECK_GT(range.length, 0);
    for (int i = model_index; i < model_index + range.length; ++i)
      (*model_index_to_range_start)[i] = model_index;
    model_index += range.length;
  }
}

// Returns the color id for the background of selected text. |has_focus|
// indicates if the table has focus.
ui::NativeTheme::ColorId text_background_color_id(bool has_focus) {
  return has_focus
             ? ui::NativeTheme::kColorId_TableSelectionBackgroundFocused
             : ui::NativeTheme::kColorId_TableSelectionBackgroundUnfocused;
}

// Returns the color id for text. |has_focus| indicates if the table has focus.
ui::NativeTheme::ColorId selected_text_color_id(bool has_focus) {
  return has_focus ? ui::NativeTheme::kColorId_TableSelectedText
                   : ui::NativeTheme::kColorId_TableSelectedTextUnfocused;
}

// Whether the platform "command" key is down.
bool IsCmdOrCtrl(const ui::Event& event) {
#if defined(OS_APPLE)
  return event.IsCommandDown();
#else
  return event.IsControlDown();
#endif
}

}  // namespace

// Used as the comparator to sort the contents of the table.
struct TableView::SortHelper {
  explicit SortHelper(TableView* table) : table(table) {}

  bool operator()(int model_index1, int model_index2) {
    return table->CompareRows(model_index1, model_index2) < 0;
  }

  TableView* table;
};

// Used as the comparator to sort the contents of the table when a TableGrouper
// is present. When groups are present we sort the groups based on the first row
// in the group and within the groups we keep the same order as the model.
struct TableView::GroupSortHelper {
  explicit GroupSortHelper(TableView* table) : table(table) {}

  bool operator()(int model_index1, int model_index2) {
    const int range1 = model_index_to_range_start[model_index1];
    const int range2 = model_index_to_range_start[model_index2];
    if (range1 == range2) {
      // The two rows are in the same group, sort so that items in the same
      // group always appear in the same order.
      return model_index1 < model_index2;
    }
    return table->CompareRows(range1, range2) < 0;
  }

  TableView* table;
  std::map<int, int> model_index_to_range_start;
};

TableView::VisibleColumn::VisibleColumn() = default;

TableView::VisibleColumn::~VisibleColumn() = default;

TableView::PaintRegion::PaintRegion() = default;

TableView::PaintRegion::~PaintRegion() = default;

class TableView::HighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  HighlightPathGenerator() = default;

  // HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell)
      return SkPath();

    const TableView* const table = static_cast<const TableView*>(view);
    // If there's no focus indicator fall back on the default highlight path
    // (highlights entire view instead of active cell).
    if (!table->GetHasFocusIndicator())
      return SkPath();

    // Draw a focus indicator around the active cell.
    gfx::Rect bounds = table->GetActiveCellBounds();
    bounds.set_x(table->GetMirroredXForRect(bounds));
    return SkPath().addRect(gfx::RectToSkRect(bounds));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HighlightPathGenerator);
};

TableView::TableView() {
  constexpr int kTextContext = style::CONTEXT_TABLE_ROW;
  constexpr int kTextStyle = style::STYLE_PRIMARY;
  font_list_ = style::GetFont(kTextContext, kTextStyle);
  row_height_ = LayoutProvider::GetControlHeightForFont(kTextContext,
                                                        kTextStyle, font_list_);

  // Always focusable, even on Mac (consistent with NSTableView).
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::HighlightPathGenerator::Install(
      this, std::make_unique<TableView::HighlightPathGenerator>());

  focus_ring_ = FocusRing::Install(this);
}

TableView::TableView(ui::TableModel* model,
                     const std::vector<ui::TableColumn>& columns,
                     TableTypes table_type,
                     bool single_selection)
    : TableView() {
  Init(model, std::move(columns), table_type, single_selection);
}

TableView::~TableView() {
  if (model_)
    model_->SetObserver(nullptr);
}

// static
std::unique_ptr<ScrollView> TableView::CreateScrollViewWithTable(
    std::unique_ptr<TableView> table) {
  auto scroll_view = ScrollView::CreateScrollViewWithBorder();
  auto* table_ptr = table.get();
  scroll_view->SetContents(std::move(table));
  table_ptr->CreateHeaderIfNecessary(scroll_view.get());
  return scroll_view;
}

void TableView::Init(ui::TableModel* model,
                     const std::vector<ui::TableColumn>& columns,
                     TableTypes table_type,
                     bool single_selection) {
  columns_ = columns;
  table_type_ = table_type;
  single_selection_ = single_selection;

  for (const auto& column : columns) {
    VisibleColumn visible_column;
    visible_column.column = column;
    visible_columns_.push_back(visible_column);
  }

  SetModel(model);
  if (model_)
    UpdateVirtualAccessibilityChildren();
}

// TODO(sky): this doesn't support arbitrarily changing the model, rename this
// to ClearModel() or something.
void TableView::SetModel(ui::TableModel* model) {
  if (model == model_)
    return;

  if (model_)
    model_->SetObserver(nullptr);
  model_ = model;
  selection_model_.Clear();
  if (model_)
    model_->SetObserver(this);
}

void TableView::SetGrouper(TableGrouper* grouper) {
  grouper_ = grouper;
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
}

int TableView::GetRowCount() const {
  return model_ ? model_->RowCount() : 0;
}

void TableView::Select(int model_row) {
  if (!model_)
    return;

  SelectByViewIndex(model_row == -1 ? -1 : ModelToView(model_row));
}

int TableView::GetFirstSelectedRow() const {
  return selection_model_.empty() ? -1 : selection_model_.selected_indices()[0];
}

void TableView::SetColumnVisibility(int id, bool is_visible) {
  if (is_visible == IsColumnVisible(id))
    return;

  if (is_visible) {
    VisibleColumn visible_column;
    visible_column.column = FindColumnByID(id);
    visible_columns_.push_back(visible_column);
  } else {
    const auto i = std::find_if(
        visible_columns_.begin(), visible_columns_.end(),
        [id](const auto& column) { return column.column.id == id; });
    if (i != visible_columns_.end()) {
      visible_columns_.erase(i);
      if (active_visible_column_index_ >= int{visible_columns_.size()})
        SetActiveVisibleColumnIndex(int{visible_columns_.size()} - 1);
    }
  }
  ClearVirtualAccessibilityChildren();
  UpdateVisibleColumnSizes();
  PreferredSizeChanged();
  SchedulePaint();
  if (header_)
    header_->SchedulePaint();
  UpdateVirtualAccessibilityChildren();
}

void TableView::ToggleSortOrder(int visible_column_index) {
  DCHECK(visible_column_index >= 0 &&
         visible_column_index < static_cast<int>(visible_columns_.size()));
  const ui::TableColumn& column = visible_columns_[visible_column_index].column;
  if (!column.sortable)
    return;
  SortDescriptors sort(sort_descriptors_);
  if (!sort.empty() && sort[0].column_id == column.id) {
    if (sort[0].ascending == column.initial_sort_is_ascending) {
      // First toggle inverts the order.
      sort[0].ascending = !sort[0].ascending;
    } else {
      // Second toggle clears the sort.
      sort.clear();
    }
  } else {
    SortDescriptor descriptor(column.id, column.initial_sort_is_ascending);
    sort.insert(sort.begin(), descriptor);
    // Only persist two sort descriptors.
    if (sort.size() > 2)
      sort.resize(2);
  }
  SetSortDescriptors(sort);
}

void TableView::SetSortDescriptors(const SortDescriptors& sort_descriptors) {
  sort_descriptors_ = sort_descriptors;
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
  if (header_)
    header_->SchedulePaint();
}

bool TableView::IsColumnVisible(int id) const {
  const auto ids_match = [id](const auto& column) {
    return column.column.id == id;
  };
  return std::any_of(visible_columns_.cbegin(), visible_columns_.cend(),
                     ids_match);
}

bool TableView::HasColumn(int id) const {
  const auto ids_match = [id](const auto& column) { return column.id == id; };
  return std::any_of(columns_.cbegin(), columns_.cend(), ids_match);
}

bool TableView::GetHasFocusIndicator() const {
  int active_row = selection_model_.active();
  return active_row != ui::ListSelectionModel::kUnselectedIndex &&
         active_visible_column_index_ !=
             ui::ListSelectionModel::kUnselectedIndex;
}

const TableView::VisibleColumn& TableView::GetVisibleColumn(int index) {
  DCHECK(index >= 0 && index < static_cast<int>(visible_columns_.size()));
  return visible_columns_[index];
}

void TableView::SetVisibleColumnWidth(int index, int width) {
  DCHECK(index >= 0 && index < static_cast<int>(visible_columns_.size()));
  if (visible_columns_[index].width == width)
    return;
  base::AutoReset<bool> reseter(&in_set_visible_column_width_, true);
  visible_columns_[index].width = width;
  for (size_t i = index + 1; i < visible_columns_.size(); ++i) {
    visible_columns_[i].x =
        visible_columns_[i - 1].x + visible_columns_[i - 1].width;
  }
  PreferredSizeChanged();
  SchedulePaint();
  UpdateVirtualAccessibilityChildrenBounds();
}

int TableView::ModelToView(int model_index) const {
  if (!GetIsSorted())
    return model_index;
  DCHECK_GE(model_index, 0) << " negative model_index " << model_index;
  DCHECK_LT(model_index, GetRowCount())
      << " out of bounds model_index " << model_index;
  return model_to_view_[model_index];
}

int TableView::ViewToModel(int view_index) const {
  DCHECK_GE(view_index, 0) << " negative view_index " << view_index;
  DCHECK_LT(view_index, GetRowCount())
      << " out of bounds view_index " << view_index;
  return GetIsSorted() ? view_to_model_[view_index] : view_index;
}

bool TableView::GetSelectOnRemove() const {
  return select_on_remove_;
}

void TableView::SetSelectOnRemove(bool select_on_remove) {
  if (select_on_remove_ == select_on_remove)
    return;

  select_on_remove_ = select_on_remove;
  OnPropertyChanged(&select_on_remove_, kPropertyEffectsNone);
}

TableTypes TableView::GetTableType() const {
  return table_type_;
}

bool TableView::GetSortOnPaint() const {
  return sort_on_paint_;
}

void TableView::SetSortOnPaint(bool sort_on_paint) {
  if (sort_on_paint_ == sort_on_paint)
    return;

  sort_on_paint_ = sort_on_paint;
  OnPropertyChanged(&sort_on_paint_, kPropertyEffectsNone);
}

void TableView::Layout() {
  // When the scrollview's width changes we force recalculating column sizes.
  ScrollView* scroll_view = ScrollView::GetScrollViewForContents(this);
  if (scroll_view) {
    const int scroll_view_width = scroll_view->GetContentsBounds().width();
    if (scroll_view_width != last_parent_width_) {
      last_parent_width_ = scroll_view_width;
      if (!in_set_visible_column_width_) {
        // Layout to the parent (the Viewport), which differs from
        // |scroll_view_width| when scrollbars are present.
        layout_width_ = parent()->width();
        UpdateVisibleColumnSizes();
      }
    }
  }
  // We have to override Layout like this since we're contained in a ScrollView.
  gfx::Size pref = GetPreferredSize();
  int width = pref.width();
  int height = pref.height();
  if (parent()) {
    width = std::max(parent()->width(), width);
    height = std::max(parent()->height(), height);
  }
  SetBounds(x(), y(), width, height);
  if (header_) {
    header_->SetBoundsRect(
        gfx::Rect(header_->bounds().origin(),
                  gfx::Size(width, header_->GetPreferredSize().height())));
  }

  focus_ring_->Layout();
}

gfx::Size TableView::CalculatePreferredSize() const {
  int width = 50;
  if (header_ && !visible_columns_.empty())
    width = visible_columns_.back().x + visible_columns_.back().width;
  return gfx::Size(width, GetRowCount() * row_height_);
}

bool TableView::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void TableView::OnVisibleBoundsChanged() {
  // When our visible bounds change, we need to make sure we update the bounds
  // of our AXVirtualView children.
  UpdateVirtualAccessibilityChildrenBounds();
}

bool TableView::OnKeyPressed(const ui::KeyEvent& event) {
  if (!HasFocus())
    return false;

  switch (event.key_code()) {
    case ui::VKEY_A:
      // control-a selects all.
      if (IsCmdOrCtrl(event) && !single_selection_ && GetRowCount()) {
        ui::ListSelectionModel selection_model;
        selection_model.SetSelectedIndex(selection_model_.active());
        for (int i = 0; i < GetRowCount(); ++i)
          selection_model.AddIndexToSelection(i);
        SetSelectionModel(std::move(selection_model));
        return true;
      }
      break;

    case ui::VKEY_HOME:
      if (GetRowCount())
        SelectByViewIndex(0);
      return true;

    case ui::VKEY_END:
      if (GetRowCount())
        SelectByViewIndex(GetRowCount() - 1);
      return true;

    case ui::VKEY_UP:
#if defined(OS_APPLE)
      if (event.IsAltDown()) {
        if (GetRowCount())
          SelectByViewIndex(0);
      } else {
        AdvanceSelection(AdvanceDirection::kDecrement);
      }
#else
      AdvanceSelection(AdvanceDirection::kDecrement);
#endif
      return true;

    case ui::VKEY_DOWN:
#if defined(OS_APPLE)
      if (event.IsAltDown()) {
        if (GetRowCount())
          SelectByViewIndex(GetRowCount() - 1);
      } else {
        AdvanceSelection(AdvanceDirection::kIncrement);
      }
#else
      AdvanceSelection(AdvanceDirection::kIncrement);
#endif
      return true;

    case ui::VKEY_LEFT:
      if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
        const AdvanceDirection direction = base::i18n::IsRTL()
                                               ? AdvanceDirection::kIncrement
                                               : AdvanceDirection::kDecrement;
        if (IsCmdOrCtrl(event)) {
          if (active_visible_column_index_ != -1 && header_) {
            header_->ResizeColumnViaKeyboard(active_visible_column_index_,
                                             direction);
            focus_ring_->SchedulePaint();
          }
        } else {
          AdvanceActiveVisibleColumn(direction);
        }
        return true;
      }
      break;

    case ui::VKEY_RIGHT:
      if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
        const AdvanceDirection direction = base::i18n::IsRTL()
                                               ? AdvanceDirection::kDecrement
                                               : AdvanceDirection::kIncrement;
        if (IsCmdOrCtrl(event)) {
          if (active_visible_column_index_ != -1 && header_) {
            header_->ResizeColumnViaKeyboard(active_visible_column_index_,
                                             direction);
            focus_ring_->SchedulePaint();
          }
        } else {
          AdvanceActiveVisibleColumn(direction);
        }
        return true;
      }
      break;

    case ui::VKEY_SPACE:
      if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell &&
          active_visible_column_index_ != -1) {
        ToggleSortOrder(active_visible_column_index_);
        return true;
      }
      break;

    default:
      break;
  }

  if (observer_)
    observer_->OnKeyDown(event.key_code());
  return false;
}

bool TableView::OnMousePressed(const ui::MouseEvent& event) {
  RequestFocus();
  if (!event.IsOnlyLeftMouseButton())
    return true;

  const int row = event.y() / row_height_;
  if (row < 0 || row >= GetRowCount())
    return true;

  if (event.GetClickCount() == 2) {
    SelectByViewIndex(row);
    if (observer_)
      observer_->OnDoubleClick();
  } else if (event.GetClickCount() == 1) {
    ui::ListSelectionModel new_model;
    ConfigureSelectionModelForEvent(event, &new_model);
    SetSelectionModel(std::move(new_model));
  }

  return true;
}

void TableView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP_DOWN)
    return;

  RequestFocus();

  const int row = event->y() / row_height_;
  if (row < 0 || row >= GetRowCount())
    return;

  event->StopPropagation();
  ui::ListSelectionModel new_model;
  ConfigureSelectionModelForEvent(*event, &new_model);
  SetSelectionModel(std::move(new_model));
}

base::string16 TableView::GetTooltipText(const gfx::Point& p) const {
  const int row = p.y() / row_height_;
  if (row < 0 || row >= GetRowCount() || visible_columns_.empty())
    return base::string16();

  const int x = GetMirroredXInView(p.x());
  const int column = GetClosestVisibleColumnIndex(this, x);
  if (x < visible_columns_[column].x ||
      x > (visible_columns_[column].x + visible_columns_[column].width))
    return base::string16();

  const int model_row = ViewToModel(row);
  if (column == 0 && !model_->GetTooltip(model_row).empty())
    return model_->GetTooltip(model_row);
  return model_->GetText(model_row, visible_columns_[column].column.id);
}

void TableView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // ID, class name and relative bounds are added by ViewAccessibility for all
  // non-virtual views, so we don't need to add them here.
  node_data->role = ax::mojom::Role::kListGrid;
  node_data->SetRestriction(ax::mojom::Restriction::kReadOnly);
  node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kActivate);
  // Subclasses should overwrite the name with the control's associated label.
  node_data->SetNameExplicitlyEmpty();

  node_data->AddIntAttribute(ax::mojom::IntAttribute::kTableRowCount,
                             static_cast<int32_t>(GetRowCount()));
  node_data->AddIntAttribute(ax::mojom::IntAttribute::kTableColumnCount,
                             static_cast<int32_t>(visible_columns_.size()));
}

bool TableView::HandleAccessibleAction(const ui::AXActionData& action_data) {
  if (!GetRowCount())
    return false;

  int active_row = selection_model_.active();
  if (active_row == ui::ListSelectionModel::kUnselectedIndex)
    active_row = ModelToView(0);

  switch (action_data.action) {
    case ax::mojom::Action::kDoDefault:
      RequestFocus();
      SelectByViewIndex(ModelToView(active_row));
      if (observer_)
        observer_->OnDoubleClick();
      break;

    case ax::mojom::Action::kFocus:
      RequestFocus();
      // Setting focus should not affect the current selection.
      if (selection_model_.empty())
        SelectByViewIndex(0);
      break;

    case ax::mojom::Action::kScrollRight: {
      const AdvanceDirection direction = base::i18n::IsRTL()
                                             ? AdvanceDirection::kDecrement
                                             : AdvanceDirection::kIncrement;
      AdvanceActiveVisibleColumn(direction);
      break;
    }

    case ax::mojom::Action::kScrollLeft: {
      const AdvanceDirection direction = base::i18n::IsRTL()
                                             ? AdvanceDirection::kIncrement
                                             : AdvanceDirection::kDecrement;
      AdvanceActiveVisibleColumn(direction);
      break;
    }

    case ax::mojom::Action::kScrollToMakeVisible:
      ScrollRectToVisible(GetRowBounds(ModelToView(active_row)));
      break;

    case ax::mojom::Action::kSetSelection:
      // TODO(nektar): Retrieve the anchor and focus nodes once AXVirtualView is
      // implemented in this class.
      SelectByViewIndex(active_row);
      break;

    case ax::mojom::Action::kShowContextMenu:
      ShowContextMenu(GetBoundsInScreen().CenterPoint(),
                      ui::MENU_SOURCE_KEYBOARD);
      break;

    default:
      return false;
  }

  return true;
}

void TableView::OnModelChanged() {
  selection_model_.Clear();
  NumRowsChanged();
}

void TableView::OnItemsChanged(int start, int length) {
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
}

void TableView::OnItemsAdded(int start, int length) {
  for (int i = 0; i < length; ++i)
    selection_model_.IncrementFrom(start);
  NumRowsChanged();
}

void TableView::OnItemsMoved(int old_start, int length, int new_start) {
  selection_model_.Move(old_start, new_start, length);
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
}

void TableView::OnItemsRemoved(int start, int length) {
  DCHECK_GE(start, 0);
  // Determine the currently selected index in terms of the view. We inline the
  // implementation here since ViewToModel() has DCHECKs that fail since the
  // model has changed but |model_to_view_| has not been updated yet.
  const int previously_selected_model_index = GetFirstSelectedRow();
  int previously_selected_view_index = previously_selected_model_index;
  if (previously_selected_model_index != -1 && GetIsSorted())
    previously_selected_view_index =
        model_to_view_[previously_selected_model_index];
  for (int i = 0; i < length; ++i)
    selection_model_.DecrementFrom(start);
  NumRowsChanged();
  // If the selection was empty and is no longer empty select the same visual
  // index.
  if (selection_model_.empty() && previously_selected_view_index != -1 &&
      GetRowCount() && select_on_remove_) {
    selection_model_.SetSelectedIndex(ViewToModel(
        std::min(GetRowCount() - 1, previously_selected_view_index)));
  }
  if (!selection_model_.empty() && selection_model_.active() == -1)
    selection_model_.set_active(GetFirstSelectedRow());
  if (!selection_model_.empty() && selection_model_.anchor() == -1)
    selection_model_.set_anchor(GetFirstSelectedRow());
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  if (observer_)
    observer_->OnSelectionChanged();
}

gfx::Point TableView::GetKeyboardContextMenuLocation() {
  int first_selected = GetFirstSelectedRow();
  gfx::Rect vis_bounds(GetVisibleBounds());
  int y = vis_bounds.height() / 2;
  if (first_selected != -1) {
    gfx::Rect cell_bounds(GetRowBounds(first_selected));
    if (cell_bounds.bottom() >= vis_bounds.y() &&
        cell_bounds.bottom() < vis_bounds.bottom()) {
      y = cell_bounds.bottom();
    }
  }
  gfx::Point screen_loc(0, y);
  if (base::i18n::IsRTL())
    screen_loc.set_x(width());
  ConvertPointToScreen(this, &screen_loc);
  return screen_loc;
}

void TableView::OnFocus() {
  SchedulePaintForSelection();
  focus_ring_->SchedulePaint();
  needs_update_accessibility_focus_ = true;
}

void TableView::OnBlur() {
  SchedulePaintForSelection();
  focus_ring_->SchedulePaint();
  needs_update_accessibility_focus_ = true;
}

void TableView::OnPaint(gfx::Canvas* canvas) {
  OnPaintImpl(canvas);
  if (needs_update_accessibility_focus_)
    UpdateAccessibilityFocus();
}

void TableView::OnPaintImpl(gfx::Canvas* canvas) {
  // Don't invoke View::OnPaint so that we can render our own focus border.

  if (sort_on_paint_)
    SortItemsAndUpdateMapping(/*schedule_paint=*/false);

  const SkColor default_bg_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TableBackground);
  canvas->DrawColor(default_bg_color);

  if (!GetRowCount() || visible_columns_.empty())
    return;

  const PaintRegion region(GetPaintRegion(GetPaintBounds(canvas)));
  if (region.min_column == -1)
    return;  // No need to paint anything.

  const SkColor selected_bg_color =
      GetNativeTheme()->GetSystemColor(text_background_color_id(HasFocus()));
  const SkColor fg_color =
      GetNativeTheme()->GetSystemColor(ui::NativeTheme::kColorId_TableText);
  const SkColor selected_fg_color =
      GetNativeTheme()->GetSystemColor(selected_text_color_id(HasFocus()));
  const SkColor alternate_bg_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TableBackgroundAlternate);
  const int cell_margin = GetCellMargin();
  const int cell_element_spacing = GetCellElementSpacing();
  for (int i = region.min_row; i < region.max_row; ++i) {
    const int model_index = ViewToModel(i);
    const bool is_selected = selection_model_.IsSelected(model_index);
    if (is_selected)
      canvas->FillRect(GetRowBounds(i), selected_bg_color);
    else if (alternate_bg_color != default_bg_color && (i % 2))
      canvas->FillRect(GetRowBounds(i), alternate_bg_color);
    for (int j = region.min_column; j < region.max_column; ++j) {
      const gfx::Rect cell_bounds(GetCellBounds(i, j));
      int text_x = cell_margin + cell_bounds.x();

      // Provide space for the grouping indicator, but draw it separately.
      if (j == 0 && grouper_)
        text_x += kGroupingIndicatorSize + cell_element_spacing;

      // Always paint the icon in the first visible column.
      if (j == 0 && table_type_ == ICON_AND_TEXT) {
        gfx::ImageSkia image = model_->GetIcon(model_index);
        if (!image.isNull()) {
          int image_x =
              GetMirroredXWithWidthInView(text_x, ui::TableModel::kIconSize);
          canvas->DrawImageInt(
              image, 0, 0, image.width(), image.height(), image_x,
              cell_bounds.y() +
                  (cell_bounds.height() - ui::TableModel::kIconSize) / 2,
              ui::TableModel::kIconSize, ui::TableModel::kIconSize, true);
        }
        text_x += ui::TableModel::kIconSize + cell_element_spacing;
      }
      if (text_x < cell_bounds.right() - cell_margin) {
        canvas->DrawStringRectWithFlags(
            model_->GetText(model_index, visible_columns_[j].column.id),
            font_list_, is_selected ? selected_fg_color : fg_color,
            gfx::Rect(GetMirroredXWithWidthInView(
                          text_x, cell_bounds.right() - text_x - cell_margin),
                      cell_bounds.y(), cell_bounds.right() - text_x,
                      row_height_),
            TableColumnAlignmentToCanvasAlignment(
                visible_columns_[j].column.alignment));
      }
    }
  }

  if (!grouper_ || region.min_column > 0)
    return;

  const SkColor grouping_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TableGroupingIndicatorColor);
  cc::PaintFlags grouping_flags;
  grouping_flags.setColor(grouping_color);
  grouping_flags.setStyle(cc::PaintFlags::kFill_Style);
  grouping_flags.setStrokeWidth(kGroupingIndicatorSize);
  grouping_flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  grouping_flags.setAntiAlias(true);
  const int group_indicator_x = GetMirroredXInView(
      GetCellBounds(0, 0).x() + cell_margin + kGroupingIndicatorSize / 2);
  for (int i = region.min_row; i < region.max_row;) {
    const int model_index = ViewToModel(i);
    GroupRange range;
    grouper_->GetGroupRange(model_index, &range);
    DCHECK_GT(range.length, 0);
    // The order of rows in a group is consistent regardless of sort, so it's ok
    // to do this calculation.
    const int start = i - (model_index - range.start);
    const int last = start + range.length - 1;
    const gfx::RectF start_cell_bounds(GetCellBounds(start, 0));
    const gfx::RectF last_cell_bounds(GetCellBounds(last, 0));
    canvas->DrawLine(
        gfx::PointF(group_indicator_x, start_cell_bounds.CenterPoint().y()),
        gfx::PointF(group_indicator_x, last_cell_bounds.CenterPoint().y()),
        grouping_flags);
    i = last + 1;
  }
}

int TableView::GetCellMargin() const {
  return LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TABLE_CELL_HORIZONTAL_MARGIN);
}

int TableView::GetCellElementSpacing() const {
  return LayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL);
}

void TableView::NumRowsChanged() {
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
  PreferredSizeChanged();
}

void TableView::SortItemsAndUpdateMapping(bool schedule_paint) {
  if (!GetIsSorted()) {
    view_to_model_.clear();
    model_to_view_.clear();
  } else {
    const int row_count = GetRowCount();
    view_to_model_.resize(row_count);
    model_to_view_.resize(row_count);
    for (int i = 0; i < row_count; ++i)
      view_to_model_[i] = i;
    if (grouper_) {
      GroupSortHelper sort_helper(this);
      GetModelIndexToRangeStart(grouper_, GetRowCount(),
                                &sort_helper.model_index_to_range_start);
      std::stable_sort(view_to_model_.begin(), view_to_model_.end(),
                       sort_helper);
    } else {
      std::stable_sort(view_to_model_.begin(), view_to_model_.end(),
                       SortHelper(this));
    }
    for (int i = 0; i < row_count; ++i)
      model_to_view_[view_to_model_[i]] = i;
    model_->ClearCollator();
  }

  UpdateVirtualAccessibilityChildren();
  if (schedule_paint)
    SchedulePaint();
}

int TableView::CompareRows(int model_row1, int model_row2) {
  const int sort_result = model_->CompareValues(model_row1, model_row2,
                                                sort_descriptors_[0].column_id);
  if (sort_result == 0 && sort_descriptors_.size() > 1) {
    // Try the secondary sort.
    return SwapCompareResult(
        model_->CompareValues(model_row1, model_row2,
                              sort_descriptors_[1].column_id),
        sort_descriptors_[1].ascending);
  }
  return SwapCompareResult(sort_result, sort_descriptors_[0].ascending);
}

gfx::Rect TableView::GetRowBounds(int row) const {
  return gfx::Rect(0, row * row_height_, width(), row_height_);
}

gfx::Rect TableView::GetCellBounds(int row, int visible_column_index) const {
  if (!header_)
    return GetRowBounds(row);
  const VisibleColumn& vis_col(visible_columns_[visible_column_index]);
  return gfx::Rect(vis_col.x, row * row_height_, vis_col.width, row_height_);
}

gfx::Rect TableView::GetActiveCellBounds() const {
  if (selection_model_.active() == ui::ListSelectionModel::kUnselectedIndex) {
    return gfx::Rect();
  }
  return GetCellBounds(ModelToView(selection_model_.active()),
                       active_visible_column_index_);
}

void TableView::AdjustCellBoundsForText(int visible_column_index,
                                        gfx::Rect* bounds) const {
  const int cell_margin = GetCellMargin();
  const int cell_element_spacing = GetCellElementSpacing();
  int text_x = cell_margin + bounds->x();
  if (visible_column_index == 0) {
    if (grouper_)
      text_x += kGroupingIndicatorSize + cell_element_spacing;
    if (table_type_ == ICON_AND_TEXT)
      text_x += ui::TableModel::kIconSize + cell_element_spacing;
  }
  bounds->set_x(text_x);
  bounds->set_width(std::max(0, bounds->right() - cell_margin - text_x));
}

void TableView::CreateHeaderIfNecessary(ScrollView* scroll_view) {
  // Only create a header if there is more than one column or the title of the
  // only column is not empty.
  if (header_ || (columns_.size() == 1 && columns_[0].title.empty()))
    return;

  header_ = scroll_view->SetHeader(std::make_unique<TableHeader>(this));
  UpdateVirtualAccessibilityChildren();
}

void TableView::UpdateVisibleColumnSizes() {
  if (!header_)
    return;

  std::vector<ui::TableColumn> columns;
  for (const auto& visible_column : visible_columns_)
    columns.push_back(visible_column.column);

  const int cell_margin = GetCellMargin();
  const int cell_element_spacing = GetCellElementSpacing();
  int first_column_padding = 0;
  if (table_type_ == ICON_AND_TEXT && header_)
    first_column_padding += ui::TableModel::kIconSize + cell_element_spacing;
  if (grouper_)
    first_column_padding += kGroupingIndicatorSize + cell_element_spacing;

  std::vector<int> sizes = views::CalculateTableColumnSizes(
      layout_width_, first_column_padding, header_->font_list(), font_list_,
      std::max(cell_margin, TableHeader::kHorizontalPadding) * 2,
      TableHeader::kSortIndicatorWidth, columns, model_);
  DCHECK_EQ(visible_columns_.size(), sizes.size());
  int x = 0;
  for (size_t i = 0; i < visible_columns_.size(); ++i) {
    visible_columns_[i].x = x;
    visible_columns_[i].width = sizes[i];
    x += sizes[i];
  }
}

TableView::PaintRegion TableView::GetPaintRegion(
    const gfx::Rect& bounds) const {
  DCHECK(!visible_columns_.empty());
  DCHECK(GetRowCount());

  PaintRegion region;
  region.min_row =
      base::ClampToRange(bounds.y() / row_height_, 0, GetRowCount() - 1);
  region.max_row = bounds.bottom() / row_height_;
  if (bounds.bottom() % row_height_ != 0)
    region.max_row++;
  region.max_row = std::min(region.max_row, GetRowCount());

  if (!header_) {
    region.max_column = 1;
    return region;
  }

  const int paint_x = GetMirroredXForRect(bounds);
  const int paint_max_x = paint_x + bounds.width();
  region.min_column = -1;
  region.max_column = visible_columns_.size();
  for (size_t i = 0; i < visible_columns_.size(); ++i) {
    int max_x = visible_columns_[i].x + visible_columns_[i].width;
    if (region.min_column == -1 && max_x >= paint_x)
      region.min_column = static_cast<int>(i);
    if (region.min_column != -1 && visible_columns_[i].x >= paint_max_x) {
      region.max_column = i;
      break;
    }
  }
  return region;
}

gfx::Rect TableView::GetPaintBounds(gfx::Canvas* canvas) const {
  SkRect sk_clip_rect;
  if (canvas->sk_canvas()->getLocalClipBounds(&sk_clip_rect))
    return gfx::ToEnclosingRect(gfx::SkRectToRectF(sk_clip_rect));
  return GetVisibleBounds();
}

void TableView::SchedulePaintForSelection() {
  if (selection_model_.size() == 1) {
    const int first_model_row = GetFirstSelectedRow();
    SchedulePaintInRect(GetRowBounds(ModelToView(first_model_row)));
    if (first_model_row != selection_model_.active())
      SchedulePaintInRect(GetRowBounds(ModelToView(selection_model_.active())));
  } else if (selection_model_.size() > 1) {
    SchedulePaint();
  }
}

ui::TableColumn TableView::FindColumnByID(int id) const {
  const auto ids_match = [id](const auto& column) { return column.id == id; };
  const auto i = std::find_if(columns_.cbegin(), columns_.cend(), ids_match);
  DCHECK(i != columns_.cend());
  return *i;
}

void TableView::AdvanceActiveVisibleColumn(AdvanceDirection direction) {
  if (visible_columns_.empty()) {
    SetActiveVisibleColumnIndex(-1);
    return;
  }

  if (active_visible_column_index_ == -1) {
    if (selection_model_.active() == -1)
      SelectByViewIndex(0);
    SetActiveVisibleColumnIndex(0);
    return;
  }

  if (direction == AdvanceDirection::kDecrement) {
    SetActiveVisibleColumnIndex(std::max(0, active_visible_column_index_ - 1));
  } else {
    SetActiveVisibleColumnIndex(
        std::min(static_cast<int>(visible_columns_.size()) - 1,
                 active_visible_column_index_ + 1));
  }
}

int TableView::GetActiveVisibleColumnIndex() const {
  return active_visible_column_index_;
}

void TableView::SetActiveVisibleColumnIndex(int index) {
  if (active_visible_column_index_ == index)
    return;
  active_visible_column_index_ = index;

  if (selection_model_.active() != ui::ListSelectionModel::kUnselectedIndex &&
      active_visible_column_index_ != -1) {
    ScrollRectToVisible(GetCellBounds(ModelToView(selection_model_.active()),
                                      active_visible_column_index_));
  }

  focus_ring_->SchedulePaint();
  needs_update_accessibility_focus_ = true;
  OnPropertyChanged(&active_visible_column_index_, kPropertyEffectsNone);
}

void TableView::SelectByViewIndex(int view_index) {
  ui::ListSelectionModel new_selection;
  if (view_index != -1) {
    SelectRowsInRangeFrom(view_index, true, &new_selection);
    new_selection.set_anchor(ViewToModel(view_index));
    new_selection.set_active(ViewToModel(view_index));
  }

  SetSelectionModel(std::move(new_selection));
}

void TableView::SetSelectionModel(ui::ListSelectionModel new_selection) {
  if (new_selection == selection_model_)
    return;

  SchedulePaintForSelection();
  selection_model_ = std::move(new_selection);
  SchedulePaintForSelection();

  // Scroll the group for the active item to visible.
  if (selection_model_.active() != -1) {
    gfx::Rect vis_rect(GetVisibleBounds());
    const GroupRange range(GetGroupRange(selection_model_.active()));
    const int start_y = GetRowBounds(ModelToView(range.start)).y();
    const int end_y =
        GetRowBounds(ModelToView(range.start + range.length - 1)).bottom();
    vis_rect.set_y(start_y);
    vis_rect.set_height(end_y - start_y);
    ScrollRectToVisible(vis_rect);

    if (active_visible_column_index_ == -1)
      SetActiveVisibleColumnIndex(0);
  } else {
    SetActiveVisibleColumnIndex(-1);
  }

  focus_ring_->SchedulePaint();
  needs_update_accessibility_focus_ = true;
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  if (observer_)
    observer_->OnSelectionChanged();
}

void TableView::AdvanceSelection(AdvanceDirection direction) {
  if (selection_model_.active() == -1) {
    SelectByViewIndex(0);
    return;
  }
  int view_index = ModelToView(selection_model_.active());
  if (direction == AdvanceDirection::kDecrement)
    view_index = std::max(0, view_index - 1);
  else
    view_index = std::min(GetRowCount() - 1, view_index + 1);
  SelectByViewIndex(view_index);
}

void TableView::ConfigureSelectionModelForEvent(
    const ui::LocatedEvent& event,
    ui::ListSelectionModel* model) const {
  const int view_index = event.y() / row_height_;
  DCHECK(view_index >= 0 && view_index < GetRowCount());

  if (selection_model_.anchor() == -1 || single_selection_ ||
      (!IsCmdOrCtrl(event) && !event.IsShiftDown())) {
    SelectRowsInRangeFrom(view_index, true, model);
    model->set_anchor(ViewToModel(view_index));
    model->set_active(ViewToModel(view_index));
    return;
  }
  if ((IsCmdOrCtrl(event) && event.IsShiftDown()) || event.IsShiftDown()) {
    // control-shift: copy existing model and make sure rows between anchor and
    // |view_index| are selected.
    // shift: reset selection so that only rows between anchor and |view_index|
    // are selected.
    if (IsCmdOrCtrl(event) && event.IsShiftDown())
      *model = selection_model_;
    else
      model->set_anchor(selection_model_.anchor());
    for (int i = std::min(view_index, ModelToView(model->anchor())),
             end = std::max(view_index, ModelToView(model->anchor()));
         i <= end; ++i) {
      SelectRowsInRangeFrom(i, true, model);
    }
    model->set_active(ViewToModel(view_index));
  } else {
    DCHECK(IsCmdOrCtrl(event));
    // Toggle the selection state of |view_index| and set the anchor/active to
    // it and don't change the state of any other rows.
    *model = selection_model_;
    model->set_anchor(ViewToModel(view_index));
    model->set_active(ViewToModel(view_index));
    SelectRowsInRangeFrom(view_index,
                          !model->IsSelected(ViewToModel(view_index)), model);
  }
}

void TableView::SelectRowsInRangeFrom(int view_index,
                                      bool select,
                                      ui::ListSelectionModel* model) const {
  const GroupRange range(GetGroupRange(ViewToModel(view_index)));
  for (int i = 0; i < range.length; ++i) {
    if (select)
      model->AddIndexToSelection(range.start + i);
    else
      model->RemoveIndexFromSelection(range.start + i);
  }
}

GroupRange TableView::GetGroupRange(int model_index) const {
  GroupRange range;
  if (grouper_) {
    grouper_->GetGroupRange(model_index, &range);
  } else {
    range.start = model_index;
    range.length = 1;
  }
  return range;
}

void TableView::UpdateVirtualAccessibilityChildren() {
  ClearVirtualAccessibilityChildren();
  if (!GetRowCount() || visible_columns_.empty())
    return;

  const base::Optional<int> primary_sorted_column_id =
      sort_descriptors().empty()
          ? base::nullopt
          : base::make_optional(sort_descriptors()[0].column_id);
  if (header_) {
    auto ax_header = std::make_unique<AXVirtualView>();
    ui::AXNodeData& header_data = ax_header->GetCustomData();
    header_data.role = ax::mojom::Role::kRow;

    for (size_t visible_column_index = 0;
         visible_column_index < visible_columns_.size();
         ++visible_column_index) {
      const VisibleColumn& visible_column =
          visible_columns_[visible_column_index];
      const ui::TableColumn column = visible_column.column;
      auto ax_cell = std::make_unique<AXVirtualView>();
      ui::AXNodeData& cell_data = ax_cell->GetCustomData();
      cell_data.role = ax::mojom::Role::kColumnHeader;
      cell_data.SetName(column.title);
      cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex,
                                static_cast<int32_t>(visible_column_index));
      cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan,
                                1);
      if (base::i18n::IsRTL())
        cell_data.SetTextDirection(ax::mojom::WritingDirection::kRtl);

      auto sort_direction = ax::mojom::SortDirection::kUnsorted;
      if (column.sortable && primary_sorted_column_id.has_value() &&
          column.id == primary_sorted_column_id.value()) {
        DCHECK(!sort_descriptors().empty());
        if (sort_descriptors()[0].ascending)
          sort_direction = ax::mojom::SortDirection::kAscending;
        else
          sort_direction = ax::mojom::SortDirection::kDescending;
      }
      cell_data.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                                static_cast<int32_t>(sort_direction));

      ax_header->AddChildView(std::move(ax_cell));
    }

    GetViewAccessibility().AddVirtualChildView(std::move(ax_header));
  }

  for (int view_index = 0; view_index < GetRowCount(); ++view_index) {
    const int model_index = ViewToModel(view_index);
    auto ax_row = std::make_unique<AXVirtualView>();
    ui::AXNodeData& row_data = ax_row->GetCustomData();
    row_data.role = ax::mojom::Role::kRow;

    if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
      row_data.AddState(ax::mojom::State::kFocusable);
      row_data.AddAction(ax::mojom::Action::kFocus);
      row_data.AddAction(ax::mojom::Action::kScrollToMakeVisible);
      row_data.AddAction(ax::mojom::Action::kSetSelection);

      // When navigating using up / down cursor keys on the Mac, we read the
      // contents of the first cell. If the user needs to explore additional
      // cells, they can use VoiceOver shortcuts.
      row_data.SetName(
          model_->GetText(model_index, visible_columns_[0].column.id));
    }

    row_data.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kSelect);
    row_data.AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex,
                             static_cast<int32_t>(view_index));
    if (!single_selection_)
      row_data.AddState(ax::mojom::State::kMultiselectable);

    base::RepeatingCallback<void(ui::AXNodeData*)> row_callback =
        base::BindRepeating(
            [](TableView* table, int model_index, ui::AXNodeData* data) {
              DCHECK(table);
              gfx::Rect row_bounds =
                  table->GetRowBounds(table->ModelToView(model_index));
              if (!table->GetVisibleBounds().Intersects(row_bounds))
                data->AddState(ax::mojom::State::kInvisible);
              if (table->selection_model().IsSelected(model_index)) {
                data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                       true);
              }
            },
            base::Unretained(this), model_index);
    ax_row->SetPopulateDataCallback(std::move(row_callback));

    for (size_t visible_column_index = 0;
         visible_column_index < visible_columns_.size();
         ++visible_column_index) {
      const VisibleColumn& visible_column =
          visible_columns_[visible_column_index];
      const ui::TableColumn column = visible_column.column;
      auto ax_cell = std::make_unique<AXVirtualView>();
      ui::AXNodeData& cell_data = ax_cell->GetCustomData();
      cell_data.role = ax::mojom::Role::kCell;
      if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
        cell_data.AddState(ax::mojom::State::kFocusable);
        cell_data.AddAction(ax::mojom::Action::kFocus);
        cell_data.AddAction(ax::mojom::Action::kScrollLeft);
        cell_data.AddAction(ax::mojom::Action::kScrollRight);
        cell_data.AddAction(ax::mojom::Action::kScrollToMakeVisible);
        cell_data.AddAction(ax::mojom::Action::kSetSelection);
      }

      cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex,
                                static_cast<int32_t>(view_index));
      cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, 1);
      cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex,
                                static_cast<int32_t>(visible_column_index));
      cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan,
                                1);

      cell_data.SetName(model_->GetText(model_index, column.id));
      if (base::i18n::IsRTL())
        cell_data.SetTextDirection(ax::mojom::WritingDirection::kRtl);

      auto sort_direction = ax::mojom::SortDirection::kUnsorted;
      if (column.sortable && primary_sorted_column_id.has_value() &&
          column.id == primary_sorted_column_id.value()) {
        DCHECK(!sort_descriptors().empty());
        if (sort_descriptors()[0].ascending)
          sort_direction = ax::mojom::SortDirection::kAscending;
        else
          sort_direction = ax::mojom::SortDirection::kDescending;
      }
      cell_data.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                                static_cast<int32_t>(sort_direction));

      base::RepeatingCallback<void(ui::AXNodeData*)> cell_callback =
          base::BindRepeating(
              [](TableView* table, int model_index, size_t visible_column_index,
                 ui::AXNodeData* data) {
                DCHECK(table);
                gfx::Rect cell_bounds = table->GetCellBounds(
                    table->ModelToView(model_index), visible_column_index);
                if (!table->GetVisibleBounds().Intersects(cell_bounds))
                  data->AddState(ax::mojom::State::kInvisible);
                if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell &&
                    static_cast<const int>(visible_column_index) ==
                        table->GetActiveVisibleColumnIndex()) {
                  if (table->selection_model().IsSelected(model_index)) {
                    data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                           true);
                  }
                }
              },
              base::Unretained(this), model_index, visible_column_index);
      ax_cell->SetPopulateDataCallback(std::move(cell_callback));

      ax_row->AddChildView(std::move(ax_cell));
    }

    GetViewAccessibility().AddVirtualChildView(std::move(ax_row));
  }

  UpdateVirtualAccessibilityChildrenBounds();
}

void TableView::ClearVirtualAccessibilityChildren() {
  GetViewAccessibility().RemoveAllVirtualChildViews();
}

void TableView::UpdateVirtualAccessibilityChildrenBounds() {
  // The virtual children may be empty if the |model_| is in the process of
  // updating (e.g. showing or hiding a column) but the virtual accessibility
  // children haven't been updated yet to reflect the new model.
  auto& virtual_children = GetViewAccessibility().virtual_children();
  if (virtual_children.empty())
    return;

  // Update the bounds for the header first, if applicable.
  if (header_) {
    auto& ax_row = virtual_children[0];
    ui::AXNodeData& row_data = ax_row->GetCustomData();
    DCHECK_EQ(ax_row->GetData().role, ax::mojom::Role::kRow);
    row_data.relative_bounds.bounds =
        gfx::RectF(CalculateHeaderRowAccessibilityBounds());

    // Update the bounds for every child cell in this row.
    for (size_t visible_column_index = 0;
         visible_column_index < ax_row->children().size();
         visible_column_index++) {
      ui::AXNodeData& cell_data =
          ax_row->children()[visible_column_index]->GetCustomData();

      DCHECK_EQ(cell_data.role, ax::mojom::Role::kColumnHeader);
      cell_data.relative_bounds.bounds = gfx::RectF(
          CalculateHeaderCellAccessibilityBounds(visible_column_index));
    }
  }

  // Update the bounds for the table's content rows.
  for (int row_index = 0; row_index < GetRowCount(); row_index++) {
    auto& ax_row = virtual_children[header_ ? row_index + 1 : row_index];
    ui::AXNodeData& row_data = ax_row->GetCustomData();
    DCHECK_EQ(ax_row->GetData().role, ax::mojom::Role::kRow);
    row_data.relative_bounds.bounds =
        gfx::RectF(CalculateTableRowAccessibilityBounds(row_index));

    // Update the bounds for every child cell in this row.
    for (size_t visible_column_index = 0;
         visible_column_index < ax_row->children().size();
         visible_column_index++) {
      ui::AXNodeData& cell_data =
          ax_row->children()[visible_column_index]->GetCustomData();

      DCHECK_EQ(cell_data.role, ax::mojom::Role::kCell);
      cell_data.relative_bounds.bounds =
          gfx::RectF(CalculateTableCellAccessibilityBounds(
              row_index, visible_column_index));
    }
  }
}

gfx::Rect TableView::CalculateHeaderRowAccessibilityBounds() const {
  gfx::Rect header_bounds = header_->GetVisibleBounds();
  gfx::Point header_origin = header_bounds.origin();
  ConvertPointToTarget(header_, this, &header_origin);
  header_bounds.set_origin(header_origin);
  return header_bounds;
}

gfx::Rect TableView::CalculateHeaderCellAccessibilityBounds(
    const int visible_column_index) const {
  const gfx::Rect& header_bounds = CalculateHeaderRowAccessibilityBounds();
  const VisibleColumn& visible_column = visible_columns_[visible_column_index];
  gfx::Rect header_cell_bounds(visible_column.x, header_bounds.y(),
                               visible_column.width, header_bounds.height());
  return header_cell_bounds;
}

gfx::Rect TableView::CalculateTableRowAccessibilityBounds(
    const int row_index) const {
  gfx::Rect row_bounds = GetRowBounds(row_index);
  return row_bounds;
}

gfx::Rect TableView::CalculateTableCellAccessibilityBounds(
    const int row_index,
    const int visible_column_index) const {
  gfx::Rect cell_bounds = GetCellBounds(row_index, visible_column_index);
  return cell_bounds;
}

void TableView::UpdateAccessibilityFocus() {
  needs_update_accessibility_focus_ = false;
  if (!HasFocus())
    return;

  if (selection_model_.active() == ui::ListSelectionModel::kUnselectedIndex ||
      active_visible_column_index_ == -1) {
    GetViewAccessibility().OverrideFocus(nullptr);
    return;
  }

  int active_row = ModelToView(selection_model_.active());
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    AXVirtualView* ax_row = GetVirtualAccessibilityRow(active_row);
    if (ax_row) {
      GetViewAccessibility().OverrideFocus(ax_row);
    }
  } else {
    AXVirtualView* ax_cell =
        GetVirtualAccessibilityCell(active_row, active_visible_column_index_);
    if (ax_cell) {
      GetViewAccessibility().OverrideFocus(ax_cell);
    }
  }
}

AXVirtualView* TableView::GetVirtualAccessibilityRow(int row) {
  DCHECK_GE(row, 0);
  DCHECK_LT(row, GetRowCount());
  if (header_)
    ++row;
  if (size_t{row} < GetViewAccessibility().virtual_children().size()) {
    const auto& ax_row = GetViewAccessibility().virtual_children()[size_t{row}];
    DCHECK(ax_row);
    DCHECK_EQ(ax_row->GetData().role, ax::mojom::Role::kRow);
    return ax_row.get();
  }
  NOTREACHED() << "|row| not found. Did you forget to call "
                  "UpdateVirtualAccessibilityChildren()?";
  return nullptr;
}

AXVirtualView* TableView::GetVirtualAccessibilityCell(
    int row,
    int visible_column_index) {
  AXVirtualView* ax_row = GetVirtualAccessibilityRow(row);
  DCHECK(ax_row) << "|row| not found. Did you forget to call "
                    "UpdateVirtualAccessibilityChildren()?";
  const auto matches_index = [visible_column_index](const auto& ax_cell) {
    DCHECK(ax_cell);
    DCHECK(ax_cell->GetData().role == ax::mojom::Role::kColumnHeader ||
           ax_cell->GetData().role == ax::mojom::Role::kCell);
    return ax_cell->GetData().GetIntAttribute(
               ax::mojom::IntAttribute::kTableCellColumnIndex) ==
           visible_column_index;
  };
  const auto i = std::find_if(ax_row->children().cbegin(),
                              ax_row->children().cend(), matches_index);
  DCHECK(i != ax_row->children().cend())
      << "|visible_column_index| not found. Did you forget to call "
      << "UpdateVirtualAccessibilityChildren()?";
  return i->get();
}

DEFINE_ENUM_CONVERTERS(TableTypes,
                       {TableTypes::TEXT_ONLY, base::ASCIIToUTF16("TEXT_ONLY")},
                       {TableTypes::ICON_AND_TEXT,
                        base::ASCIIToUTF16("ICON_AND_TEXT")})

BEGIN_METADATA(TableView, View)
ADD_READONLY_PROPERTY_METADATA(int, RowCount)
ADD_READONLY_PROPERTY_METADATA(int, FirstSelectedRow)
ADD_READONLY_PROPERTY_METADATA(bool, HasFocusIndicator)
ADD_PROPERTY_METADATA(int, ActiveVisibleColumnIndex)
ADD_READONLY_PROPERTY_METADATA(bool, IsSorted)
ADD_READONLY_PROPERTY_METADATA(int, RowHeight)
ADD_PROPERTY_METADATA(bool, SelectOnRemove)
ADD_READONLY_PROPERTY_METADATA(TableTypes, TableType)
ADD_PROPERTY_METADATA(bool, SortOnPaint)
END_METADATA

}  // namespace views
