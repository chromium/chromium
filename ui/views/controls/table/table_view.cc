// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_view.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_utils.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/ax_virtual_view.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_grouper.h"
#include "ui/views/controls/table/table_header.h"
#include "ui/views/controls/table/table_utils.h"
#include "ui/views/controls/table/table_view_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_utils.h"

namespace views {

namespace {

constexpr int kGroupingIndicatorSize = 6;

// Returns result, unless ascending is false in which case -result is returned.
int SwapCompareResult(int result, bool ascending) {
  return ascending ? result : -result;
}

// Populates |model_index_to_range_start| based on the |grouper|.
void GetModelIndexToRangeStart(
    TableGrouper* grouper,
    size_t row_count,
    std::map<size_t, size_t>* model_index_to_range_start) {
  for (size_t model_index = 0; model_index < row_count;) {
    GroupRange range;
    grouper->GetGroupRange(model_index, &range);
    DCHECK_GT(range.length, 0u);
    for (size_t i = model_index; i < model_index + range.length; ++i)
      (*model_index_to_range_start)[i] = model_index;
    model_index += range.length;
  }
}

// Returns the color id for the background of selected text. |has_focus|
// indicates if the table has focus.
ui::ColorId text_background_color_id(bool has_focus) {
  return has_focus ? ui::kColorTableBackgroundSelectedFocused
                   : ui::kColorTableBackgroundSelectedUnfocused;
}

// Returns the color id for text. |has_focus| indicates if the table has focus.
ui::ColorId selected_text_color_id(bool has_focus) {
  return has_focus ? ui::kColorTableForegroundSelectedFocused
                   : ui::kColorTableForegroundSelectedUnfocused;
}

// Whether the platform "command" key is down.
bool IsCmdOrCtrl(const ui::Event& event) {
#if BUILDFLAG(IS_MAC)
  return event.IsCommandDown();
#else
  return event.IsControlDown();
#endif
}

}  // namespace

// Used as the comparator to sort the contents of the table.
struct TableView::SortHelper {
  explicit SortHelper(TableView* table) : table(table) {}

  bool operator()(size_t model_index1, size_t model_index2) {
    return table->CompareRows(model_index1, model_index2) < 0;
  }

  raw_ptr<TableView> table;
};

// Used as the comparator to sort the contents of the table when a TableGrouper
// is present. When groups are present we sort the groups based on the first row
// in the group and within the groups we keep the same order as the model.
struct TableView::GroupSortHelper {
  explicit GroupSortHelper(TableView* table) : table(table) {}

  bool operator()(size_t model_index1, size_t model_index2) {
    const size_t range1 = model_index_to_range_start[model_index1];
    const size_t range2 = model_index_to_range_start[model_index2];
    if (range1 == range2) {
      // The two rows are in the same group, sort so that items in the same
      // group always appear in the same order.
      return model_index1 < model_index2;
    }
    return table->CompareRows(range1, range2) < 0;
  }

  raw_ptr<TableView> table;
  std::map<size_t, size_t> model_index_to_range_start;
};

TableView::VisibleColumn::VisibleColumn() = default;

TableView::VisibleColumn::~VisibleColumn() = default;

TableView::PaintRegion::PaintRegion() = default;

TableView::PaintRegion::~PaintRegion() = default;

class TableView::HighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  HighlightPathGenerator() = default;

  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

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
};

TableView::TableView() : weak_factory_(this) {
  constexpr int kTextContext = style::CONTEXT_TABLE_ROW;
  constexpr int kTextStyle = style::STYLE_BODY_4;
  font_list_ = TypographyProvider::Get().GetFont(kTextContext, kTextStyle);
  row_height_ = LayoutProvider::GetControlHeightForFont(kTextContext,
                                                        kTextStyle, font_list_);

  // Always focusable, even on Mac (consistent with NSTableView).
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  views::HighlightPathGenerator::Install(
      this, std::make_unique<TableView::HighlightPathGenerator>());

  FocusRing::Install(this);
  views::FocusRing::Get(this)->SetHasFocusPredicate(
      base::BindRepeating([](const View* view) {
        const auto* v = views::AsViewClass<TableView>(view);
        CHECK(v);
        return v->HasFocus() && !v->header_row_is_active_;
      }));
  GetViewAccessibility().SetRole(ax::mojom::Role::kListGrid);
  GetViewAccessibility().SetName(
      std::u16string(), ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  GetViewAccessibility().SetReadOnly(true);
  GetViewAccessibility().SetDefaultActionVerb(
      ax::mojom::DefaultActionVerb::kActivate);
  GetViewAccessibility().SetTableRowCount(static_cast<int32_t>(GetRowCount()));
  GetViewAccessibility().SetTableColumnCount(
      static_cast<int32_t>(visible_columns_.size()));
}

TableView::TableView(ui::TableModel* model,
                     const std::vector<ui::TableColumn>& columns,
                     TableType table_type,
                     bool single_selection)
    : TableView() {
  GetViewAccessibility().SetRole(ax::mojom::Role::kListGrid);
  Init(model, std::move(columns), table_type, single_selection);
}

TableView::~TableView() {
  if (model_)
    model_->SetObserver(nullptr);
}

// static
std::unique_ptr<ScrollView> TableView::CreateScrollViewWithTable(
    std::unique_ptr<TableView> table,
    bool has_border) {
  auto scroll_view = has_border ? ScrollView::CreateScrollViewWithBorder()
                                : std::make_unique<ScrollView>();
  auto* table_ptr = table.get();
  scroll_view->SetContents(std::move(table));
  table_ptr->CreateHeaderIfNecessary(scroll_view.get());
  return scroll_view;
}

// static
Builder<ScrollView> TableView::CreateScrollViewBuilderWithTable(
    Builder<TableView>&& table) {
  auto scroll_view = ScrollView::CreateScrollViewWithBorder();
  auto* scroll_view_ptr = scroll_view.get();
  return Builder<ScrollView>(std::move(scroll_view))
      .SetContents(std::move(table).CustomConfigure(base::BindOnce(
          [](ScrollView* scroll_view, TableView* table_view) {
            table_view->CreateHeaderIfNecessary(scroll_view);
          },
          scroll_view_ptr)));
}

void TableView::Init(ui::TableModel* model,
                     const std::vector<ui::TableColumn>& columns,
                     TableType table_type,
                     bool single_selection) {
  SetColumns(columns);
  SetTableType(table_type);
  SetSingleSelection(single_selection);
  SetModel(model);
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
  if (model_) {
    model_->SetObserver(this);

    // Clears and creates a new virtual accessibility tree.
    RebuildVirtualAccessibilityChildren();
  } else {
    ClearVirtualAccessibilityChildren();
  }
}

void TableView::SetColumns(const std::vector<ui::TableColumn>& columns) {
  columns_ = columns;
  visible_columns_.clear();
  for (const auto& column : columns) {
    VisibleColumn visible_column;
    visible_column.column = column;
    visible_columns_.push_back(visible_column);
  }

  GetViewAccessibility().SetTableColumnCount(
      static_cast<int32_t>(visible_columns_.size()));
}

void TableView::SetTableType(TableType table_type) {
  if (table_type_ == table_type)
    return;
  table_type_ = table_type;
  OnPropertyChanged(&table_type_, PropertyEffects::kPropertyEffectsLayout);
}

TableType TableView::GetTableType() const {
  return table_type_;
}

void TableView::SetSingleSelection(bool single_selection) {
  if (single_selection_ == single_selection)
    return;
  single_selection_ = single_selection;
  OnPropertyChanged(&single_selection_, PropertyEffects::kPropertyEffectsPaint);
}

bool TableView::GetSingleSelection() const {
  return single_selection_;
}

void TableView::SetGrouper(TableGrouper* grouper) {
  grouper_ = grouper;
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
}

size_t TableView::GetRowCount() const {
  return model_ ? model_->RowCount() : 0;
}

void TableView::Select(std::optional<size_t> model_row) {
  if (!model_)
    return;

  SelectByViewIndex(model_row.has_value()
                        ? std::make_optional(ModelToView(model_row.value()))
                        : std::nullopt);
}

void TableView::SetSelectionAll(bool select) {
  if (!GetRowCount())
    return;

  ui::ListSelectionModel selection_model;

  if (select) {
    selection_model.AddIndexRangeToSelection(0, GetRowCount() - 1);
    SetAccessibleSelectionForRange(0, GetRowCount() - 1, /* selected */ true);
  }

  selection_model.set_anchor(selection_model_.anchor());
  selection_model.set_active(selection_model_.active());

  SetSelectionModel(std::move(selection_model));
}

std::optional<size_t> TableView::GetFirstSelectedRow() const {
  return selection_model_.empty()
             ? std::nullopt
             : std::make_optional(*selection_model_.selected_indices().begin());
}

// TODO(dpenning) : Prevent the last column from being closed. See
// crbug.com/1324306 for details.
void TableView::SetColumnVisibility(int id, bool is_visible) {
  if (is_visible == IsColumnVisible(id))
    return;

  if (is_visible) {
    VisibleColumn visible_column;
    visible_column.column = FindColumnByID(id);
    visible_columns_.push_back(visible_column);
  } else {
    const auto i =
        base::ranges::find(visible_columns_, id,
                           [](const auto& column) { return column.column.id; });
    if (i != visible_columns_.end()) {
      visible_columns_.erase(i);
      if (active_visible_column_index_.has_value() &&
          active_visible_column_index_.value() >= visible_columns_.size())
        SetActiveVisibleColumnIndex(
            visible_columns_.empty()
                ? std::nullopt
                : std::make_optional(visible_columns_.size() - 1));
    }

    GetViewAccessibility().SetTableColumnCount(
        static_cast<int32_t>(visible_columns_.size()));
  }

  UpdateVisibleColumnSizes();
  PreferredSizeChanged();
  SchedulePaint();

  if (header_)
    header_->SchedulePaint();

  // This will clear and create the entire accessibility tree, to optimize this
  // further, removing/adding the cell's dynamically could be done instead.
  RebuildVirtualAccessibilityChildren();
}

void TableView::ToggleSortOrder(size_t visible_column_index) {
  DCHECK(visible_column_index < visible_columns_.size());
  const ui::TableColumn& column = visible_columns_[visible_column_index].column;
  if (!column.sortable)
    return;
  SortDescriptors sort(sort_descriptors_);
  if (!sort.empty() && sort[0].column_id == column.id) {
    if (sort[0].ascending == column.initial_sort_is_ascending) {
      // First toggle inverts the order.
      sort[0].ascending = !sort[0].ascending;
      GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
          sort[0].ascending ? IDS_APP_TABLE_COLUMN_SORTED_ASC_ACCNAME
                            : IDS_APP_TABLE_COLUMN_SORTED_DESC_ACCNAME,
          column.title));
    } else {
      // Second toggle clears the sort.
      sort.clear();
      GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
          IDS_APP_TABLE_COLUMN_NOT_SORTED_ACCNAME, column.title));
    }
  } else {
    SortDescriptor descriptor(column.id, column.initial_sort_is_ascending);
    sort.insert(sort.begin(), descriptor);
    // Only persist two sort descriptors.
    if (sort.size() > 2)
      sort.resize(2);
    GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
        sort[0].ascending ? IDS_APP_TABLE_COLUMN_SORTED_ASC_ACCNAME
                          : IDS_APP_TABLE_COLUMN_SORTED_DESC_ACCNAME,
        column.title));
  }
  SetSortDescriptors(sort);
  UpdateFocusRings();
}

