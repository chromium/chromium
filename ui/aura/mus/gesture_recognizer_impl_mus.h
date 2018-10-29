// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_GESTURE_RECOGNIZER_IMPL_MUS_H_
#define UI_AURA_MUS_GESTURE_RECOGNIZER_IMPL_MUS_H_

#include "base/macros.h"
#include "ui/aura/mus/window_tree_client_observer.h"
#include "ui/events/event_observer.h"
#include "ui/events/gestures/gesture_recognizer_impl.h"

namespace ui {
class PointF;
}

namespace aura {

class Window;
class WindowTreeClient;

// GestureRecognizer implementation for Mus. This is mostly identical to
// GestureRecognizerImpl, but it handles keeping GetLastTouchPointForTarget in
// sync with the server when the touch events are handled within the server.
class GestureRecognizerImplMus : public ui::GestureRecognizerImpl,
                                 public aura::WindowTreeClientObserver,
                                 public ui::EventObserver {
 public:
  explicit GestureRecognizerImplMus(aura::WindowTreeClient* client);
  ~GestureRecognizerImplMus() override;

 private:
  // ui::GestureRecognizerImpl:
  bool GetLastTouchPointForTarget(ui::GestureConsumer* consumer,
                                  gfx::PointF* point) override;

  // aura::WindowTreeClientObserver:
  void OnWillDestroyClient(aura::WindowTreeClient* client) override;
  void OnWindowMoveStarted(aura::Window* window,
                           const gfx::Point& cursor_location,
                           ws::mojom::MoveLoopSource source) override;
  void OnWindowMoveEnded(bool success) override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  aura::WindowTreeClient* client_;
  aura::Window* moving_window_ = nullptr;
  gfx::Point last_location_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerImplMus);
};

}  // namespace aura

#endif  // UI_AURA_MUS_GESTURE_RECOGNIZER_IMPL_MUS_H_
