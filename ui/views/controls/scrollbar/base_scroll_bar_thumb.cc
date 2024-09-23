// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/base_scroll_bar_thumb.h"

#include "base/i18n/rtl.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scrollbar/scroll_bar.h"

namespace {
// The distance the mouse can be dragged outside the bounds of the thumb during
// dragging before the scrollbar will snap back to its regular position.
static constexpr int kScrollThumbDragOutSnap = 100;
}  // namespace

namespace views {

BaseScrollBarThumb::BaseScrollBarThumb(ScrollBar* scroll_bar)
    : scroll_bar_(scroll_bar) {}

BaseScrollBarThumb::~BaseScrollBarThumb() = default;

void BaseScrollBarThumb::SetLength(int length) {
  // Make sure the thumb is never sized smaller than its minimum possible
  // display size.
  gfx::Size size = GetPreferredSize({});
  size.SetToMax(
      gfx::Size(IsHorizontal() ? length : 0, IsHorizontal() ? 0 : length));
  SetSize(size);
}

int BaseScrollBarThumb::GetLength() const {
  if (IsHorizontal())
    return width();
  return height();
}

void BaseScrollBarThumb::SetPosition(int position) {
  gfx::Rect thumb_bounds = bounds();
  gfx::Rect track_bounds = scroll_bar_->GetTrackBounds();
  if (IsHorizontal()) {
    thumb_bounds.set_x(track_bounds.x() + position);
  } else {
    thumb_bounds.set_y(track_bounds.y() + position);
  }
  SetBoundsRect(thumb_bounds);
}

int BaseScrollBarThumb::GetPosition() const {
  gfx::Rect track_bounds = scroll_bar_->GetTrackBounds();
  if (IsHorizontal())
    return x() - track_bounds.x();
  return y() - track_bounds.y();
}

void BaseScrollBarThumb::SetSnapBackOnDragOutside(bool snap) {
  snap_back_on_drag_outside_ = snap;
}

bool BaseScrollBarThumb::GetSnapBackOnDragOutside() const {
  return snap_back_on_drag_outside_;
}

void BaseScrollBarThumb::OnMouseEntered(const ui::MouseEvent& event) {
  SetState(Button::STATE_HOVERED);
}

void BaseScrollBarThumb::OnMouseExited(const ui::MouseEvent& event) {
  SetState(Button::STATE_NORMAL);
}

bool BaseScrollBarThumb::OnMousePressed(const ui::MouseEvent& event) {
  mouse_offset_ = IsHorizontal() ? event.x() : event.y();
  drag_start_position_ = GetPosition();
  SetState(Button::STATE_PRESSED);
  return true;
}

bool BaseScrollBarThumb::OnMouseDragged(const ui::MouseEvent& event) {
  if (snap_back_on_drag_outside_) {
    // If the user moves the mouse more than |kScrollThumbDragOutSnap| outside
    // the bounds of the thumb, the scrollbar will snap the scroll back to the
    // point it was at before the drag began.
    if (IsHorizontal()) {
      if ((event.y() < y() - kScrollThumbDragOutSnap) ||
          (event.y() > (y() + height() + kScrollThumbDragOutSnap))) {
        scroll_bar_->ScrollToThumbPosition(drag_start_position_, false);
        return true;
      }
    } else {
      if ((event.x() < x() - kScrollThumbDragOutSnap) ||
          (event.x() > (x() + width() + kScrollThumbDragOutSnap))) {
        scroll_bar_->ScrollToThumbPosition(drag_start_position_, false);
        return true;
      }
    }
  }
  if (IsHorizontal()) {
    int thumb_x = event.x() - mouse_offset_;
    if (base::i18n::IsRTL())
      thumb_x *= -1;
    scroll_bar_->ScrollToThumbPosition(GetPosition() + thumb_x, false);
  } else {
    int thumb_y = event.y() - mouse_offset_;
    scroll_bar_->ScrollToThumbPosition(GetPosition() + thumb_y, false);
  }
  return true;
}

void BaseScrollBarThumb::OnMouseReleased(const ui::MouseEvent& event) {
  SetState(HitTestPoint(event.location()) ? Button::STATE_HOVERED
                                          : Button::STATE_NORMAL);
}

void BaseScrollBarThumb::OnMouseCaptureLost() {
  SetState(Button::STATE_HOVERED);
}

Button::ButtonState BaseScrollBarThumb::GetState() const {
  return state_;
}

void BaseScrollBarThumb::SetState(Button::ButtonState state) {
  if (state_ == state)
    return;

  state_ = state;
  OnStateChanged();
}

void BaseScrollBarThumb::OnStateChanged() {
  SchedulePaint();
}

bool BaseScrollBarThumb::IsHorizontal() const {
  return scroll_bar_->GetOrientation() == ScrollBar::Orientation::kHorizontal;
}

BEGIN_METADATA(BaseScrollBarThumb)
ADD_PROPERTY_METADATA(bool, SnapBackOnDragOutside);
END_METADATA

}  // namespace views
