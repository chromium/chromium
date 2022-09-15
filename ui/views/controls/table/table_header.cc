// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/table/table_header.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/table/table_utils.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/style/platform_style.h"

namespace views {

namespace {

// The minimum width we allow a column to go down to.
constexpr int kMinColumnWidth = 10;

// Amount that a column is resized when using the keyboard.
constexpr int kResizeKeyboardAmount = 5;

constexpr int kVerticalPadding = 4;

// Distace from edge columns can be resized by.
constexpr int kResizePadding = 5;

// Amount of space above/below the separator.
constexpr int kSeparatorPadding = 4;

// Size of the sort indicator (doesn't include padding).
constexpr int kSortIndicatorSize = 8;

}  // namespace

// static
const int TableHeader::kHorizontalPadding = 7;

// static
const int TableHeader::kSortIndicatorWidth =
    kSortIndicatorSize + TableHeader::kHorizontalPadding * 2;

class TableHeader::HighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  HighlightPathGenerator() = default;
  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;
  ~HighlightPathGenerator() override = default;

  // HighlightPathGenerator:
  SkPath GetHighlightPath(const View* view) override {
    if (!PlatformStyle::kTableViewSupportsKeyboardNavigationByCell)
      return SkPath();

    const TableHeader* const header = static_cast<const TableHeader*>(view);
    // If there's no focus indicator fall back on the default highlight path
    // (highlights entire view instead of active cell).
    if (!header->HasFocusIndicator())
      return SkPath();

    // Draw a focus indicator around the active cell.
    gfx::Rect bounds = header->GetActiveHeaderCellBounds();
    bounds.set_x(header->GetMirroredXForRect(bounds));
    return SkPath().addRect(gfx::RectToSkRect(bounds));
  }
};

using Columns = std::vector<TableView::VisibleColumn>;

TableHeader::TableHeader(TableView* table) : table_(table) {
  HighlightPathGenerator::Install(
      this, std::make_unique<TableHeader::HighlightPathGenerator>());
  FocusRing::Install(this);
  views::FocusRing::Get(this)->SetHasFocusPredicate([&](View* view) {
    return static_cast<TableHeader*>(view)->GetHeaderRowHasFocus();
  });
}

TableHeader::~TableHeader() = default;

