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
  return GetEnabled() ? ui::Cursor(ui::mojom::CursorType::kEastWestResize)
                      : ui::Cursor();
}

void ResizeArea::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTapDown) {
    SetInitialPosition(event->x());
    event->SetHandled();
  } else if (event->type() == ui::EventType::kGestureScrollBegin ||
             event->type() == ui::EventType::kGestureScrollUpdate) {
    ReportResizeAmount(event->x(), false);
    event->SetHandled();
  } else if (event->type() == ui::EventType::kGestureEnd) {
    ReportResizeAmount(event->x(), true);
    event->SetHandled();
  }
}

bool ResizeArea::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;

  SetInitialPosition(event.x());
  return true;
}

bool ResizeArea::OnMouseDragged(const ui::MouseEvent& event) {
  if (!event.IsLeftMouseButton())
    return false;

  ReportResizeAmount(event.x(), false);
  return true;
}

void ResizeArea::OnMouseReleased(const ui::MouseEvent& event) {
  ReportResizeAmount(event.x(), true);
}

void ResizeArea::OnMouseCaptureLost() {
  ReportResizeAmount(initial_position_, true);
}

void ResizeArea::ReportResizeAmount(int resize_amount, bool last_update) {
  gfx::Point point(resize_amount, 0);
  View::ConvertPointToScreen(this, &point);
  resize_amount = point.x() - initial_position_;
  delegate_->OnResize(base::i18n::IsRTL() ? -resize_amount : resize_amount,
                      last_update);
}

void ResizeArea::SetInitialPosition(int event_x) {
  gfx::Point point(event_x, 0);
  View::ConvertPointToScreen(this, &point);
  initial_position_ = point.x();
}

BEGIN_METADATA(ResizeArea)
END_METADATA

}  // namespace views
