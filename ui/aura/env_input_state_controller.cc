// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/env_input_state_controller.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"

namespace aura {

EnvInputStateController::EnvInputStateController(Env* env) : env_(env) {}

EnvInputStateController::~EnvInputStateController() = default;

void EnvInputStateController::UpdateStateForMouseEvent(
    const Window* window,
    const ui::MouseEvent& event) {
  switch (event.type()) {
    case ui::EventType::kMousePressed:
      env_->set_mouse_button_flags(event.button_flags());
      break;
    case ui::EventType::kMouseReleased:
      env_->set_mouse_button_flags(event.button_flags() &
                                   ~event.changed_button_flags());
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
    const aura::Window* window,
    const ui::TouchEvent& event) {
  switch (event.type()) {
    case ui::EventType::kTouchPressed:
      touch_ids_down_ |= (1 << event.pointer_details().id);
      env_->SetTouchDown(touch_ids_down_ != 0);
      break;

    // Handle EventType::kTouchCancelled only if it has a native event.
    case ui::EventType::kTouchCancelled:
      if (!event.HasNativeEvent())
        break;
      [[fallthrough]];
    case ui::EventType::kTouchReleased:
      touch_ids_down_ = (touch_ids_down_ | (1 << event.pointer_details().id)) ^
                        (1 << event.pointer_details().id);
      env_->SetTouchDown(touch_ids_down_ != 0);
      break;

    case ui::EventType::kTouchMoved:
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  const gfx::Point& location_in_root = event.root_location();
  const auto* root_window = window->GetRootWindow();
  client::ScreenPositionClient* client =
      client::GetScreenPositionClient(root_window);
  gfx::Point location_in_screen = location_in_root;
  if (client) {
    client->ConvertPointToScreen(root_window, &location_in_screen);
  }
  env_->SetLastTouchLocation(window, location_in_screen);
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