void TableHeader::UpdateFocusState() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void TableHeader::OnPaint(gfx::Canvas* canvas) {
  ui::ColorProvider* color_provider = GetColorProvider();
  const SkColor text_color =
      color_provider->GetColor(ui::kColorTableHeaderForeground);
  const SkColor separator_color =
      color_provider->GetColor(ui::kColorTableHeaderSeparator);
  // Paint the background and a separator at the bottom. The separator color
  // matches that of the border around the scrollview.
  OnPaintBackground(canvas);
  SkColor border_color =
      color_provider->GetColor(ui::kColorFocusableBorderUnfocused);
  canvas->DrawSharpLine(gfx::PointF(0, height() - 1),
                        gfx::PointF(width(), height() - 1), border_color);

  const Columns& columns = table_->visible_columns();
  const int sorted_column_id = table_->sort_descriptors().empty()
                                   ? -1
                                   : table_->sort_descriptors()[0].column_id;
  for (const auto& column : columns) {
    if (column.width >= 2) {
      const int separator_x = GetMirroredXInView(column.x + column.width - 1);
      canvas->DrawSharpLine(
          gfx::PointF(separator_x, kSeparatorPadding),
          gfx::PointF(separator_x, height() - kSeparatorPadding),
          separator_color);
    }

    const int x = column.x + kHorizontalPadding;
    int width = column.width - kHorizontalPadding - kHorizontalPadding;
    if (width <= 0)
      continue;

    const int title_width =
        gfx::GetStringWidth(column.column.title, font_list_);
    const bool paint_sort_indicator =
        (column.column.id == sorted_column_id &&
         title_width + kSortIndicatorWidth <= width);

    if (paint_sort_indicator)
      width -= kSortIndicatorWidth;

    canvas->DrawStringRectWithFlags(
        column.column.title, font_list_, text_color,
        gfx::Rect(GetMirroredXWithWidthInView(x, width), kVerticalPadding,
                  width, height() - kVerticalPadding * 2),
        TableColumnAlignmentToCanvasAlignment(
            GetMirroredTableColumnAlignment(column.column.alignment)));

    if (paint_sort_indicator) {
      cc::PaintFlags flags;
      flags.setColor(text_color);
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setAntiAlias(true);

      int indicator_x = 0;
      switch (column.column.alignment) {
        case ui::TableColumn::LEFT:
          indicator_x = x + title_width;
          break;
        case ui::TableColumn::CENTER:
          indicator_x = x + width / 2 + title_width / 2;
          break;
        case ui::TableColumn::RIGHT:
          indicator_x = x + width;
          break;
      }

      const int scale = base::i18n::IsRTL() ? -1 : 1;
      indicator_x += (kSortIndicatorWidth - kSortIndicatorSize) / 2;
      indicator_x = GetMirroredXInView(indicator_x);
      int indicator_y = height() / 2 - kSortIndicatorSize / 2;
      SkPath indicator_path;
      if (table_->sort_descriptors()[0].ascending) {
        indicator_path.moveTo(SkIntToScalar(indicator_x),
                              SkIntToScalar(indicator_y + kSortIndicatorSize));
        indicator_path.lineTo(
            SkIntToScalar(indicator_x + kSortIndicatorSize * scale),
            SkIntToScalar(indicator_y + kSortIndicatorSize));
        indicator_path.lineTo(
            SkIntToScalar(indicator_x + kSortIndicatorSize / 2 * scale),
            SkIntToScalar(indicator_y));
      } else {
        indicator_path.moveTo(SkIntToScalar(indicator_x),
                              SkIntToScalar(indicator_y));
        indicator_path.lineTo(
            SkIntToScalar(indicator_x + kSortIndicatorSize * scale),
            SkIntToScalar(indicator_y));
        indicator_path.lineTo(
            SkIntToScalar(indicator_x + kSortIndicatorSize / 2 * scale),
            SkIntToScalar(indicator_y + kSortIndicatorSize));
      }
      indicator_path.close();
      canvas->DrawPath(indicator_path, flags);
    }
  }
}

gfx::Size TableHeader::CalculatePreferredSize() const {
  return gfx::Size(1, kVerticalPadding * 2 + font_list_.GetHeight());
}

bool TableHeader::GetNeedsNotificationWhenVisibleBoundsChange() const {
  return true;
}

void TableHeader::OnVisibleBoundsChanged() {
  // Ensure the TableView updates its virtual children's bounds, because that
  // includes the bounds representing this TableHeader.
  table_->UpdateVirtualAccessibilityChildrenBounds();
}

void TableHeader::AddedToWidget() {
  // Ensure the TableView updates its virtual children's bounds, because that
  // includes the bounds representing this TableHeader.
  table_->UpdateVirtualAccessibilityChildrenBounds();
}

ui::Cursor TableHeader::GetCursor(const ui::MouseEvent& event) {
  return GetResizeColumn(GetMirroredXInView(event.x())).has_value()
             ? ui::mojom::CursorType::kColumnResize
             : View::GetCursor(event);
}

bool TableHeader::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    StartResize(event);
    return true;
  }

  // Return false so that context menus on ancestors work.
  return false;
}

bool TableHeader::OnMouseDragged(const ui::MouseEvent& event) {
  ContinueResize(event);
  return true;
}

void TableHeader::OnMouseReleased(const ui::MouseEvent& event) {
  const bool was_resizing = resize_details_ != nullptr;
  resize_details_.reset();
  if (!was_resizing && event.IsOnlyLeftMouseButton())
    ToggleSortOrder(event);
}

void TableHeader::OnMouseCaptureLost() {
  if (is_resizing()) {
    table_->SetVisibleColumnWidth(resize_details_->column_index,
                                  resize_details_->initial_width);
  }
  resize_details_.reset();
}

void TableHeader::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      if (!resize_details_.get())
        ToggleSortOrder(*event);
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      StartResize(*event);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      ContinueResize(*event);
      break;
    case ui::ET_GESTURE_SCROLL_END:
      resize_details_.reset();
      break;
    default:
      return;
  }
  event->SetHandled();
}

