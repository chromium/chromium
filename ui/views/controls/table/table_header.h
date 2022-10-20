// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_HEADER_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_HEADER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// Views used to render the header for the table.
class VIEWS_EXPORT TableHeader : public views::View {
 public:
  METADATA_HEADER(TableHeader);

  // Amount the text is padded on the left/right side.
  static const int kHorizontalPadding;

  // Amount of space reserved for the indicator and padding.
  static const int kSortIndicatorWidth;

  explicit TableHeader(TableView* table);
  TableHeader(const TableHeader&) = delete;
  TableHeader& operator=(const TableHeader&) = delete;
  ~TableHeader() override;

  const gfx::FontList& font_list() const { return font_list_; }

  void ResizeColumnViaKeyboard(size_t index,
                               TableView::AdvanceDirection direction);

  // Call to update TableHeader objects that rely on the focus state of its
  // corresponding virtual accessibility views.
  void UpdateFocusState();

  // views::View overrides.
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize() const override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void AddedToWidget() override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;

 private:
  class HighlightPathGenerator;

  // Used to track the column being resized.
  struct ColumnResizeDetails {
    ColumnResizeDetails() = default;

    // Index into table_->visible_columns() that is being resized.
    size_t column_index = 0;

    // X-coordinate of the mouse at the time the resize started.
    int initial_x = 0;

    // Width of the column when the drag started.
    int initial_width = 0;
  };

  // Returns true if the TableView's header has focus.
  bool GetHeaderRowHasFocus() const;

  // Gets the bounds of the currently active header cell.
  gfx::Rect GetActiveHeaderCellBounds() const;

  // Returns true if one of the TableHeader's cells has a focus indicator.
  bool HasFocusIndicator() const;

  // If not already resizing and |event| is over a resizable column starts
  // resizing.
  bool StartResize(const ui::LocatedEvent& event);

  // Continues a resize operation. Does nothing if not in the process of
  // resizing.
  void ContinueResize(const ui::LocatedEvent& event);

  // Toggles the sort order of the column at the location in |event|.
  void ToggleSortOrder(const ui::LocatedEvent& event);

  // Returns the column to resize given the specified x-coordinate, or nullopt
  // if |x| is not in the resize range of any columns.
  absl::optional<size_t> GetResizeColumn(int x) const;

  bool is_resizing() const { return resize_details_.get() != nullptr; }

  const gfx::FontList font_list_;

  raw_ptr<TableView, DanglingUntriaged> table_;

  // If non-null a resize is in progress.
  std::unique_ptr<ColumnResizeDetails> resize_details_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_HEADER_H_
