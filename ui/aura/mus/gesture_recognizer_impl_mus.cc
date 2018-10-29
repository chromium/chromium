// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/gesture_recognizer_impl_mus.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace aura {

GestureRecognizerImplMus::GestureRecognizerImplMus(
    aura::WindowTreeClient* client)
    : client_(client) {
  client->AddObserver(this);
}

GestureRecognizerImplMus::~GestureRecognizerImplMus() {
  OnWindowMoveEnded(false);
  if (client_)
    client_->RemoveObserver(this);
}

void GestureRecognizerImplMus::OnWillDestroyClient(
    aura::WindowTreeClient* client) {
  DCHECK_EQ(client_, client);
  OnWindowMoveEnded(false);
  client_->RemoveObserver(this);
  client_ = nullptr;
}

void GestureRecognizerImplMus::OnWindowMoveStarted(
    aura::Window* window,
    const gfx::Point& cursor_location,
    ws::mojom::MoveLoopSource source) {
  DCHECK(!moving_window_);
  if (source != ws::mojom::MoveLoopSource::TOUCH)
    return;
  moving_window_ = window;
  last_location_in_screen_ = cursor_location;
  Env* env = Env::GetInstance();
  std::set<ui::EventType> types = {
      ui::ET_TOUCH_RELEASED, ui::ET_TOUCH_PRESSED, ui::ET_TOUCH_MOVED,
      ui::ET_TOUCH_CANCELLED,
  };
  env->AddEventObserver(this, env, types);
}

void GestureRecognizerImplMus::OnWindowMoveEnded(bool success) {
  if (!moving_window_)
    return;
  Env::GetInstance()->RemoveEventObserver(this);
  moving_window_ = nullptr;
}

bool GestureRecognizerImplMus::GetLastTouchPointForTarget(
    ui::GestureConsumer* consumer,
    gfx::PointF* point) {
  // When a window is moving, the touch events are handled completely within the
  // shell and do not come to the client and so the default
  // GetLastTouchPointForTarget won't work. Instead, this reports the last
  // location through PointerWatcher. See also
  // https://docs.google.com/document/d/1AKeK8IuF-j2TJ-2sPsewORXdjnr6oAzy5nnR1zwrsfc/edit#
  aura::Window* target_window = static_cast<aura::Window*>(consumer);
  if (moving_window_ && moving_window_->Contains(target_window)) {
    aura::client::ScreenPositionClient* client =
        aura::client::GetScreenPositionClient(target_window->GetRootWindow());
    if (client) {
      gfx::Point location_in_window = last_location_in_screen_;
      client->ConvertPointFromScreen(target_window, &location_in_window);
      point->set_x(location_in_window.x());
      point->set_y(location_in_window.y());
      return true;
    }
  }
  return GestureRecognizerImpl::GetLastTouchPointForTarget(consumer, point);
}

void GestureRecognizerImplMus::OnEvent(const ui::Event& event) {
  DCHECK(moving_window_);

  last_location_in_screen_ = event.AsLocatedEvent()->location();
  display::Display display;
  if (display::Screen::GetScreen()->GetDisplayWithDisplayId(
          moving_window_->GetHost()->GetDisplayId(), &display)) {
    last_location_in_screen_ += display.bounds().OffsetFromOrigin();
  }
}

}  // namespace aura
