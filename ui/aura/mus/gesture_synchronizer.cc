// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/gesture_synchronizer.h"

#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/window.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace aura {

GestureSynchronizer::GestureSynchronizer(ws::mojom::WindowTree* window_tree)
    : window_tree_(window_tree) {
  Env::GetInstance()->gesture_recognizer()->AddObserver(this);
}

GestureSynchronizer::~GestureSynchronizer() {
  Env::GetInstance()->gesture_recognizer()->RemoveObserver(this);
}

void GestureSynchronizer::OnActiveTouchesCanceledExcept(
    ui::GestureConsumer* not_cancelled) {
  ws::Id not_cancelled_window_id = kInvalidServerId;
  if (not_cancelled) {
    WindowMus* not_cancelled_window =
        WindowMus::Get(static_cast<Window*>(not_cancelled));
    not_cancelled_window_id = not_cancelled_window->server_id();
  }
  window_tree_->CancelActiveTouchesExcept(not_cancelled_window_id);
}

void GestureSynchronizer::OnEventsTransferred(
    ui::GestureConsumer* current_consumer,
    ui::GestureConsumer* new_consumer,
    ui::TransferTouchesBehavior transfer_touches_behavior) {
  WindowMus* current_window =
      WindowMus::Get(static_cast<Window*>(current_consumer));
  WindowMus* new_window = WindowMus::Get(static_cast<Window*>(new_consumer));
  DCHECK(current_window);
  DCHECK(new_window);
  window_tree_->TransferGestureEventsTo(
      current_window->server_id(), new_window->server_id(),
      transfer_touches_behavior == ui::TransferTouchesBehavior::kCancel);
}

void GestureSynchronizer::OnActiveTouchesCanceled(
    ui::GestureConsumer* consumer) {
  WindowMus* window = WindowMus::Get(static_cast<Window*>(consumer));
  window_tree_->CancelActiveTouches(window->server_id());
}

}  // namespace aura