void TableView::SetSortDescriptors(const SortDescriptors& sort_descriptors) {
  sort_descriptors_ = sort_descriptors;
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
  if (header_)
    header_->SchedulePaint();
}

bool TableView::IsColumnVisible(int id) const {
  return base::Contains(visible_columns_, id, [](const VisibleColumn& column) {
    return column.column.id;
  });
}

bool TableView::HasColumn(int id) const {
  return base::Contains(columns_, id, &ui::TableColumn::id);
}

bool TableView::GetHasFocusIndicator() const {
  return selection_model_.active().has_value() &&
         active_visible_column_index_.has_value();
}

void TableView::SetObserver(TableViewObserver* observer) {
  if (observer_ == observer)
    return;
  observer_ = observer;
  OnPropertyChanged(&observer_, PropertyEffects::kPropertyEffectsNone);
}

TableViewObserver* TableView::GetObserver() const {
  return observer_;
}

const TableView::VisibleColumn& TableView::GetVisibleColumn(size_t index) {
  DCHECK(index < visible_columns_.size());
  return visible_columns_[index];
}

void TableView::SetVisibleColumnWidth(size_t index, int width) {
  DCHECK(index < visible_columns_.size());
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
  UpdateFocusRings();
  UpdateVirtualAccessibilityChildrenBounds();
}

size_t TableView::ModelToView(size_t model_index) const {
  if (!GetIsSorted())
    return model_index;
  CHECK_LT(model_index, model_to_view_.size())
      << " out of bounds model_index " << model_index;
  return model_to_view_[model_index];
}

size_t TableView::ViewToModel(size_t view_index) const {
  CHECK_LT(view_index, GetRowCount());
  if (!GetIsSorted())
    return view_index;
  DCHECK_LT(view_index, view_to_model_.size())
      << " out of bounds view_index " << view_index;
  return view_to_model_[view_index];
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

bool TableView::GetSortOnPaint() const {
  return sort_on_paint_;
}

void TableView::SetSortOnPaint(bool sort_on_paint) {
  if (sort_on_paint_ == sort_on_paint)
    return;

  sort_on_paint_ = sort_on_paint;
  OnPropertyChanged(&sort_on_paint_, kPropertyEffectsNone);
}

ax::mojom::SortDirection TableView::GetFirstSortDescriptorDirection() const {
  DCHECK(!sort_descriptors().empty());
  if (sort_descriptors()[0].ascending)
    return ax::mojom::SortDirection::kAscending;
  return ax::mojom::SortDirection::kDescending;
}

void TableView::Layout(PassKey) {
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
  gfx::Size pref = GetPreferredSize({});
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
                  gfx::Size(width, header_->GetPreferredSize({}).height())));
  }

  views::FocusRing::Get(this)->DeprecatedLayoutImmediately();
}

gfx::Size TableView::CalculatePreferredSize(
    const SizeBounds& /*available_size*/) const {
  int width = 50;
  if (header_ && !visible_columns_.empty())
    width = visible_columns_.back().x + visible_columns_.back().width;
  return gfx::Size(width, static_cast<int>(GetRowCount()) * row_height_);
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
        SetSelectionAll(/*select=*/true);
        return true;
      }
      break;

    case ui::VKEY_HOME:
      if (header_row_is_active_)
        break;
      if (GetRowCount())
        SelectByViewIndex(size_t{0});
      return true;

    case ui::VKEY_END:
      if (header_row_is_active_)
        break;
      if (GetRowCount())
        SelectByViewIndex(GetRowCount() - 1);
      return true;

    case ui::VKEY_UP:
#if BUILDFLAG(IS_MAC)
      if (event.IsAltDown()) {
        if (GetRowCount())
          SelectByViewIndex(size_t{0});
      } else {
        AdvanceSelection(AdvanceDirection::kDecrement);
      }
#else
      AdvanceSelection(AdvanceDirection::kDecrement);
#endif
      return true;

    case ui::VKEY_DOWN:
