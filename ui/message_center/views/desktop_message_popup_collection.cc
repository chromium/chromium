// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/desktop_message_popup_collection.h"

#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace message_center {

DesktopMessagePopupCollection::DesktopMessagePopupCollection() = default;

DesktopMessagePopupCollection::~DesktopMessagePopupCollection() = default;

void DesktopMessagePopupCollection::StartObserving() {
  auto* screen = display::Screen::GetScreen();
  if (screen_ || !screen)
    return;

  screen_ = screen;
  display_observer_.emplace(this);
  display::Display display = screen_->GetPrimaryDisplay();
  primary_display_id_ = display.id();
  RecomputeAlignment(display);
}

int DesktopMessagePopupCollection::GetPopupOriginX(
    const gfx::Rect& popup_bounds) const {
  if (IsFromLeft())
    return work_area_.x() + kMarginBetweenPopups;
  return work_area_.right() - kMarginBetweenPopups - popup_bounds.width();
}

int DesktopMessagePopupCollection::GetBaseline() const {
  return IsTopDown() ? work_area_.y() + kMarginBetweenPopups
                     : work_area_.bottom() - kMarginBetweenPopups;
}

gfx::Rect DesktopMessagePopupCollection::GetWorkArea() const {
  return work_area_;
}

bool DesktopMessagePopupCollection::IsTopDown() const {
  return (alignment_ & POPUP_ALIGNMENT_TOP) != 0;
}

bool DesktopMessagePopupCollection::IsFromLeft() const {
  return (alignment_ & POPUP_ALIGNMENT_LEFT) != 0;
}

bool DesktopMessagePopupCollection::RecomputeAlignment(
    const display::Display& display) {
  if (work_area_ == display.work_area())
    return false;

  work_area_ = display.work_area();

  // If the taskbar is at the top, render notifications top down. Some platforms
  // like Gnome can have taskbars at top and bottom. In this case it's more
  // likely that the systray is on the top one.
  alignment_ = work_area_.y() > display.bounds().y() ? POPUP_ALIGNMENT_TOP
                                                     : POPUP_ALIGNMENT_BOTTOM;

  // If the taskbar is on the left show the notifications on the left. Otherwise
  // show it on right since it's very likely that the systray is on the right if
  // the taskbar is on the top or bottom.
  // Since on some platforms like Ubuntu Unity there's also a launcher along
  // with a taskbar (panel), we need to check that there is really nothing at
  // the top before concluding that the taskbar is at the left.
  alignment_ |= (work_area_.x() > display.bounds().x() &&
                 work_area_.y() == display.bounds().y())
                    ? POPUP_ALIGNMENT_LEFT
                    : POPUP_ALIGNMENT_RIGHT;

  return true;
}

void DesktopMessagePopupCollection::ConfigureWidgetInitParamsForContainer(
    views::Widget* widget,
    views::Widget::InitParams* init_params) {
  // Do nothing, which will use the default container.
}

bool DesktopMessagePopupCollection::IsPrimaryDisplayForNotification() const {
  return true;
}

bool DesktopMessagePopupCollection::BlockForMixedFullscreen(
    const Notification& notification) const {
  // Always return false. Notifications for fullscreen will be blocked by
  // chrome/browser/notifications/fullscreen_notification_blocker.
  return false;
}

// Anytime the display configuration changes, we need to recompute the alignment
// on the primary display. But, we get different events on different platforms.
// On Windows, for example, when switching from a laptop display to an external
// monitor, we get a OnDisplayMetricsChanged() event. On Linux, we get a
// OnDisplayRemoved() and a OnDisplayAdded() instead. In order to account for
// these slightly different abstractions, we update on every event.
void DesktopMessagePopupCollection::UpdatePrimaryDisplay() {
  display::Display primary_display = screen_->GetPrimaryDisplay();
  if (primary_display.id() != primary_display_id_) {
    primary_display_id_ = primary_display.id();
    if (RecomputeAlignment(primary_display))
      ResetBounds();
  }
}

void DesktopMessagePopupCollection::OnDisplayAdded(
    const display::Display& added_display) {
  // The added display could be the new primary display.
  UpdatePrimaryDisplay();
}

void DesktopMessagePopupCollection::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  // One of the removed displays may have been the primary display.
  UpdatePrimaryDisplay();
}

void DesktopMessagePopupCollection::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  // Set to kInvalidDisplayId so the alignment is updated regardless of whether
  // the primary display actually changed.
  primary_display_id_ = display::kInvalidDisplayId;
  UpdatePrimaryDisplay();
}

}  // namespace message_center
