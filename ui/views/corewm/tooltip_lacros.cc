// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_lacros.h"

#include <algorithm>

#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"
#include "ui/wm/public/tooltip_observer.h"

namespace views::corewm {

namespace {

ui::PlatformWindowTooltipTrigger ToPlatformWindowTooltipTrigger(
    TooltipTrigger trigger) {
  switch (trigger) {
    case TooltipTrigger::kCursor:
      return ui::PlatformWindowTooltipTrigger::kCursor;
    case TooltipTrigger::kKeyboard:
      return ui::PlatformWindowTooltipTrigger::kKeyboard;
  }
}

}  // namespace

const char TooltipLacros::kWidgetName[] = "TooltipLacros";

TooltipLacros::TooltipLacros() = default;

TooltipLacros::~TooltipLacros() {
  // Hide tooltip before destructing.
  Hide();
}

void TooltipLacros::AddObserver(wm::TooltipObserver* observer) {
  observers_.AddObserver(observer);
}

void TooltipLacros::RemoveObserver(wm::TooltipObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TooltipLacros::OnTooltipShownOnServer(const std::u16string& text,
                                           const gfx::Rect& bounds) {
  is_visible_ = true;
  observers_.Notify(&wm::TooltipObserver::OnTooltipShown, parent_window_, text,
                    bounds);
}

void TooltipLacros::OnTooltipHiddenOnServer() {
  is_visible_ = false;
  observers_.Notify(&wm::TooltipObserver::OnTooltipHidden, parent_window_);
}

int TooltipLacros::GetMaxWidth(const gfx::Point& location) const {
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Rect display_bounds(screen->GetDisplayNearestPoint(location).bounds());
  return std::min(kTooltipMaxWidth, (display_bounds.width() + 1) / 2);
}

void TooltipLacros::Update(aura::Window* parent_window,
                           const std::u16string& text,
                           const gfx::Point& position,
                           const TooltipTrigger trigger) {
  DCHECK(parent_window);
  parent_window_ = parent_window;
  text_ = text;
  position_ = position;

  // Add the distance between `parent_window` and its toplevel window to
  // `position_` since Ash-side server will use this position as relative to
  // wayland toplevel window.
  // TODO(crbug.com/40246673): Use WaylandWindow instead of ToplevelWindow/Popup
  // when it's supported on ozone.
  aura::Window::ConvertPointToTarget(
      parent_window_, parent_window_->GetRootWindow(), &position_);
  trigger_ = trigger;
}

void TooltipLacros::SetDelay(const base::TimeDelta& show_delay,
                             const base::TimeDelta& hide_delay) {
  show_delay_ = show_delay;
  hide_delay_ = hide_delay;
}

void TooltipLacros::Show() {
  DCHECK(parent_window_);
  auto* host =
      views::DesktopWindowTreeHostLacros::From(parent_window_->GetHost());
  auto* platform_window = host ? host->platform_window() : nullptr;
  DCHECK(platform_window);
  platform_window->ShowTooltip(text_, position_,
                               ToPlatformWindowTooltipTrigger(trigger_),
                               show_delay_, hide_delay_);
}

void TooltipLacros::Hide() {
  if (!parent_window_) {
    return;
  }

  auto* host =
      views::DesktopWindowTreeHostLacros::From(parent_window_->GetHost());
  auto* platform_window = host ? host->platform_window() : nullptr;
  // `platform_window` may be null for testing.
  if (platform_window) {
    platform_window->HideTooltip();
  }

  parent_window_ = nullptr;
}

bool TooltipLacros::IsVisible() {
  return is_visible_;
}

}  // namespace views::corewm
