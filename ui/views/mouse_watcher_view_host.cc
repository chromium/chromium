// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mouse_watcher_view_host.h"

#include "ui/display/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

MouseWatcherViewHost::MouseWatcherViewHost(View* view,
                                           const gfx::Insets& hot_zone_insets)
    : view_(view),
      hot_zone_insets_(hot_zone_insets) {
}

MouseWatcherViewHost::~MouseWatcherViewHost() = default;

bool MouseWatcherViewHost::Contains(const gfx::Point& screen_point,
                                    EventType type) {
  bool in_view = IsCursorInViewZone(screen_point);
  if (!in_view || (type == EventType::kExit && !IsMouseOverWindow()))
    return false;
  return true;
}

// Returns whether or not the cursor is currently in the view's "zone" which
// is defined as a slightly larger region than the view.
bool MouseWatcherViewHost::IsCursorInViewZone(const gfx::Point& screen_point) {
  gfx::Rect bounds = view_->GetLocalBounds();
  gfx::Point view_topleft(bounds.origin());
  View::ConvertPointToScreen(view_, &view_topleft);
  bounds.set_origin(view_topleft);
  bounds.SetRect(view_topleft.x() - hot_zone_insets_.left(),
                 view_topleft.y() - hot_zone_insets_.top(),
                 bounds.width() + hot_zone_insets_.width(),
                 bounds.height() + hot_zone_insets_.height());

  return bounds.Contains(screen_point.x(), screen_point.y());
}

// Returns true if the mouse is over the view's window.
bool MouseWatcherViewHost::IsMouseOverWindow() {
  Widget* widget = view_->GetWidget();
  if (!widget)
    return false;

  return display::Screen::GetScreen()->IsWindowUnderCursor(
      widget->GetNativeWindow());
}

}  // namespace views