#if BUILDFLAG(IS_MAC)
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
          if (active_visible_column_index_.has_value() && header_) {
            header_->ResizeColumnViaKeyboard(
                active_visible_column_index_.value(), direction);
            UpdateFocusRings();
          }
        } else {
          AdvanceActiveVisibleColumn(direction);
        }
        return true;
      }
      break;

    case ui::VKEY_RIGHT:
      // TODO(crbug.com/40773239): Update TableView to support keyboard
      // navigation to table cells on Mac when "Full keyboard access" is
      // specified.
      if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
        const AdvanceDirection direction = base::i18n::IsRTL()
                                               ? AdvanceDirection::kDecrement
                                               : AdvanceDirection::kIncrement;
        if (IsCmdOrCtrl(event)) {
          if (active_visible_column_index_.has_value() && header_) {
            header_->ResizeColumnViaKeyboard(
                active_visible_column_index_.value(), direction);
            UpdateFocusRings();
          }
        } else {
          AdvanceActiveVisibleColumn(direction);
        }
        return true;
      }
      break;

    // Currently there are TableView clients that take an action when the return
    // key is pressed and there is an active selection in the body. To avoid
    // breaking these cases only allow toggling sort order with the return key
    // when the table header is active.
    case ui::VKEY_RETURN:
      if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell &&
          active_visible_column_index_.has_value() && header_row_is_active_) {
        ToggleSortOrder(active_visible_column_index_.value());
        return true;
      }
      break;

    case ui::VKEY_SPACE:
      if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell &&
          active_visible_column_index_.has_value()) {
        ToggleSortOrder(active_visible_column_index_.value());
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
  if (row < 0 || static_cast<size_t>(row) >= GetRowCount())
    return true;

  if (event.GetClickCount() == 2) {
    SelectByViewIndex(static_cast<size_t>(row));
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
  if (event->type() != ui::EventType::kGestureTapDown) {
    return;
  }

  RequestFocus();

  const int row = event->y() / row_height_;
  if (row < 0 || static_cast<size_t>(row) >= GetRowCount())
    return;

  event->StopPropagation();
  ui::ListSelectionModel new_model;
  ConfigureSelectionModelForEvent(*event, &new_model);
  SetSelectionModel(std::move(new_model));
}

std::u16string TableView::GetTooltipText(const gfx::Point& p) const {
  const int row = p.y() / row_height_;
  if (row < 0 || static_cast<size_t>(row) >= GetRowCount() ||
      visible_columns_.empty()) {
    return std::u16string();
  }

  const int x = GetMirroredXInView(p.x());
  const std::optional<size_t> column = GetClosestVisibleColumnIndex(*this, x);
  if (!column.has_value() || x < visible_columns_[column.value()].x ||
      x > (visible_columns_[column.value()].x +
           visible_columns_[column.value()].width)) {
    return std::u16string();
  }

  const size_t model_row = ViewToModel(static_cast<size_t>(row));
  if (column.value() == 0 && !model_->GetTooltip(model_row).empty())
    return model_->GetTooltip(model_row);
  return model_->GetText(model_row, visible_columns_[column.value()].column.id);
}

bool TableView::HandleAccessibleAction(const ui::AXActionData& action_data) {
  const size_t row_count = GetRowCount();
  if (!row_count)
    return false;

  // On CrOS, the table wrapper node is not a AXVirtualView
  // and thus |ax_view| will be null.
  AXVirtualView* ax_view = AXVirtualView::GetFromId(action_data.target_node_id);
  bool focus_on_row =
      ax_view ? ax_view->GetData().role == ax::mojom::Role::kRow : false;

  size_t active_row = selection_model_.active().value_or(ModelToView(0));

  switch (action_data.action) {
    case ax::mojom::Action::kDoDefault:
      RequestFocus();
      if (focus_on_row) {
        // If the ax focus is on a row, select this row.
        DCHECK(ax_view);
        size_t row_index =
            base::checked_cast<size_t>(ax_view->GetData().GetIntAttribute(
                ax::mojom::IntAttribute::kTableRowIndex));
        SelectByViewIndex(row_index);
        GetViewAccessibility().AnnounceText(l10n_util::GetStringFUTF16(
            IDS_TABLE_VIEW_AX_ANNOUNCE_ROW_SELECTED,
            model_->GetText(ViewToModel(row_index),
                            GetVisibleColumn(0).column.id)));
      } else {
        // If the ax focus is on the full table, select the row as indicated by
        // the model.
        SelectByViewIndex(ModelToView(active_row));
        if (observer_)
          observer_->OnDoubleClick();
      }
      break;

    case ax::mojom::Action::kFocus:
      RequestFocus();
      // Setting focus should not affect the current selection.
      if (selection_model_.empty() && GetRowCount() > 0) {
        SelectByViewIndex(size_t{0});
      }
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

void TableView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // If the bounds change, we need to update the bounds of our AXVirtualView
  // children.
  UpdateVirtualAccessibilityChildrenVisibilityState();
}

void TableView::OnModelChanged() {
  selection_model_.Clear();
  GetViewAccessibility().SetTableRowCount(static_cast<int32_t>(GetRowCount()));
  RebuildVirtualAccessibilityChildren();
  PreferredSizeChanged();
}

void TableView::OnItemsChanged(size_t start, size_t length) {
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
}

void TableView::OnItemsAdded(size_t start, size_t length) {
  DCHECK_LE(start + length, GetRowCount());

  for (size_t i = 0; i < length; ++i) {
    // Increment selection model counter at start.
    selection_model_.IncrementFrom(start);

    // Append new virtual row to accessibility view.
    const size_t virtual_children_count =
        GetViewAccessibility().virtual_children().size();
    const size_t next_index =
        header_ ? virtual_children_count - 1 : virtual_children_count;
    GetViewAccessibility().AddVirtualChildView(
        CreateRowAccessibilityView(next_index));
  }

  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
  // This has to be done after updating the mapping.
  // If we don't do this, the view indices and the model indices will be out of
  // sync, since new AXVirtualViews were added. This will cause CHECKS to hit
  // when trying to access the model indices.
  UpdateVirtualAccessibilityChildrenVisibilityState();
  PreferredSizeChanged();
  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
}

void TableView::OnItemsMoved(size_t old_start,
                             size_t length,
                             size_t new_start) {
  selection_model_.Move(old_start, new_start, length);
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
}

void TableView::OnItemsRemoved(size_t start, size_t length) {
  // Determine the currently selected index in terms of the view. We inline the
  // implementation here since ViewToModel() has DCHECKs that fail since the
  // model has changed but |model_to_view_| has not been updated yet.
  const std::optional<size_t> previously_selected_model_index =
      GetFirstSelectedRow();
  std::optional<size_t> previously_selected_view_index =
      previously_selected_model_index;
  if (previously_selected_model_index.has_value() && GetIsSorted())
    previously_selected_view_index =
        model_to_view_[previously_selected_model_index.value()];
  for (size_t i = 0; i < length; ++i)
    selection_model_.DecrementFrom(start);

  // Update the `view_to_model_` and `model_to_view_` mappings prior to updating
  // TableView's virtual children below. We do this because at this point the
  // table model has changed but the model-view mappings have not yet been
  // updated to reflect this. `RemoveFromParentView()` below may trigger calls
  // back into TableView and this would happen before the model-view mappings
  // have been updated. This can result in memory overflow errors.
  // See (https://crbug.com/1173373).
  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
  if (GetIsSorted()) {
    DCHECK_EQ(GetRowCount(), view_to_model_.size());
    DCHECK_EQ(GetRowCount(), model_to_view_.size());
  }

  // If the selection was empty and is no longer empty select the same visual
  // index.
  if (selection_model_.empty() && previously_selected_view_index.has_value() &&
      GetRowCount() && select_on_remove_) {
    selection_model_.SetSelectedIndex(ViewToModel(
        std::min(GetRowCount() - 1, previously_selected_view_index.value())));
    // `ListSelectionModel::SetSelectedIndex` clears the selection and selects
    // only the specified index.
    ClearAccessibleSelection();
    SetAccessibleSelectionForIndex(
        std::min(GetRowCount() - 1, previously_selected_view_index.value()),
        /* selected */ true);
  }
  if (!selection_model_.empty()) {
    const size_t selected_model_index =
        *selection_model_.selected_indices().begin();
    if (!selection_model_.active().has_value())
      selection_model_.set_active(selected_model_index);
    if (!selection_model_.anchor().has_value())
      selection_model_.set_anchor(selected_model_index);
  }

  // Remove the virtual views that are no longer needed.
  auto& virtual_children = GetViewAccessibility().virtual_children();
  for (size_t i = start; !virtual_children.empty() && i < start + length; i++)
    virtual_children[virtual_children.size() - 1]->RemoveFromParentView();

  GetViewAccessibility().SetTableRowCount(static_cast<int32_t>(GetRowCount()));
  UpdateVirtualAccessibilityChildrenBounds();
  PreferredSizeChanged();
  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
  if (observer_)
    observer_->OnSelectionChanged();
}

gfx::Point TableView::GetKeyboardContextMenuLocation() {
  std::optional<size_t> first_selected = GetFirstSelectedRow();
  gfx::Rect vis_bounds(GetVisibleBounds());
  int y = vis_bounds.height() / 2;
  if (first_selected.has_value()) {
    gfx::Rect cell_bounds(GetRowBounds(first_selected.value()));
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
  UpdateFocusRings();
  ScheduleUpdateAccessibilityFocusIfNeeded();
}

void TableView::OnBlur() {
  SchedulePaintForSelection();
  UpdateFocusRings();
  ScheduleUpdateAccessibilityFocusIfNeeded();
}

void TableView::OnPaint(gfx::Canvas* canvas) {
  OnPaintImpl(canvas);
}

void TableView::OnPaintImpl(gfx::Canvas* canvas) {
  // Don't invoke View::OnPaint so that we can render our own focus border.

  if (sort_on_paint_)
    SortItemsAndUpdateMapping(/*schedule_paint=*/false);

  ui::ColorProvider* color_provider = GetColorProvider();
  const SkColor default_bg_color =
      color_provider->GetColor(ui::kColorTableBackground);
  canvas->DrawColor(default_bg_color);

  if (!GetRowCount() || visible_columns_.empty())
    return;

  const PaintRegion region(GetPaintRegion(GetPaintBounds(canvas)));
  if (region.min_column == visible_columns_.size())
    return;  // No need to paint anything.

  const SkColor selected_bg_color =
      color_provider->GetColor(text_background_color_id(HasFocus()));
  const SkColor fg_color = color_provider->GetColor(ui::kColorTableForeground);
  const SkColor selected_fg_color =
      color_provider->GetColor(selected_text_color_id(HasFocus()));
  const SkColor alternate_bg_color =
      color_provider->GetColor(ui::kColorTableBackgroundAlternate);
  const int cell_margin = GetCellMargin();
  const int cell_element_spacing = GetCellElementSpacing();
  for (size_t i = region.min_row; i < region.max_row; ++i) {
    const size_t model_index = ViewToModel(i);
    const bool is_selected = selection_model_.IsSelected(model_index);
    if (is_selected)
      canvas->FillRect(GetRowBounds(i), selected_bg_color);
    else if (alternate_bg_color != default_bg_color && (i % 2))
      canvas->FillRect(GetRowBounds(i), alternate_bg_color);
    for (size_t j = region.min_column; j < region.max_column; ++j) {
      const gfx::Rect cell_bounds = GetCellBounds(i, j);
      gfx::Rect text_bounds = cell_bounds;
      text_bounds.Inset(gfx::Insets::VH(0, cell_margin));

      // Provide space for the grouping indicator, but draw it separately.
      if (j == 0 && grouper_) {
        text_bounds.Inset(gfx::Insets().set_left(kGroupingIndicatorSize +
                                                 cell_element_spacing));
      }

      // Always paint the icon in the first visible column.
      if (j == 0 && table_type_ == TableType::kIconAndText) {
        gfx::ImageSkia image =
            model_->GetIcon(model_index).Rasterize(GetColorProvider());
        if (!image.isNull()) {
          // Need to check the area where the icon is paint. And it is necessary
          // to consider the UI layout direction.
          gfx::Rect dest_image_bounds =
              GetPaintIconDestBounds(cell_bounds, text_bounds.x());
          // The area does not have a width drawing icon.
          if (!dest_image_bounds.IsEmpty()) {
            gfx::Rect src_image_bounds =
                GetPaintIconSrcBounds(image.size(), dest_image_bounds.width());
            canvas->DrawImageInt(
                image, src_image_bounds.x(), src_image_bounds.y(),
                src_image_bounds.width(), src_image_bounds.height(),
                dest_image_bounds.x(), dest_image_bounds.y(),
                dest_image_bounds.width(), dest_image_bounds.height(), true);
          }
        }
        text_bounds.Inset(gfx::Insets().set_left(ui::TableModel::kIconSize +
                                                 cell_element_spacing));
      }

      // Paint text if there is still room for it after all that insetting.
      if (!text_bounds.IsEmpty()) {
        canvas->DrawStringRectWithFlags(
            model_->GetText(model_index, visible_columns_[j].column.id),
            font_list_, is_selected ? selected_fg_color : fg_color,
            GetMirroredRect(text_bounds),
            TableColumnAlignmentToCanvasAlignment(
                GetMirroredTableColumnAlignment(
                    visible_columns_[j].column.alignment)));
      }
    }
  }

  if (!grouper_ || region.min_column > 0)
    return;

  const SkColor grouping_color =
      color_provider->GetColor(ui::kColorTableGroupingIndicator);
  cc::PaintFlags grouping_flags;
  grouping_flags.setColor(grouping_color);
  grouping_flags.setStyle(cc::PaintFlags::kFill_Style);
  grouping_flags.setStrokeWidth(kGroupingIndicatorSize);
  grouping_flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  grouping_flags.setAntiAlias(true);
  const int group_indicator_x = GetMirroredXInView(
      GetCellBounds(0, 0).x() + cell_margin + kGroupingIndicatorSize / 2);
  for (size_t i = region.min_row; i < region.max_row;) {
    const size_t model_index = ViewToModel(i);
    GroupRange range;
    grouper_->GetGroupRange(model_index, &range);
    DCHECK_GT(range.length, 0u);
    // The order of rows in a group is consistent regardless of sort, so it's ok
    // to do this calculation.
    const size_t start = i - (model_index - range.start);
    const size_t last = start + range.length - 1;
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


void TableView::SortItemsAndUpdateMapping(bool schedule_paint) {
  const size_t row_count = GetRowCount();

  if (!GetIsSorted()) {
    view_to_model_.clear();
    model_to_view_.clear();

    // If we didn't sort the items, we still need to update the accessible name
    // for the entire table, since the mappings might've changed either way.
    UpdateAccessibleNameForIndex(0, GetRowCount());
  } else {
    view_to_model_.resize(row_count);
    model_to_view_.resize(row_count);

    // Resets the mapping so it can be sorted again.
    for (size_t view_index = 0; view_index < row_count; ++view_index)
      view_to_model_[view_index] = view_index;

    if (grouper_) {
      GroupSortHelper sort_helper(this);
      GetModelIndexToRangeStart(grouper_, row_count,
                                &sort_helper.model_index_to_range_start);
      std::stable_sort(view_to_model_.begin(), view_to_model_.end(),
                       sort_helper);
    } else {
      std::stable_sort(view_to_model_.begin(), view_to_model_.end(),
                       SortHelper(this));
    }

    for (size_t view_index = 0; view_index < row_count; ++view_index) {
      model_to_view_[view_to_model_[view_index]] = view_index;

      // After sorting and updating the mappings, we need to recompute the
      // accessible name for every index.
      UpdateAccessibleNameForIndex(view_index, 1);
    }

    model_->ClearCollator();
  }

  GetViewAccessibility().SetTableRowCount(static_cast<int32_t>(GetRowCount()));
  UpdateVirtualAccessibilityChildrenBounds();

  if (schedule_paint)
    SchedulePaint();
}

int TableView::CompareRows(size_t model_row1, size_t model_row2) {
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

gfx::Rect TableView::GetRowBounds(size_t row) const {
  return gfx::Rect(0, static_cast<int>(row) * row_height_, width(),
                   row_height_);
}

gfx::Rect TableView::GetCellBounds(size_t row,
                                   size_t visible_column_index) const {
  if (!header_)
    return GetRowBounds(row);
  const VisibleColumn& vis_col(visible_columns_[visible_column_index]);
  return gfx::Rect(vis_col.x, static_cast<int>(row) * row_height_,
                   vis_col.width, row_height_);
}

gfx::Rect TableView::GetActiveCellBounds() const {
  if (!selection_model_.active().has_value())
    return gfx::Rect();
  return GetCellBounds(ModelToView(selection_model_.active().value()),
                       active_visible_column_index_.value());
}

void TableView::AdjustCellBoundsForText(size_t visible_column_index,
                                        gfx::Rect* bounds) const {
  const int cell_margin = GetCellMargin();
  const int cell_element_spacing = GetCellElementSpacing();
  int text_x = cell_margin + bounds->x();
  if (visible_column_index == 0) {
    if (grouper_)
      text_x += kGroupingIndicatorSize + cell_element_spacing;
    if (table_type_ == TableType::kIconAndText) {
      text_x += ui::TableModel::kIconSize + cell_element_spacing;
    }
  }
  bounds->set_x(text_x);
  bounds->set_width(std::max(0, bounds->right() - cell_margin - text_x));
}

void TableView::CreateHeaderIfNecessary(ScrollView* scroll_view) {
  // Only create a header if there is more than one column, or the title of the
  // only column is not empty.
  if (header_ || (columns_.size() == 1 && columns_[0].title.empty())) {
    return;
  }

  header_ = scroll_view->SetHeader(
      std::make_unique<TableHeader>(weak_factory_.GetWeakPtr()));

  // The header accessibility view should be the first row, to match the
  // original view accessibility construction.
  GetViewAccessibility().AddVirtualChildViewAt(CreateHeaderAccessibilityView(),
                                               0);
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
  if (table_type_ == TableType::kIconAndText && header_) {
    first_column_padding += ui::TableModel::kIconSize + cell_element_spacing;
  }
  if (grouper_)
    first_column_padding += kGroupingIndicatorSize + cell_element_spacing;

  std::vector<int> sizes = views::CalculateTableColumnSizes(
      layout_width_, first_column_padding, header_->font_list(), font_list_,
      std::max(cell_margin, header_->GetHorizontalPadding()) * 2,
      header_->GetSortIndicatorWidth(), columns, model_);
  DCHECK_EQ(visible_columns_.size(), sizes.size());
  int x = 0;
  for (size_t i = 0; i < visible_columns_.size(); ++i) {
    visible_columns_[i].x = x;
    visible_columns_[i].width = sizes[i];
    x += sizes[i];
  }
}

// The default drawing size for icons in a table view is 16 * 16. If the cell
// size is not sufficient, the original image needs to be clipped. e.g if the
// original image size is 32 * 32, the normal bounds would be src bounds (0, 0,
// 32, 32) and dest bounds (x, y, 16, 16). If the dest bounds are (x, y, 8, 16),
// the original image needs to be clipped to prevent stretching during drawing.
// For LTR (left-to-right) layout, the src bounds would be (0, 0, 16, 32), and
// the width would be calculated as width = image_size.width() *
// image_dest_width / ui::TableModel::kIconSize.
// For RTL (right-to-left) layout, the src bounds would be (16, 0, 16, 32),
// and the `x` would be calculated as x = image_size.width() - src_image_width.
// (https://crbug.com/1494675)
gfx::Rect TableView::GetPaintIconSrcBounds(const gfx::Size& image_size,
                                           int image_dest_width) const {
  int src_image_x = 0;
  int src_image_width =
      image_size.width() * image_dest_width / ui::TableModel::kIconSize;
  if (GetMirrored()) {
    src_image_x = image_size.width() - src_image_width;
  }
  return gfx::Rect(src_image_x, 0, src_image_width, image_size.height());
}

gfx::Rect TableView::GetPaintIconDestBounds(const gfx::Rect& cell_bounds,
                                            int text_bounds_x) const {
  int dest_image_x =
      GetMirroredXWithWidthInView(text_bounds_x, ui::TableModel::kIconSize);
  int dest_image_width = ui::TableModel::kIconSize;
  gfx::Rect mirrored_cell_bounds = GetMirroredRect(cell_bounds);
  if (GetMirrored()) {
    if (dest_image_x < mirrored_cell_bounds.x()) {
      dest_image_width =
          dest_image_x + dest_image_width - mirrored_cell_bounds.x();
      dest_image_x = mirrored_cell_bounds.x();
    }
  } else {
    if (dest_image_x + dest_image_width > mirrored_cell_bounds.right()) {
      dest_image_width = mirrored_cell_bounds.right() - dest_image_x;
    }
  }
  return gfx::Rect(
      dest_image_x,
      cell_bounds.y() + (cell_bounds.height() - ui::TableModel::kIconSize) / 2,
      dest_image_width > 0 ? dest_image_width : 0, ui::TableModel::kIconSize);
}

TableView::PaintRegion TableView::GetPaintRegion(
    const gfx::Rect& bounds) const {
  DCHECK(!visible_columns_.empty());
  DCHECK(GetRowCount());

  PaintRegion region;
  region.min_row = static_cast<size_t>(
      std::clamp(bounds.y() / row_height_, 0,
                 base::saturated_cast<int>(GetRowCount() - 1)));
  region.max_row = static_cast<size_t>(bounds.bottom() / row_height_);
  if (bounds.bottom() % row_height_ != 0)
    region.max_row++;
  region.max_row = std::min(region.max_row, GetRowCount());

  if (!header_) {
    region.max_column = 1;
    return region;
  }

  const int paint_x = GetMirroredXForRect(bounds);
  const int paint_max_x = paint_x + bounds.width();
  region.min_column = region.max_column = visible_columns_.size();
  for (size_t i = 0; i < visible_columns_.size(); ++i) {
    int max_x = visible_columns_[i].x + visible_columns_[i].width;
    if (region.min_column == visible_columns_.size() && max_x >= paint_x)
      region.min_column = i;
    if (region.min_column != visible_columns_.size() &&
        visible_columns_[i].x >= paint_max_x) {
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
    const std::optional<size_t> first_model_row = GetFirstSelectedRow();
    SchedulePaintInRect(GetRowBounds(ModelToView(first_model_row.value())));

    if (selection_model_.active().has_value() &&
        first_model_row != selection_model_.active().value()) {
      SchedulePaintInRect(
          GetRowBounds(ModelToView(selection_model_.active().value())));
    }
  } else if (selection_model_.size() > 1) {
    SchedulePaint();
  }
}

ui::TableColumn TableView::FindColumnByID(int id) const {
  const auto i = base::ranges::find(columns_, id, &ui::TableColumn::id);
  DCHECK(i != columns_.cend());
  return *i;
}

void TableView::AdvanceActiveVisibleColumn(AdvanceDirection direction) {
  if (visible_columns_.empty()) {
    SetActiveVisibleColumnIndex(std::nullopt);
    return;
  }

  if (!active_visible_column_index_.has_value()) {
    if (!selection_model_.active().has_value() && !header_row_is_active_ &&
        GetRowCount()) {
      SelectByViewIndex(size_t{0});
    }
    SetActiveVisibleColumnIndex(size_t{0});
    return;
  }

  if (direction == AdvanceDirection::kDecrement) {
    SetActiveVisibleColumnIndex(
        std::max(size_t{1}, active_visible_column_index_.value()) - 1);
  } else {
    SetActiveVisibleColumnIndex(std::min(
        visible_columns_.size() - 1, active_visible_column_index_.value() + 1));
  }
}

std::optional<size_t> TableView::GetActiveVisibleColumnIndex() const {
  return active_visible_column_index_;
}

void TableView::SetActiveVisibleColumnIndex(std::optional<size_t> index) {
  if (active_visible_column_index_ == index) {
    return;
  }
  active_visible_column_index_ = index;
  if (active_visible_column_index_.has_value()) {
    if (selection_model_.active().has_value()) {
      ScrollRectToVisible(
          GetCellBounds(ModelToView(selection_model_.active().value()),
                        active_visible_column_index_.value()));
    } else if (header_row_is_active()) {
      const TableView::VisibleColumn& column =
          GetVisibleColumn(active_visible_column_index_.value());
      ScrollRectToVisible(gfx::Rect(column.x, 0, column.width, height()));
    }
    UpdateAccessibleSelectionForColumnIndex(
        active_visible_column_index_.value());
  }

  UpdateFocusRings();
  ScheduleUpdateAccessibilityFocusIfNeeded();
  OnPropertyChanged(&active_visible_column_index_, kPropertyEffectsNone);
}

void TableView::SelectByViewIndex(std::optional<size_t> view_index) {
  ui::ListSelectionModel new_selection;
  if (view_index.has_value()) {
    CHECK_LE(view_index.value(), GetRowCount());
    SelectRowsInRangeFrom(view_index.value(), true, &new_selection);
    new_selection.set_anchor(ViewToModel(view_index.value()));
    new_selection.set_active(ViewToModel(view_index.value()));
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
  if (selection_model_.active().has_value()) {
    gfx::Rect vis_rect(GetMirroredRect(GetVisibleBounds()));
    const GroupRange range(GetGroupRange(selection_model_.active().value()));
    const int start_y = GetRowBounds(ModelToView(range.start)).y();
    const int end_y =
        GetRowBounds(ModelToView(range.start + range.length - 1)).bottom();
    vis_rect.set_y(start_y);
    vis_rect.set_height(end_y - start_y);
    ScrollRectToVisible(vis_rect);

    if (!active_visible_column_index_.has_value())
      SetActiveVisibleColumnIndex(size_t{0});
  } else if (!header_row_is_active_) {
    SetActiveVisibleColumnIndex(std::nullopt);
  }

  UpdateFocusRings();
  ScheduleUpdateAccessibilityFocusIfNeeded();
  if (observer_)
    observer_->OnSelectionChanged();
}

void TableView::AdvanceSelection(AdvanceDirection direction) {
  if (!selection_model_.active().has_value()) {
    bool make_header_active =
        header_ && direction == AdvanceDirection::kDecrement;
    header_row_is_active_ = make_header_active;
    if (make_header_active) {
      SelectByViewIndex(std::nullopt);
    } else if (GetRowCount() > 0) {
      SelectByViewIndex(std::make_optional(size_t{0}));
    }
    UpdateFocusRings();
    ScheduleUpdateAccessibilityFocusIfNeeded();
    return;
  }
  size_t active_index = selection_model_.active().value();
  size_t view_index = ModelToView(active_index);
  const GroupRange range(GetGroupRange(active_index));
  size_t view_range_start = ModelToView(range.start);
  if (direction == AdvanceDirection::kDecrement) {
    bool make_header_active = header_ && view_index == 0;
    header_row_is_active_ = make_header_active;
    SelectByViewIndex(
        make_header_active
            ? std::nullopt
            : std::make_optional(std::max(size_t{1}, view_range_start) - 1));
  } else {
    header_row_is_active_ = false;
    SelectByViewIndex(
        std::min(GetRowCount() - 1, view_range_start + range.length));
  }
}

void TableView::ConfigureSelectionModelForEvent(
    const ui::LocatedEvent& event,
    ui::ListSelectionModel* model) const {
  const int view_index_int = event.y() / row_height_;
  DCHECK_GE(view_index_int, 0);
  const size_t view_index = static_cast<size_t>(view_index_int);
  DCHECK_LT(view_index, GetRowCount());

  if (!selection_model_.anchor().has_value() || single_selection_ ||
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
    DCHECK(model->anchor().has_value());
    const size_t anchor_index = ModelToView(model->anchor().value());
    const auto [min, max] = std::minmax(view_index, anchor_index);
    for (size_t i = min; i <= max; ++i)
      SelectRowsInRangeFrom(i, true, model);
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

void TableView::SelectRowsInRangeFrom(size_t view_index,
                                      bool select,
                                      ui::ListSelectionModel* model) const {
  const GroupRange range(GetGroupRange(ViewToModel(view_index)));
  for (size_t i = 0; i < range.length; ++i) {
    if (select) {
      model->AddIndexToSelection(range.start + i);
      SetAccessibleSelectionForIndex(ModelToView(range.start + i),
                                     /* selected */ true);
    } else {
      model->RemoveIndexFromSelection(range.start + i);
      SetAccessibleSelectionForIndex(ModelToView(range.start + i),
                                     /* selected */ false);
    }
  }
}

GroupRange TableView::GetGroupRange(size_t model_index) const {
  GroupRange range;
  if (grouper_) {
    grouper_->GetGroupRange(model_index, &range);
  } else {
    range.start = model_index;
    range.length = 1;
  }
  return range;
}

void TableView::RebuildVirtualAccessibilityChildren() {
  ClearVirtualAccessibilityChildren();

  if (!GetRowCount()) {
    return;
  }

  if (header_)
    GetViewAccessibility().AddVirtualChildView(CreateHeaderAccessibilityView());

  // Create a virtual accessibility view for each row. At this point on, the
  // table has no sort behavior, hence the view index is the same as the model
  // index, the sorting will happen at the end.
  for (size_t index = 0; index < GetRowCount(); ++index) {
    GetViewAccessibility().AddVirtualChildView(
        CreateRowAccessibilityView(index));
    UpdateAccessibleNameForIndex(/* start */ index, /* length */ 1);
  }

  SortItemsAndUpdateMapping(/*schedule_paint=*/true);
  // This has to be done after updating the mapping.
  // If we don't do this, the view indices and the model indices will be out of
  // sync, since new AXVirtualViews were added. This will cause CHECKS to hit
  // when trying to access the model indices.
  UpdateVirtualAccessibilityChildrenVisibilityState();
  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
}

void TableView::UpdateAccessibleNameForIndex(size_t start_view_index,
                                             size_t length) {
  if (start_view_index >= GetRowCount() ||
      start_view_index + length > GetRowCount()) {
    return;
  }

  for (size_t view_index = start_view_index;
       view_index < start_view_index + length; ++view_index) {
    AXVirtualView* ax_row = GetVirtualAccessibilityBodyRow(view_index);
    CHECK(ax_row);

    size_t model_index = ViewToModel(view_index);

    // We only need to update the name if the column is visible.
    if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell &&
        visible_columns_.size()) {
      ax_row->GetCustomData().SetName(
          model_->GetText(model_index, GetVisibleColumn(0).column.id));
    }

    for (auto& ax_cell : ax_row->children()) {
      auto column_index = ax_row->GetIndexOf(ax_cell.get());
      // Once we find the first non-visible column, we can break out of the
      // loop.
      if (column_index.value() >= visible_columns_.size()) {
        break;
      }
      ui::AXNodeData& cell_data = ax_cell->GetCustomData();

      std::u16string current_name = base::UTF8ToUTF16(
          cell_data.GetStringAttribute(ax::mojom::StringAttribute::kName));
      std::u16string new_name = model()->GetText(
          model_index, GetVisibleColumn(column_index.value()).column.id);
      if (current_name != new_name) {
        cell_data.SetName(new_name);
        ax_cell->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged);
      }
    }
  }
}

void TableView::ClearVirtualAccessibilityChildren() {
  GetViewAccessibility().RemoveAllVirtualChildViews();
}

void TableView::UpdateVirtualAccessibilityChildrenVisibilityState() {
  for (size_t i = 0; i < GetRowCount(); ++i) {
    AXVirtualView* ax_row = GetVirtualAccessibilityBodyRow(i);
    CHECK(ax_row);

    auto ax_index = GetViewAccessibility().GetIndexOf(ax_row);
    size_t row_index = ax_index.value() - (header_ ? 1 : 0);
    size_t model_index = ViewToModel(row_index);

    gfx::Rect row_bounds = GetRowBounds(model_index);
    // TODO(crbug.com/325137417): This should be undone, its incorrect to set
    // the invisible state when view is offscreen. Once ViewsAX is finished we
    // should remove this and compute the IsOffscreen property from the bounds
    // of the view and its parent.
    if (!GetVisibleBounds().Intersects(row_bounds)) {
      ax_row->GetCustomData().AddState(ax::mojom::State::kInvisible);
    } else {
      ax_row->GetCustomData().RemoveState(ax::mojom::State::kInvisible);
    }
    for (auto& ax_cell : ax_row->children()) {
      auto column_index = ax_row->GetIndexOf(ax_cell.get());
      DCHECK(column_index.has_value());

      gfx::Rect cell_bounds = GetCellBounds(row_index, column_index.value());
      if (!GetVisibleBounds().Intersects(cell_bounds)) {
        ax_cell->GetCustomData().AddState(ax::mojom::State::kInvisible);
      } else {
        ax_cell->GetCustomData().RemoveState(ax::mojom::State::kInvisible);
      }
    }
  }
}

void TableView::SetAccessibleSelectionForIndex(size_t view_index,
                                               bool selected) const {
  DCHECK(view_index < GetRowCount());

  AXVirtualView* ax_row = GetVirtualAccessibilityBodyRow(view_index);
  CHECK(ax_row);

  // Select/Unselect the row.
  ax_row->GetCustomData().AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                           selected);

  // Select/Unselect the cell.
  if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    for (size_t cell = 0; cell < ax_row->children().size(); cell++) {
      if (cell == GetActiveVisibleColumnIndex()) {
        ax_row->children()[cell]->GetCustomData().AddBoolAttribute(
            ax::mojom::BoolAttribute::kSelected, selected);
      }
    }
  }
}

void TableView::SetAccessibleSelectionForRange(size_t start_view_index,
                                               size_t end_view_index,
                                               bool selected) const {
  DCHECK_LE(start_view_index, end_view_index);
  DCHECK_LT(end_view_index, GetRowCount());

  for (size_t i = start_view_index; i <= end_view_index; ++i) {
    SetAccessibleSelectionForIndex(i, selected);
  }
}

void TableView::ClearAccessibleSelection() const {
  for (size_t row = 0; row < GetRowCount(); ++row) {
    SetAccessibleSelectionForIndex(row, false);
  }
}

void TableView::UpdateAccessibleSelectionForColumnIndex(
    size_t visible_column_index) const {
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell ||
      visible_column_index >= visible_columns_.size()) {
    return;
  }

  for (size_t row = 0; row < GetRowCount(); ++row) {
    if (selection_model().IsSelected(ViewToModel(row))) {
      GetVirtualAccessibilityCell(row, visible_column_index)
          ->GetCustomData()
          .AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
    }
  }
}

std::unique_ptr<AXVirtualView> TableView::CreateRowAccessibilityView(
    size_t row_index) {
  auto ax_row = std::make_unique<AXVirtualView>();

  ui::AXNodeData& row_data = ax_row->GetCustomData();
  row_data.role = ax::mojom::Role::kRow;

  row_data.AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex,
                           static_cast<int32_t>(row_index));

  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    row_data.AddState(ax::mojom::State::kFocusable);
    row_data.AddAction(ax::mojom::Action::kFocus);
    row_data.AddAction(ax::mojom::Action::kScrollToMakeVisible);
    row_data.AddAction(ax::mojom::Action::kSetSelection);
  }

  row_data.SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kSelect);
  if (!single_selection_)
    row_data.AddState(ax::mojom::State::kMultiselectable);

  for (size_t visible_column_index = 0;
       visible_column_index < visible_columns_.size(); ++visible_column_index) {
    std::unique_ptr<AXVirtualView> ax_cell =
        CreateCellAccessibilityView(row_index, visible_column_index);
    ax_row->AddChildView(std::move(ax_cell));
  }

  return ax_row;
}

std::unique_ptr<AXVirtualView> TableView::CreateCellAccessibilityView(
    size_t row_index,
    size_t column_index) {
  const VisibleColumn& visible_column = visible_columns_[column_index];
  const ui::TableColumn column = visible_column.column;
  auto ax_cell = std::make_unique<AXVirtualView>();
  ui::AXNodeData& cell_data = ax_cell->GetCustomData();
  cell_data.role = ax::mojom::Role::kGridCell;

  cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex,
                            static_cast<int32_t>(row_index));

  if (PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    cell_data.AddState(ax::mojom::State::kFocusable);
    cell_data.AddAction(ax::mojom::Action::kFocus);
    cell_data.AddAction(ax::mojom::Action::kScrollLeft);
    cell_data.AddAction(ax::mojom::Action::kScrollRight);
    cell_data.AddAction(ax::mojom::Action::kScrollToMakeVisible);
    cell_data.AddAction(ax::mojom::Action::kSetSelection);
  }

  cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, 1);
  cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex,
                            static_cast<int32_t>(column_index));
  cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan, 1);

  if (base::i18n::IsRTL())
    cell_data.SetTextDirection(ax::mojom::WritingDirection::kRtl);

  auto sort_direction = ax::mojom::SortDirection::kUnsorted;
  const std::optional<int> primary_sorted_column_id =
      sort_descriptors().empty()
          ? std::nullopt
          : std::make_optional(sort_descriptors()[0].column_id);

  if (column.sortable && primary_sorted_column_id.has_value() &&
      column.id == primary_sorted_column_id.value()) {
    sort_direction = GetFirstSortDescriptorDirection();
  }
  cell_data.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                            static_cast<int32_t>(sort_direction));

  return ax_cell;
}

