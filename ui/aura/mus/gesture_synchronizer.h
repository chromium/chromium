// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_GESTURE_SYNCHRONIZER_H_
#define UI_AURA_MUS_GESTURE_SYNCHRONIZER_H_

#include "base/macros.h"
#include "ui/events/gestures/gesture_recognizer_observer.h"

namespace ws {
namespace mojom {

class WindowTree;

}  // namespace mojom
}  // namespace ws

namespace aura {

// GestureSynchronizer is responsible for keeping GestureRecognizer's state
// synchronized between aura and the mus server.
class GestureSynchronizer : public ui::GestureRecognizerObserver {
 public:
  explicit GestureSynchronizer(ws::mojom::WindowTree* window_tree);
  ~GestureSynchronizer() override;

 private:
  // ui::GestureRecognizerObserver:
  void OnActiveTouchesCanceledExcept(
      ui::GestureConsumer* not_cancelled) override;
  void OnEventsTransferred(
      ui::GestureConsumer* current_consumer,
      ui::GestureConsumer* new_consumer,
      ui::TransferTouchesBehavior transfer_touches_behavior) override;
  void OnActiveTouchesCanceled(ui::GestureConsumer* consumer) override;

  ws::mojom::WindowTree* window_tree_;

  DISALLOW_COPY_AND_ASSIGN(GestureSynchronizer);
};

}  // namespace aura

#endif  // UI_AURA_MUS_GESTURE_SYNCHRONIZER_H_
