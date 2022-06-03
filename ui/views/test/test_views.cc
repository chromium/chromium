// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_views.h"

#include "ui/events/event.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

namespace views {

StaticSizedView::StaticSizedView(const gfx::Size& preferred_size)
    // Default GetMinimumSize() is GetPreferredSize(). Default GetMaximumSize()
    // is 0x0.
    : preferred_size_(preferred_size), minimum_size_(preferred_size) {}

StaticSizedView::~StaticSizedView() = default;

gfx::Size StaticSizedView::CalculatePreferredSize() const {
  return preferred_size_;
}

gfx::Size StaticSizedView::GetMinimumSize() const {
  return minimum_size_;
}

gfx::Size StaticSizedView::GetMaximumSize() const {
  return maximum_size_;
}

ProportionallySizedView::ProportionallySizedView(int factor)
    : factor_(factor), preferred_width_(-1) {}

ProportionallySizedView::~ProportionallySizedView() = default;

void ProportionallySizedView::SetPreferredWidth(int width) {
  preferred_width_ = width;
  PreferredSizeChanged();
}

int ProportionallySizedView::GetHeightForWidth(int w) const {
  return w * factor_;
}

gfx::Size ProportionallySizedView::CalculatePreferredSize() const {
  if (preferred_width_ >= 0)
    return gfx::Size(preferred_width_, GetHeightForWidth(preferred_width_));
  return View::CalculatePreferredSize();
}

CloseWidgetView::CloseWidgetView(ui::EventType event_type)
    : event_type_(event_type) {}

void CloseWidgetView::OnEvent(ui::Event* event) {
  if (event->type() == event_type_) {
    // Go through NativeWidgetPrivate to simulate what happens if the OS
    // deletes the NativeWindow out from under us.
    // TODO(tapted): Change this to ViewsTestBase::SimulateNativeDestroy for a
    // more authentic test on Mac.
    GetWidget()->native_widget_private()->CloseNow();
  } else {
    View::OnEvent(event);
    if (!event->IsTouchEvent())
      event->SetHandled();
  }
}

EventCountView::EventCountView() = default;

EventCountView::~EventCountView() = default;

int EventCountView::GetEventCount(ui::EventType type) {
  return event_count_[type];
}

void EventCountView::ResetCounts() {
  event_count_.clear();
}

void EventCountView::OnMouseMoved(const ui::MouseEvent& event) {
  // MouseMove events are not re-dispatched from the RootView.
  ++event_count_[ui::ET_MOUSE_MOVED];
  last_flags_ = 0;
}

void EventCountView::OnKeyEvent(ui::KeyEvent* event) {
  RecordEvent(event);
}

void EventCountView::OnMouseEvent(ui::MouseEvent* event) {
  RecordEvent(event);
}

void EventCountView::OnScrollEvent(ui::ScrollEvent* event) {
  RecordEvent(event);
}

void EventCountView::OnGestureEvent(ui::GestureEvent* event) {
  RecordEvent(event);
}

void EventCountView::RecordEvent(ui::Event* event) {
  ++event_count_[event->type()];
  last_flags_ = event->flags();
  if (handle_mode_ == CONSUME_EVENTS)
    event->SetHandled();
}

ResizeAwareParentView::ResizeAwareParentView() {
  SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kHorizontal));
}

void ResizeAwareParentView::ChildPreferredSizeChanged(View* child) {
  Layout();
}

}  // namespace views