void TableView::UpdateFocusRings() {
  views::FocusRing::Get(this)->SchedulePaint();
  if (header_)
    header_->UpdateFocusState();
}

std::unique_ptr<AXVirtualView> TableView::CreateHeaderAccessibilityView() {
  DCHECK(header_) << "header_ needs to be instantiated before setting its"
                     "accessibility view.";

  const std::optional<int> primary_sorted_column_id =
      sort_descriptors().empty()
          ? std::nullopt
          : std::make_optional(sort_descriptors()[0].column_id);

  auto ax_header = std::make_unique<AXVirtualView>();
  ui::AXNodeData& header_data = ax_header->GetCustomData();
  header_data.role = ax::mojom::Role::kRow;

  for (size_t visible_column_index = 0;
       visible_column_index < visible_columns_.size(); ++visible_column_index) {
    const VisibleColumn& visible_column =
        visible_columns_[visible_column_index];
    const ui::TableColumn column = visible_column.column;
    auto ax_cell = std::make_unique<AXVirtualView>();
    ui::AXNodeData& cell_data = ax_cell->GetCustomData();
    cell_data.role = ax::mojom::Role::kColumnHeader;
    cell_data.SetName(column.title);
    cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex,
                              static_cast<int32_t>(visible_column_index));
    cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan, 1);
    if (base::i18n::IsRTL()) {
      cell_data.SetTextDirection(ax::mojom::WritingDirection::kRtl);
    }

    auto sort_direction = ax::mojom::SortDirection::kUnsorted;
    if (column.sortable && primary_sorted_column_id.has_value() &&
        column.id == primary_sorted_column_id.value()) {
      sort_direction = GetFirstSortDescriptorDirection();
    }
    cell_data.AddIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                              static_cast<int32_t>(sort_direction));

    ax_header->AddChildView(std::move(ax_cell));
  }

  return ax_header;
}

