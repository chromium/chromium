// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_HEADER_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_HEADER_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// Views used to render the header for the table.
class VIEWS_EXPORT TableHeader : public views::View {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // Amount the text is padded on the left/right side.
  static const int kHorizontalPadding;

  // Amount of space reserved for the indicator and padding.
  static const int kSortIndicatorWidth;

  explicit TableHeader(TableView* table);
  ~TableHeader() override;

  const gfx::FontList& font_list() const { return font_list_; }

  void ResizeColumnViaKeyboard(int index,
                               TableView::AdvanceDirection direction);

  // views::View overrides.
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void AddedToWidget() override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;

 private:
  // Used to track the column being resized.
  struct ColumnResizeDetails {
    ColumnResizeDetails() = default;

    // Index into table_->visible_columns() that is being resized.
    int column_index = 0;

    // X-coordinate of the mouse at the time the resize started.
    int initial_x = 0;

    // Width of the column when the drag started.
    int initial_width = 0;
  };

  // If not already resizing and |event| is over a resizable column starts
  // resizing.
  bool StartResize(const ui::LocatedEvent& event);

  // Continues a resize operation. Does nothing if not in the process of
  // resizing.
  void ContinueResize(const ui::LocatedEvent& event);

  // Toggles the sort order of the column at the location in |event|.
  void ToggleSortOrder(const ui::LocatedEvent& event);

  // Returns the column to resize given the specified x-coordinate, or -1 if |x|
  // is not in the resize range of any columns.
  int GetResizeColumn(int x) const;

  bool is_resizing() const { return resize_details_.get() != nullptr; }

  const gfx::FontList font_list_;

  TableView* table_;

  // If non-null a resize is in progress.
  std::unique_ptr<ColumnResizeDetails> resize_details_;

  DISALLOW_COPY_AND_ASSIGN(TableHeader);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_HEADER_H_
