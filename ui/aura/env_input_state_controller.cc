// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/env_input_state_controller.h"

#include "build/build_config.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"

namespace aura {

EnvInputStateController::EnvInputStateController(Env* env) : env_(env) {}

EnvInputStateController::~EnvInputStateController() = default;

void EnvInputStateController::UpdateStateForMouseEvent(
    const Window* window,
    Window* target,
    const ui::MouseEvent& event) {
  switch (event.type()) {
    case ui::EventType::kMousePressed:
      env_->set_mouse_button_flags(event.button_flags());
      if (event.changed_button_flags() & ui::EF_LEFT_MOUSE_BUTTON) {
        // A non-client press (e.g. on the window caption) may become a window
        // move, so it is not treated as a press on its target window. A press
        // supersedes any earlier press whose release was never seen.
        SetLeftMousePress((event.flags() & ui::EF_IS_NON_CLIENT) ? nullptr
                                                                 : target);
      }
      break;
    case ui::EventType::kMouseReleased:
      env_->set_mouse_button_flags(event.button_flags() &
                                   ~event.changed_button_flags());
      if (event.changed_button_flags() & ui::EF_LEFT_MOUSE_BUTTON) {
        ResetLeftMousePress();
      }
      break;
    default:
      break;
  }

  // If a synthesized event is created from a native event (e.g. EnterNotify
  // XEvents), then we should take the location as we would for a
  // non-synthesized event.
  if (event.type() != ui::EventType::kMouseCaptureChanged &&
      (!(event.flags() & ui::EF_IS_SYNTHESIZED) || event.HasNativeEvent())) {
    SetLastMouseLocation(window, event.root_location());
  }
}

void EnvInputStateController::UpdateStateForTouchEvent(
    Window* window,
    const ui::TouchEvent& event) {
  const gfx::Point& location_in_root = event.root_location();
  const auto* root_window = window->GetRootWindow();
  client::ScreenPositionClient* client =
      client::GetScreenPositionClient(root_window);
  gfx::Point location_in_screen = location_in_root;
  if (client) {
    client->ConvertPointToScreen(root_window, &location_in_screen);
  }

  const ui::PointerId id = event.pointer_details().id;
  switch (event.type()) {
    case ui::EventType::kTouchPressed:
      touch_ids_down_ |= (1 << id);
      env_->SetTouchDown(touch_ids_down_ != 0);
      // A press for an id that is still tracked means the previous sequence's
      // release was never seen. It is superseded by this press.
      RemoveActiveTouch(id);
      // A non-client touch (e.g. on a draggable region of web contents in a
      // frameless app window) may become a window move, so it is not treated
      // as a touch on its target window.
      if (!(event.flags() & ui::EF_IS_NON_CLIENT)) {
        AddActiveTouch(id, window, location_in_screen);
      }
      break;

    case ui::EventType::kTouchCancelled:
#if BUILDFLAG(IS_CHROMEOS)
      // Handle EventType::kTouchCancelled only if it has a native event.
      // ChromeOS exo touch drag relies on the ability to cancel touch
      // downs with synthetic events when handing off to the new consumer,
      // without losing the global env touch down state.
      if (!event.HasNativeEvent()) {
        // The touch is no longer owned by the window it was pressed on, so
        // it must not be used to start a drag from that window.
        RemoveActiveTouch(id);
        break;
      }
      [[fallthrough]];
#endif  // BUILDFLAG(IS_CHROMEOS)
    case ui::EventType::kTouchReleased:
      touch_ids_down_ = (touch_ids_down_ | (1 << id)) ^ (1 << id);
      env_->SetTouchDown(touch_ids_down_ != 0);
      RemoveActiveTouch(id);
      break;

    case ui::EventType::kTouchMoved:
      if (auto it = active_touches_.find(id); it != active_touches_.end()) {
        it->second.location_in_screen = location_in_screen;
      }
      break;

    default:
      NOTREACHED();
  }
  env_->SetLastTouchLocation(window, location_in_screen);
}

std::optional<gfx::Point> EnvInputStateController::ConsumeTouchForDrag(
    const Window* window) {
  if (!window) {
    return std::nullopt;
  }
  for (auto& [id, touch] : active_touches_) {
    if (touch.target == window && !touch.used_for_drag) {
      touch.used_for_drag = true;
      return touch.location_in_screen;
    }
  }
  return std::nullopt;
}

bool EnvInputStateController::ConsumeLeftMousePressForDrag(
    const Window* window) {
  if (!window || left_mouse_press_target_ != window ||
      left_mouse_press_used_for_drag_) {
    return false;
  }
  left_mouse_press_used_for_drag_ = true;
  return true;
}

void EnvInputStateController::ResetActiveTouches() {
  active_touches_.clear();
  target_observations_.RemoveAllObservations();
  // `left_mouse_press_target_` may still be observed.
  if (left_mouse_press_target_) {
    ObserveTarget(left_mouse_press_target_);
  }
}

void EnvInputStateController::OnWindowDestroying(Window* window) {
  // Keep the touches themselves, since they are still physically down, but
  // forget the destroyed target so that it can never match another window
  // that is later allocated at the same address.
  for (auto& [id, touch] : active_touches_) {
    if (touch.target == window) {
      touch.target = nullptr;
    }
  }
  if (left_mouse_press_target_ == window) {
    left_mouse_press_target_ = nullptr;
  }
  target_observations_.RemoveObservation(window);
}

void EnvInputStateController::AddActiveTouch(
    ui::PointerId id,
    Window* window,
    const gfx::Point& location_in_screen) {
  active_touches_[id] = {.target = window,
                         .location_in_screen = location_in_screen};
  ObserveTarget(window);
}

void EnvInputStateController::RemoveActiveTouch(ui::PointerId id) {
  auto it = active_touches_.find(id);
  if (it == active_touches_.end()) {
    return;
  }
  Window* target = it->second.target;
  active_touches_.erase(it);
  MaybeStopObservingTarget(target);
}

void EnvInputStateController::SetLeftMousePress(Window* window) {
  ResetLeftMousePress();
  left_mouse_press_target_ = window;
  ObserveTarget(window);
}

void EnvInputStateController::ResetLeftMousePress() {
  Window* target = left_mouse_press_target_;
  left_mouse_press_target_ = nullptr;
  left_mouse_press_used_for_drag_ = false;
  MaybeStopObservingTarget(target);
}

void EnvInputStateController::ObserveTarget(Window* window) {
  if (window && !target_observations_.IsObservingSource(window)) {
    target_observations_.AddObservation(window);
  }
}

void EnvInputStateController::MaybeStopObservingTarget(Window* window) {
  if (!window || left_mouse_press_target_ == window) {
    return;
  }
  for (const auto& [id, touch] : active_touches_) {
    if (touch.target == window) {
      return;
    }
  }
  target_observations_.RemoveObservation(window);
}

void EnvInputStateController::SetLastMouseLocation(
    const Window* root_window,
    const gfx::Point& location_in_root) const {
  // If |root_window| is null, we are only using the event to update event
  // states, so we shouldn't update mouse location.
  if (!root_window)
    return;

  client::ScreenPositionClient* client =
      client::GetScreenPositionClient(root_window);
  if (client) {
    gfx::Point location_in_screen = location_in_root;
    client->ConvertPointToScreen(root_window, &location_in_screen);
    env_->SetLastMouseLocation(location_in_screen);
  } else {
    env_->SetLastMouseLocation(location_in_root);
  }
}

}  // namespace aura