bool TableView::UpdateVirtualAccessibilityRowData(AXVirtualView* ax_row,
                                                  int view_index,
                                                  int model_index) {
  DCHECK_GE(view_index, 0);

  ui::AXNodeData& row_data = ax_row->GetCustomData();

  int previous_view_index =
      row_data.GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex);
  if (previous_view_index == view_index)
    return false;

  row_data.AddIntAttribute(ax::mojom::IntAttribute::kTableRowIndex,
                           static_cast<int32_t>(view_index));

  // Update the cell's in the current row to have the new data.
  for (const auto& ax_cell : ax_row->children()) {
    ui::AXNodeData& cell_data = ax_cell->GetCustomData();
    cell_data.AddIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex,
                              static_cast<int32_t>(view_index));
  }

  return true;
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
    DCHECK_EQ(row_data.role, ax::mojom::Role::kRow);
    row_data.relative_bounds.bounds =
        gfx::RectF(CalculateHeaderRowAccessibilityBounds());

    // Update the bounds for every child cell in this row.
    for (size_t visible_column_index = 0;
         visible_column_index < ax_row->children().size();
         visible_column_index++) {
      ui::AXNodeData& cell_data =
          ax_row->children()[visible_column_index]->GetCustomData();
      DCHECK_EQ(cell_data.role, ax::mojom::Role::kColumnHeader);

      if (visible_column_index < visible_columns_.size()) {
        cell_data.relative_bounds.bounds = gfx::RectF(
            CalculateHeaderCellAccessibilityBounds(visible_column_index));
      } else {
        cell_data.relative_bounds.bounds = gfx::RectF();
      }
    }
  }

  // Update the bounds for the table's content rows.
  for (size_t row_index = 0; row_index < GetRowCount(); row_index++) {
    const size_t ax_row_index = header_ ? row_index + 1 : row_index;
    if (ax_row_index >= virtual_children.size())
      break;

    auto& ax_row = virtual_children[ax_row_index];
    ui::AXNodeData& row_data = ax_row->GetCustomData();
    DCHECK_EQ(row_data.role, ax::mojom::Role::kRow);
    row_data.relative_bounds.bounds =
        gfx::RectF(CalculateTableRowAccessibilityBounds(row_index));

    // Update the bounds for every child cell in this row.
    for (size_t visible_column_index = 0;
         visible_column_index < ax_row->children().size();
         visible_column_index++) {
      ui::AXNodeData& cell_data =
          ax_row->children()[visible_column_index]->GetCustomData();
      DCHECK_EQ(cell_data.role, ax::mojom::Role::kGridCell);

      if (visible_column_index < visible_columns_.size()) {
        cell_data.relative_bounds.bounds =
            gfx::RectF(CalculateTableCellAccessibilityBounds(
                row_index, visible_column_index));
      } else {
        cell_data.relative_bounds.bounds = gfx::RectF();
      }
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
    const size_t visible_column_index) const {
  const gfx::Rect& header_bounds = CalculateHeaderRowAccessibilityBounds();
  const VisibleColumn& visible_column = visible_columns_[visible_column_index];
  gfx::Rect header_cell_bounds(visible_column.x, header_bounds.y(),
                               visible_column.width, header_bounds.height());
  return header_cell_bounds;
}

gfx::Rect TableView::CalculateTableRowAccessibilityBounds(
    const size_t row_index) const {
  gfx::Rect row_bounds = GetRowBounds(row_index);
  return row_bounds;
}

gfx::Rect TableView::CalculateTableCellAccessibilityBounds(
    const size_t row_index,
    const size_t visible_column_index) const {
  gfx::Rect cell_bounds = GetCellBounds(row_index, visible_column_index);
  return cell_bounds;
}

void TableView::ScheduleUpdateAccessibilityFocusIfNeeded() {
  if (update_accessibility_focus_pending_)
    return;

  update_accessibility_focus_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&TableView::UpdateAccessibilityFocus,
                                weak_factory_.GetWeakPtr(),
                                UpdateAccessibilityFocusPassKey()));
}

