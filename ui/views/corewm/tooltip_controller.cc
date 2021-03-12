// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/corewm/tooltip_state_manager.h"
#include "ui/views/widget/tooltip_manager.h"

namespace views {
namespace corewm {
namespace {

constexpr auto kDefaultHideTooltipTimeoutInMs =
    base::TimeDelta::FromSeconds(10);

// Returns true if |target| is a valid window to get the tooltip from.
// |event_target| is the original target from the event and |target| the window
// at the same location.
bool IsValidTarget(aura::Window* event_target, aura::Window* target) {
  if (!target || (event_target == target))
    return true;

  void* event_target_grouping_id = event_target->GetNativeWindowProperty(
      TooltipManager::kGroupingPropertyKey);
  void* target_grouping_id =
      target->GetNativeWindowProperty(TooltipManager::kGroupingPropertyKey);
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
    : state_manager_(
          std::make_unique<TooltipStateManager>(std::move(tooltip))) {}

TooltipController::~TooltipController() {
  if (observed_window_)
    observed_window_->RemoveObserver(this);
}

int TooltipController::GetMaxWidth(const gfx::Point& location) const {
  return state_manager_->GetMaxWidth(location);
}

void TooltipController::UpdateTooltip(aura::Window* target) {
  // Ensure relevant tooltip is updated when it is visible or scheduled to
  // show. Otherwise, a stale tooltip might be shown.
  if (observed_window_ == target &&
      (state_manager_->IsVisible() ||
       state_manager_->IsWillShowTooltipTimerRunning())) {
    UpdateIfRequired();
  }

  ResetWindowAtMousePressedIfNeeded(target);
}

void TooltipController::SetHideTooltipTimeout(aura::Window* target,
                                              base::TimeDelta timeout) {
  hide_tooltip_timeout_map_[target] = timeout;
}

void TooltipController::SetTooltipsEnabled(bool enable) {
  if (tooltips_enabled_ == enable)
    return;
  tooltips_enabled_ = enable;
  UpdateTooltip(observed_window_);
}

void TooltipController::OnKeyEvent(ui::KeyEvent* event) {
  // Always hide a tooltip on a key event. Since this controller is a pre-target
  // handler (i.e. the events are received here before the target act on them),
  // hiding the tooltip will not cancel any action supposed to show it triggered
  // by a key press.
  HideAndReset();
}

void TooltipController::OnMouseEvent(ui::MouseEvent* event) {
  // Ignore mouse events that coincide with the last touch event.
  if (event->location() == last_touch_loc_) {
    SetObservedWindow(nullptr);

    if (state_manager_->IsVisible())
      UpdateIfRequired();
    return;
  }
  switch (event->type()) {
    case ui::ET_MOUSE_CAPTURE_CHANGED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED: {
      last_mouse_loc_ = event->location();
      state_manager_->UpdatePositionIfWillShowTooltipTimerIsRunning(
          last_mouse_loc_);
      aura::Window* target = nullptr;
      // Avoid a call to display::Screen::GetWindowAtScreenPoint() since it can
      // be very expensive on X11 in cases when the tooltip is hidden anyway.
      if (tooltips_enabled_ && !aura::Env::GetInstance()->IsMouseButtonDown() &&
          !IsDragDropInProgress()) {
        target = GetTooltipTarget(*event, &last_mouse_loc_);
      }
      SetObservedWindow(target);

      if (state_manager_->IsVisible() ||
          (observed_window_ && IsTooltipTextUpdateNeeded())) {
        UpdateIfRequired();
      }
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
      state_manager_->HideAndReset();
      break;
    case ui::ET_MOUSEWHEEL:
      // Hide the tooltip for click, release, drag, wheel events.
      if (state_manager_->IsVisible())
        state_manager_->HideAndReset();
      break;
    default:
      break;
  }
}

void TooltipController::OnTouchEvent(ui::TouchEvent* event) {
  // Hide the tooltip for touch events.
  HideAndReset();
  last_touch_loc_ = event->location();
}

void TooltipController::OnCancelMode(ui::CancelModeEvent* event) {
  HideAndReset();
}

base::StringPiece TooltipController::GetLogContext() const {
  return "TooltipController";
}

void TooltipController::OnCursorVisibilityChanged(bool is_visible) {
  UpdateIfRequired();
}

void TooltipController::OnWindowVisibilityChanged(aura::Window* window,
                                                  bool visible) {
  if (!visible)
    HideAndReset();
}

void TooltipController::OnWindowDestroyed(aura::Window* window) {
  if (observed_window_ == window) {
    RemoveHideTooltipTimeoutFromMap(observed_window_);
    observed_window_ = nullptr;
  }

  if (state_manager_->tooltip_parent_window() == window)
    HideAndReset();
}

void TooltipController::OnWindowPropertyChanged(aura::Window* window,
                                                const void* key,
                                                intptr_t old) {
  if ((key == wm::kTooltipIdKey || key == wm::kTooltipTextKey) &&
      wm::GetTooltipText(window) != std::u16string() &&
      (IsTooltipTextUpdateNeeded() || IsTooltipIdUpdateNeeded())) {
    UpdateIfRequired();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TooltipController private:

void TooltipController::HideAndReset() {
  state_manager_->HideAndReset();
  SetObservedWindow(nullptr);
}

void TooltipController::UpdateIfRequired() {
  if (!tooltips_enabled_ || aura::Env::GetInstance()->IsMouseButtonDown() ||
      IsDragDropInProgress() || !IsCursorVisible()) {
    state_manager_->HideAndReset();
    return;
  }

  // When a user press a mouse button, we want to hide the tooltip and prevent
  // the tooltip from showing up again until the cursor moves to another view
  // than the one that received the press event.
  if (ShouldHideBecauseMouseWasOncePressed()) {
    state_manager_->HideAndReset();
    return;
  }
  tooltip_window_at_mouse_press_ = nullptr;

  // If the uniqueness indicator is different from the previously encountered
  // one, we should force tooltip update
  if (!state_manager_->IsVisible() || IsTooltipTextUpdateNeeded() ||
      IsTooltipIdUpdateNeeded()) {
    state_manager_->StopWillHideTooltipTimer();
    state_manager_->Show(observed_window_, wm::GetTooltipText(observed_window_),
                         last_mouse_loc_, GetHideTooltipTimeout());
  }
}

bool TooltipController::IsDragDropInProgress() const {
  if (!observed_window_)
    return false;
  aura::client::DragDropClient* client =
      aura::client::GetDragDropClient(observed_window_->GetRootWindow());
  return client && client->IsDragDropInProgress();
}

bool TooltipController::IsCursorVisible() const {
  if (!observed_window_)
    return false;
  aura::Window* root = observed_window_->GetRootWindow();
  if (!root)
    return false;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root);
  // |cursor_client| may be NULL in tests, treat NULL as always visible.
  return !cursor_client || cursor_client->IsCursorVisible();
}

base::TimeDelta TooltipController::GetHideTooltipTimeout() {
  std::map<aura::Window*, base::TimeDelta>::const_iterator it =
      hide_tooltip_timeout_map_.find(observed_window_);
  if (it == hide_tooltip_timeout_map_.end())
    return kDefaultHideTooltipTimeoutInMs;
  return it->second;
}

void TooltipController::SetObservedWindow(aura::Window* target) {
  if (observed_window_ == target)
    return;

  // When we are setting the |observed_window_| to nullptr, it is generally
  // because the cursor is over a window not owned by Chromium. To prevent a
  // tooltip from being shown after the cursor goes to another window not
  // managed by us, hide the the tooltip and cancel all timers that would show
  // the tooltip.
  if (!target && state_manager_->tooltip_parent_window()) {
    // Important: We can't call `TooltipController::HideAndReset` or we'd get an
    // infinite loop here.
    state_manager_->HideAndReset();
  }

  if (observed_window_)
    observed_window_->RemoveObserver(this);
  observed_window_ = target;
  if (observed_window_)
    observed_window_->AddObserver(this);
}

bool TooltipController::IsTooltipIdUpdateNeeded() const {
  return state_manager_->tooltip_id() != wm::GetTooltipId(observed_window_);
}

bool TooltipController::IsTooltipTextUpdateNeeded() const {
  return state_manager_->tooltip_text() != wm::GetTooltipText(observed_window_);
}

void TooltipController::RemoveHideTooltipTimeoutFromMap(aura::Window* window) {
  hide_tooltip_timeout_map_.erase(window);
}

void TooltipController::ResetWindowAtMousePressedIfNeeded(
    aura::Window* target) {
  // Reset |tooltip_window_at_mouse_press_| if the cursor moved within the same
  // window but over a region that has different tooltip text. This handles the
  // case of clicking on a view, moving within the same window but over a
  // different view, then back to the original view.
  if (tooltip_window_at_mouse_press_ &&
      target == tooltip_window_at_mouse_press_ &&
      wm::GetTooltipText(target) != tooltip_text_at_mouse_press_) {
    tooltip_window_at_mouse_press_ = nullptr;
  }
}

// TODO(bebeaudr): This approach is less than ideal. It looks at the tooltip
// text at the moment the mouse was pressed to determine whether or not we are
// on the same tooltip as before. This cause problems when two elements are next
// to each other and have the same text - unlikely, but an issue nonetheless.
// However, this is currently the nearest we can get since we don't have an
// identifier of the renderer side element that triggered the tooltip. Could we
// pass a renderer element unique id alongside the tooltip text?
bool TooltipController::ShouldHideBecauseMouseWasOncePressed() {
  return tooltip_window_at_mouse_press_ &&
         observed_window_ == tooltip_window_at_mouse_press_ &&
         wm::GetTooltipText(observed_window_) == tooltip_text_at_mouse_press_;
}

}  // namespace corewm
}  // namespace views
