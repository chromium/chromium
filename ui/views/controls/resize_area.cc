// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/resize_area.h"

#include "base/i18n/rtl.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/resize_area_delegate.h"

namespace views {

ResizeArea::ResizeArea(ResizeAreaDelegate* delegate) : delegate_(delegate) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kSplitter);
}

ResizeArea::~ResizeArea() = default;

ui::Cursor ResizeArea::GetCursor(const ui::MouseEvent& event) {
  return GetEnabled()
             ? ui::Cursor(axis_ == Axis::kHorizontal
                              ? ui::mojom::CursorType::kEastWestResize
                              : ui::mojom::CursorType::kNorthSouthResize)
             : ui::Cursor();
}

void ResizeArea::OnGestureEvent(ui::GestureEvent* event) {
  auto event_position = axis_ == Axis::kHorizontal ? event->x() : event->y();
  if (event->type() == ui::EventType::kGestureTapDown) {
    SetInitialPosition(event_position);
    event->SetHandled();
  } else if (event->type() == ui::EventType::kGestureScrollBegin ||
             event->type() == ui::EventType::kGestureScrollUpdate) {
    ReportResizeAmount(event_position, false);
    event->SetHandled();
  } else if (event->type() == ui::EventType::kGestureEnd) {
    if (is_resizing_) {
      ReportResizeAmount(event_position, true);
    }
    event->SetHandled();
  }
}

bool ResizeArea::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton()) {
    return false;
  }

  SetInitialPosition(axis_ == Axis::kHorizontal ? event.x() : event.y());
  return true;
}

bool ResizeArea::OnMouseDragged(const ui::MouseEvent& event) {
  if (!event.IsLeftMouseButton()) {
    return false;
  }

  ReportResizeAmount(axis_ == Axis::kHorizontal ? event.x() : event.y(), false);
  return true;
}

void ResizeArea::OnMouseReleased(const ui::MouseEvent& event) {
  if (is_resizing_) {
    ReportResizeAmount(axis_ == Axis::kHorizontal ? event.x() : event.y(),
                       true);
  }
}

void ResizeArea::OnMouseCaptureLost() {
  ReportResizeAmount(initial_position_, true);
}

void ResizeArea::ReportResizeAmount(int resize_amount, bool last_update) {
  gfx::Point point = axis_ == Axis::kHorizontal ? gfx::Point(resize_amount, 0)
                                                : gfx::Point(0, resize_amount);
  View::ConvertPointToScreen(this, &point);
  resize_amount =
      (axis_ == Axis::kHorizontal ? point.x() : point.y()) - initial_position_;
  is_resizing_ = !last_update;
  delegate_->OnResize((axis_ == Axis::kHorizontal && base::i18n::IsRTL())
                          ? -resize_amount
                          : resize_amount,
                      last_update);
}

void ResizeArea::SetInitialPosition(int event_position) {
  gfx::Point point = axis_ == Axis::kHorizontal ? gfx::Point(event_position, 0)
                                                : gfx::Point(0, event_position);
  View::ConvertPointToScreen(this, &point);
  initial_position_ = (axis_ == Axis::kHorizontal ? point.x() : point.y());
}

BEGIN_METADATA(ResizeArea)
END_METADATA

}  // namespace views