void TableView::UpdateAccessibilityFocus(
    UpdateAccessibilityFocusPassKey pass_key) {
  DCHECK(update_accessibility_focus_pending_);
  update_accessibility_focus_pending_ = false;

  if (!HasFocus())
    return;

  if (header_ && header_row_is_active_) {
    AXVirtualView* ax_header_row = GetVirtualAccessibilityHeaderRow();
    if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell ||
        !active_visible_column_index_.has_value()) {
      if (ax_header_row) {
        ax_header_row->NotifyAccessibilityEvent(ax::mojom::Event::kSelection);
        GetViewAccessibility().OverrideFocus(ax_header_row);
      }
    } else {
      AXVirtualView* ax_header_cell = GetVirtualAccessibilityCellImpl(
          ax_header_row, active_visible_column_index_.value());
      if (ax_header_cell) {
        ax_header_cell->NotifyAccessibilityEvent(ax::mojom::Event::kSelection);
        GetViewAccessibility().OverrideFocus(ax_header_cell);
      }
    }
    return;
  }

  if (!selection_model_.active().has_value() ||
      !active_visible_column_index_.has_value()) {
    GetViewAccessibility().OverrideFocus(nullptr);
    return;
  }

  size_t active_row = ModelToView(selection_model_.active().value());
  AXVirtualView* ax_row = GetVirtualAccessibilityBodyRow(active_row);
  if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell) {
    if (ax_row) {
      ax_row->NotifyAccessibilityEvent(ax::mojom::Event::kSelection);
      GetViewAccessibility().OverrideFocus(ax_row);
    }
  } else {
    AXVirtualView* ax_cell = GetVirtualAccessibilityCellImpl(
        ax_row, active_visible_column_index_.value());
    if (ax_cell) {
      ax_cell->NotifyAccessibilityEvent(ax::mojom::Event::kSelection);
      GetViewAccessibility().OverrideFocus(ax_cell);
    }
  }
}