void TableHeader::OnThemeChanged() {
  View::OnThemeChanged();
  SetBackground(CreateSolidBackground(
      GetColorProvider()->GetColor(ui::kColorTableHeaderBackground)));
}

void TableHeader::ResizeColumnViaKeyboard(
    size_t index,
    TableView::AdvanceDirection direction) {
  const TableView::VisibleColumn& column = table_->GetVisibleColumn(index);
  const int needed_for_title =
      gfx::GetStringWidth(column.column.title, font_list_) +
      2 * kHorizontalPadding;

  int new_width = column.width;
  switch (direction) {
    case TableView::AdvanceDirection::kIncrement:
      new_width += kResizeKeyboardAmount;
      break;
    case TableView::AdvanceDirection::kDecrement:
      new_width -= kResizeKeyboardAmount;
      break;
  }

  table_->SetVisibleColumnWidth(
      index, std::max({kMinColumnWidth, needed_for_title, new_width}));
}

bool TableHeader::GetHeaderRowHasFocus() const {
  return table_->HasFocus() && table_->header_row_is_active();
}

gfx::Rect TableHeader::GetActiveHeaderCellBounds() const {
  const absl::optional<size_t> active_index =
      table_->GetActiveVisibleColumnIndex();
  DCHECK(active_index.has_value());
  const TableView::VisibleColumn& column =
      table_->GetVisibleColumn(active_index.value());
  return gfx::Rect(column.x, 0, column.width, height());
}

bool TableHeader::HasFocusIndicator() const {
  return table_->GetActiveVisibleColumnIndex().has_value();
}

bool TableHeader::StartResize(const ui::LocatedEvent& event) {
  if (is_resizing())
    return false;

  const absl::optional<size_t> index =
      GetResizeColumn(GetMirroredXInView(event.x()));
  if (!index.has_value())
    return false;

  resize_details_ = std::make_unique<ColumnResizeDetails>();
  resize_details_->column_index = index.value();
  resize_details_->initial_x = event.root_location().x();
  resize_details_->initial_width =
      table_->GetVisibleColumn(index.value()).width;
  return true;
}

void TableHeader::ContinueResize(const ui::LocatedEvent& event) {
  if (!is_resizing())
    return;

  const int scale = base::i18n::IsRTL() ? -1 : 1;
  const int delta =
      scale * (event.root_location().x() - resize_details_->initial_x);
  const TableView::VisibleColumn& column =
      table_->GetVisibleColumn(resize_details_->column_index);
  const int needed_for_title =
      gfx::GetStringWidth(column.column.title, font_list_) +
      2 * kHorizontalPadding;
  table_->SetVisibleColumnWidth(
      resize_details_->column_index,
      std::max({kMinColumnWidth, needed_for_title,
                resize_details_->initial_width + delta}));
}

void TableHeader::ToggleSortOrder(const ui::LocatedEvent& event) {
  if (table_->visible_columns().empty())
    return;

  const int x = GetMirroredXInView(event.x());
  const absl::optional<size_t> index = GetClosestVisibleColumnIndex(table_, x);
  if (!index.has_value())
    return;
  const TableView::VisibleColumn& column(
      table_->GetVisibleColumn(index.value()));
  if (x >= column.x && x < column.x + column.width && event.y() >= 0 &&
      event.y() < height()) {
    table_->ToggleSortOrder(index.value());
  }
}

absl::optional<size_t> TableHeader::GetResizeColumn(int x) const {
  const Columns& columns(table_->visible_columns());
  if (columns.empty())
    return absl::nullopt;

  const absl::optional<size_t> index = GetClosestVisibleColumnIndex(table_, x);
  DCHECK(index.has_value());
  const TableView::VisibleColumn& column(
      table_->GetVisibleColumn(index.value()));
  if (index.value() > 0 && x >= column.x - kResizePadding &&
      x <= column.x + kResizePadding) {
    return index.value() - 1;
  }
  const int max_x = column.x + column.width;
  return (x >= max_x - kResizePadding && x <= max_x + kResizePadding)
             ? absl::make_optional(index.value())
             : absl::nullopt;
}
BEGIN_METADATA(TableHeader, View)
END_METADATA

}  // namespace views
