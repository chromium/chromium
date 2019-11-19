// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/wm/public/tooltip_client.h"

namespace views {
namespace corewm {
namespace {

constexpr int kDelayForTooltipUpdateInMs = 500;
constexpr int kDefaultTooltipShownTimeoutMs = 10000;
#if defined(OS_WIN)
// Drawing a long word in tooltip is very slow on Windows. crbug.com/513693
constexpr size_t kMaxTooltipLength = 1024;
#else
constexpr size_t kMaxTooltipLength = 2048;
#endif

// Returns true if |target| is a valid window to get the tooltip from.
// |event_target| is the original target from the event and |target| the window
// at the same location.
bool IsValidTarget(aura::Window* event_target, aura::Window* target) {
  if (!target || (event_target == target))
    return true;

  void* event_target_grouping_id = event_target->GetNativeWindowProperty(
      TooltipManager::kGroupingPropertyKey);
  void* target_grouping_id = target->GetNativeWindowProperty(
      TooltipManager::kGroupingPropertyKey);
  return event_target_grouping_id &&
      event_target_grouping_id == target_grouping_id;
}

// Returns the target (the Window tooltip text comes from) based on the event.
// If a Window other than event.target() is returned, |location| is adjusted
// to be in the coordinates of the returned Window.
aura::Window* GetTooltipTarget(const ui::MouseEvent& event,
                               gfx::Point* location) {
  switch (event.type()) {
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      // On windows we can get a capture changed without an exit. We need to
      // reset state when this happens else the tooltip may incorrectly show.
      return nullptr;
    case ui::ET_MOUSE_EXITED:
      return nullptr;
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED: {
      aura::Window* event_target = static_cast<aura::Window*>(event.target());
      if (!event_target)
        return nullptr;

      // If a window other than |event_target| has capture, ignore the event.
      // This can happen when RootWindow creates events when showing/hiding, or
      // the system generates an extra event. We have to check
      // GetGlobalCaptureWindow() as Windows does not use a singleton
      // CaptureClient.
      if (!event_target->HasCapture()) {
        aura::Window* root = event_target->GetRootWindow();
        if (root) {
          aura::client::CaptureClient* capture_client =
              aura::client::GetCaptureClient(root);
          if (capture_client) {
            aura::Window* capture_window =
                capture_client->GetGlobalCaptureWindow();
            if (capture_window && event_target != capture_window)
              return nullptr;
          }
        }
        return event_target;
      }

      // If |target| has capture all events go to it, even if the mouse is
      // really over another window. Find the real window the mouse is over.
      const gfx::Point screen_loc = event.target()->GetScreenLocation(event);
      display::Screen* screen = display::Screen::GetScreen();
      aura::Window* target = screen->GetWindowAtScreenPoint(screen_loc);
      if (!target)
        return nullptr;
      gfx::Point target_loc(screen_loc);
      aura::client::GetScreenPositionClient(target->GetRootWindow())
          ->ConvertPointFromScreen(target, &target_loc);
      aura::Window* screen_target = target->GetEventHandlerForPoint(target_loc);
      if (!IsValidTarget(event_target, screen_target))
        return nullptr;

      aura::Window::ConvertPointToTarget(screen_target, target, &target_loc);
      *location = target_loc;
      return screen_target;
    }
    default:
      NOTREACHED();
      break;
  }
  return nullptr;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TooltipController public:

TooltipController::TooltipController(std::unique_ptr<Tooltip> tooltip)
    : tooltip_window_(nullptr),
      tooltip_id_(nullptr),
      tooltip_window_at_mouse_press_(nullptr),
      tooltip_(std::move(tooltip)),
      tooltips_enabled_(true),
      tooltip_show_delayed_(true) {}

TooltipController::~TooltipController() {
  if (tooltip_window_)
    tooltip_window_->RemoveObserver(this);
}

int TooltipController::GetMaxWidth(const gfx::Point& location) const {
  return tooltip_->GetMaxWidth(location);
}

void TooltipController::UpdateTooltip(aura::Window* target) {
  // If tooltip is visible, we may want to hide it. If it is not, we are ok.
  if (tooltip_window_ == target && tooltip_->IsVisible())
    UpdateIfRequired();

  // Reset |tooltip_window_at_mouse_press_| if the moving within the same window
  // but over a region that has different tooltip text.
  // This handles the case of clicking on a view, moving within the same window
  // but over a different view, than back to the original.
  if (tooltip_window_at_mouse_press_ &&
      target == tooltip_window_at_mouse_press_ &&
      wm::GetTooltipText(target) != tooltip_text_at_mouse_press_) {
    tooltip_window_at_mouse_press_ = nullptr;
  }
}

void TooltipController::SetTooltipShownTimeout(aura::Window* target,
                                               int timeout_in_ms) {
  tooltip_shown_timeout_map_[target] = timeout_in_ms;
}

void TooltipController::SetTooltipsEnabled(bool enable) {
  if (tooltips_enabled_ == enable)
    return;
  tooltips_enabled_ = enable;
  UpdateTooltip(tooltip_window_);
}

void TooltipController::OnKeyEvent(ui::KeyEvent* event) {
  // On key press, we want to hide the tooltip and not show it until change.
  // This is the same behavior as hiding tooltips on timeout. Hence, we can
  // simply simulate a timeout.
  if (tooltip_shown_timer_.IsRunning()) {
    tooltip_shown_timer_.Stop();
    TooltipShownTimerFired();
  }
}

void TooltipController::OnMouseEvent(ui::MouseEvent* event) {
  // Ignore mouse events that coincide with the last touch event.
  if (event->location() == last_touch_loc_) {
    SetTooltipWindow(nullptr);

    if (tooltip_->IsVisible())
      UpdateIfRequired();
    return;
  }
  switch (event->type()) {
    case ui::ET_MOUSE_CAPTURE_CHANGED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED: {
      curr_mouse_loc_ = event->location();
      aura::Window* target = nullptr;
      // Avoid a call to display::Screen::GetWindowAtScreenPoint() since it can
      // be very expensive on X11 in cases when the tooltip is hidden anyway.
      if (tooltips_enabled_ &&
          !aura::Env::GetInstance()->IsMouseButtonDown() &&
          !IsDragDropInProgress()) {
        target = GetTooltipTarget(*event, &curr_mouse_loc_);
      }
      SetTooltipWindow(target);

      if (tooltip_->IsVisible() ||
          (tooltip_window_ &&
           tooltip_text_ != wm::GetTooltipText(tooltip_window_)))
        UpdateIfRequired();
      break;
    }
    case ui::ET_MOUSE_PRESSED:
      if ((event->flags() & ui::EF_IS_NON_CLIENT) == 0) {
        aura::Window* target = static_cast<aura::Window*>(event->target());
        // We don't get a release for non-client areas.
        tooltip_window_at_mouse_press_ = target;
        if (target)
          tooltip_text_at_mouse_press_ = wm::GetTooltipText(target);
      }
      tooltip_->Hide();
      break;
    case ui::ET_MOUSEWHEEL:
      // Hide the tooltip for click, release, drag, wheel events.
      if (tooltip_->IsVisible())
        tooltip_->Hide();
      break;
    default:
      break;
  }
}

void TooltipController::OnTouchEvent(ui::TouchEvent* event) {
  // Hide the tooltip for touch events.
  tooltip_->Hide();
  SetTooltipWindow(nullptr);
  last_touch_loc_ = event->location();
}

void TooltipController::OnCancelMode(ui::CancelModeEvent* event) {
  tooltip_->Hide();
  SetTooltipWindow(nullptr);
}

void TooltipController::OnCursorVisibilityChanged(bool is_visible) {
  UpdateIfRequired();
}

void TooltipController::OnWindowDestroyed(aura::Window* window) {
  if (tooltip_window_ == window) {
    tooltip_->Hide();
    tooltip_shown_timeout_map_.erase(tooltip_window_);
    tooltip_window_ = nullptr;
  }
}

void TooltipController::OnWindowPropertyChanged(aura::Window* window,
                                                const void* key,
                                                intptr_t old) {
  if ((key == wm::kTooltipIdKey || key == wm::kTooltipTextKey) &&
      wm::GetTooltipText(window) != base::string16() &&
      (tooltip_text_ != wm::GetTooltipText(window) ||
       tooltip_id_ != wm::GetTooltipId(window)))
    UpdateIfRequired();
}

////////////////////////////////////////////////////////////////////////////////
// TooltipController private:

void TooltipController::TooltipShownTimerFired() {
  tooltip_->Hide();
}

void TooltipController::UpdateIfRequired() {
  if (!tooltips_enabled_ || aura::Env::GetInstance()->IsMouseButtonDown() ||
      IsDragDropInProgress() || !IsCursorVisible()) {
    tooltip_->Hide();
    return;
  }

  base::string16 tooltip_text;
  if (tooltip_window_)
    tooltip_text = wm::GetTooltipText(tooltip_window_);

  // If the user pressed a mouse button. We will hide the tooltip and not show
  // it until there is a change in the tooltip.
  if (tooltip_window_at_mouse_press_) {
    if (tooltip_window_ == tooltip_window_at_mouse_press_ &&
        tooltip_text == tooltip_text_at_mouse_press_) {
      tooltip_->Hide();
      return;
    }
    tooltip_window_at_mouse_press_ = nullptr;
  }

  // If the uniqueness indicator is different from the previously encountered
  // one, we should force tooltip update
  const void* tooltip_id = wm::GetTooltipId(tooltip_window_);
  bool ids_differ = false;
  ids_differ = tooltip_id_ != tooltip_id;
  tooltip_id_ = tooltip_id;

  // If we come here from UpdateTooltip(), we have already checked for tooltip
  // visibility and this check below will have no effect.
  if (tooltip_text_ != tooltip_text || !tooltip_->IsVisible() || ids_differ) {
    tooltip_shown_timer_.Stop();
    tooltip_text_ = tooltip_text;
    base::string16 trimmed_text =
        gfx::TruncateString(tooltip_text_, kMaxTooltipLength, gfx::WORD_BREAK);
    // If the string consists entirely of whitespace, then don't both showing it
    // (an empty tooltip is useless).
    base::TrimWhitespace(trimmed_text, base::TRIM_ALL,
                         &tooltip_text_whitespace_trimmed_);
    if (tooltip_text_whitespace_trimmed_.empty()) {
      tooltip_->Hide();
      tooltip_defer_timer_.Stop();
    } else if (tooltip_show_delayed_) {
      // Initialize the one-shot timer to show the tooltip in a while.
      // If there is already a request queued then cancel it and post the new
      // request. This ensures that tooltip won't show up too early.
      // The delayed appearance of a tooltip is by default.
      if (tooltip_defer_timer_.IsRunning()) {
        tooltip_defer_timer_.Reset();
      } else {
        tooltip_defer_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(
                                                  kDelayForTooltipUpdateInMs),
                                   this, &TooltipController::ShowTooltip);
      }
    } else {
      ShowTooltip();  // Allow tooltip to show up without delay for unit tests.
    }
  }
}

void TooltipController::ShowTooltip() {
  if (!tooltip_window_)
    return;
  gfx::Point widget_loc =
      curr_mouse_loc_ + tooltip_window_->GetBoundsInScreen().OffsetFromOrigin();
  tooltip_->SetText(tooltip_window_, tooltip_text_whitespace_trimmed_,
                    widget_loc);
  tooltip_->Show();
  int timeout = GetTooltipShownTimeout();
  if (timeout > 0) {
    tooltip_shown_timer_.Start(FROM_HERE,
                               base::TimeDelta::FromMilliseconds(timeout), this,
                               &TooltipController::TooltipShownTimerFired);
  }
}

bool TooltipController::IsTooltipVisible() {
  return tooltip_->IsVisible();
}

bool TooltipController::IsDragDropInProgress() {
  if (!tooltip_window_)
    return false;
  aura::client::DragDropClient* client =
      aura::client::GetDragDropClient(tooltip_window_->GetRootWindow());
  return client && client->IsDragDropInProgress();
}

bool TooltipController::IsCursorVisible() {
  if (!tooltip_window_)
    return false;
  aura::Window* root = tooltip_window_->GetRootWindow();
  if (!root)
    return false;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root);
  // |cursor_client| may be NULL in tests, treat NULL as always visible.
  return !cursor_client || cursor_client->IsCursorVisible();
}

int TooltipController::GetTooltipShownTimeout() {
  std::map<aura::Window*, int>::const_iterator it =
      tooltip_shown_timeout_map_.find(tooltip_window_);
  if (it == tooltip_shown_timeout_map_.end())
    return kDefaultTooltipShownTimeoutMs;
  return it->second;
}

void TooltipController::SetTooltipWindow(aura::Window* target) {
  if (tooltip_window_ == target)
    return;
  if (tooltip_window_)
    tooltip_window_->RemoveObserver(this);
  tooltip_window_ = target;
  if (tooltip_window_)
    tooltip_window_->AddObserver(this);
}

}  // namespace corewm
}  // namespace views