AXVirtualView* TableView::GetVirtualAccessibilityBodyRow(size_t row) const {
  DCHECK_LT(row, GetRowCount());
  if (header_)
    ++row;
  CHECK_LT(row, GetViewAccessibility().virtual_children().size())
      << "|row| not found. Did you forget to call "
         "RebuildVirtualAccessibilityChildren()?";

  const auto& ax_row = GetViewAccessibility().virtual_children()[row];
  DCHECK(ax_row);
  DCHECK_EQ(ax_row->GetData().role, ax::mojom::Role::kRow);
  return ax_row.get();
}

AXVirtualView* TableView::GetVirtualAccessibilityHeaderRow() {
  CHECK(header_) << "|row| not found. Did you forget to call "
                    "RebuildVirtualAccessibilityChildren()?";
  // The header row is always the first virtual child.
  const auto& ax_row = GetViewAccessibility().virtual_children()[size_t{0}];
  DCHECK(ax_row);
  DCHECK_EQ(ax_row->GetData().role, ax::mojom::Role::kRow);
  return ax_row.get();
}

AXVirtualView* TableView::GetVirtualAccessibilityCell(
    size_t row,
    size_t visible_column_index) const {
  return GetVirtualAccessibilityCellImpl(GetVirtualAccessibilityBodyRow(row),
                                         visible_column_index);
}

void TableView::SetHeaderStyle(const TableHeaderStyle& style) {
  header_style_ = style;
  if (header_) {
    UpdateVisibleColumnSizes();
    PreferredSizeChanged();
    SchedulePaint();
    header_->SchedulePaint();
  }
}

AXVirtualView* TableView::GetVirtualAccessibilityCellImpl(
    AXVirtualView* ax_row,
    size_t visible_column_index) const {
  DCHECK(ax_row) << "|row| not found. Did you forget to call "
                    "RebuildVirtualAccessibilityChildren()?";
  const auto matches_index = [visible_column_index](const auto& ax_cell) {
    DCHECK(ax_cell);
    DCHECK(ax_cell->GetData().role == ax::mojom::Role::kColumnHeader ||
           ax_cell->GetData().role == ax::mojom::Role::kGridCell);
    return base::checked_cast<size_t>(ax_cell->GetData().GetIntAttribute(
               ax::mojom::IntAttribute::kTableCellColumnIndex)) ==
           visible_column_index;
  };
  const auto i = base::ranges::find_if(ax_row->children(), matches_index);
  DCHECK(i != ax_row->children().cend())
      << "|visible_column_index| not found. Did you forget to call "
      << "RebuildVirtualAccessibilityChildren()?";
  return i->get();
}

BEGIN_METADATA(TableView)
ADD_READONLY_PROPERTY_METADATA(size_t, RowCount)
ADD_READONLY_PROPERTY_METADATA(std::optional<size_t>, FirstSelectedRow)
ADD_READONLY_PROPERTY_METADATA(bool, HasFocusIndicator)
ADD_PROPERTY_METADATA(std::optional<size_t>, ActiveVisibleColumnIndex)
ADD_READONLY_PROPERTY_METADATA(bool, IsSorted)
ADD_PROPERTY_METADATA(TableViewObserver*, Observer)
ADD_READONLY_PROPERTY_METADATA(int, RowHeight)
ADD_PROPERTY_METADATA(bool, SingleSelection)
ADD_PROPERTY_METADATA(bool, SelectOnRemove)
ADD_PROPERTY_METADATA(TableType, TableType)
ADD_PROPERTY_METADATA(bool, SortOnPaint)
END_METADATA

}  // namespace views

DEFINE_ENUM_CONVERTERS(views::TableType,
                       {views::TableType::kTextOnly, u"TEXT_ONLY"},
                       {views::TableType::kIconAndText, u"ICON_AND_TEXT"})
