// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_MOUSE_H_
#define UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_MOUSE_H_

#include <list>
#include <memory>
#include <set>
#include <utility>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/rectify_callback.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"

namespace views {

class Widget;

namespace test {

// Class which provides useful primitives for controlling the mouse and then
// cleaning up mouse state (even if a test fails). As this object does control
// the mouse, do not create multiple simultaneous instances, and strongly prefer
// to use it only in test suites such as interactive_ui_tests where a single
// test can control the mouse at a time.
class InteractionTestUtilMouse {
 public:
  // Construct for a particular window or browser. This is required because the
  // util object may need access to a drag controller, which is most easily
  // accessed via the window.
  explicit InteractionTestUtilMouse(Widget* widget);

  ~InteractionTestUtilMouse();
  InteractionTestUtilMouse(const InteractionTestUtilMouse&) = delete;
  void operator=(const InteractionTestUtilMouse&) = delete;

  // Callback called when a gesture ends; if the gesture succeeded, `success`
  // will be true. If CancelAllGestures() is called or this object is destroyed,
  // the callback is called immediately.
  using GestureCallback = base::OnceCallback<void(bool success)>;

  // These represent mouse gestures of different types. They are implementation
  // details; prefer to use the static factory methods below.
  using MouseButtonGesture =
      std::pair<ui_controls::MouseButton, ui_controls::MouseButtonState>;
  using MouseMoveGesture = gfx::Point;
  using MouseGesture = absl::variant<MouseMoveGesture, MouseButtonGesture>;
  using MouseGestures = std::list<MouseGesture>;

  // These factory methods create individual or compound gestures. They can be
  // chained together. Prefer these to directly constructing a MouseGesture.
  static MouseGesture MoveTo(gfx::Point point);
  static MouseGesture MouseDown(ui_controls::MouseButton button);
  static MouseGesture MouseUp(ui_controls::MouseButton button);
  static MouseGestures Click(ui_controls::MouseButton button);
  static MouseGestures DragAndHold(gfx::Point destination);
  static MouseGestures DragAndRelease(gfx::Point destination);

  // Perform the gesture or gestures specified, then call `result_callback` on
  // success or failure.
  template <typename T, typename... Args>
  void PerformGestures(T result_callback, Args... gestures);

  // Cancels any pending actions and cleans up any resulting mouse state (i.e.
  // releases any buttons which were pressed).
  void CancelAllGestures();

 private:
  explicit InteractionTestUtilMouse(gfx::NativeWindow window);

  // Helper methods for adding gestures to a gesture list.
  static void AddGestures(MouseGestures& gestures, MouseGesture to_add);
  static void AddGestures(MouseGestures& gestures, MouseGestures to_add);

  void PerformGesturesImpl(MouseGestures gestures,
                           GestureCallback result_callback);

  void QueueNextGesture();
  void PerformNextGesture();
  void OnMouseButton(MouseButtonGesture gesture);
  void OnMouseMove();
  void OnDragStart();
  void OnDragEnd();
  void OnSequenceComplete();

  // List of gestures left to perform.
  MouseGestures pending_gestures_;

  // The callback that will be called when all gestures are performed, or the
  // current gesture fails or is canceled.
  GestureCallback pending_callback_;

  // The set of mouse buttons currently depressed. Used to clean up on abort.
  std::set<ui_controls::MouseButton> buttons_down_;

  // Whether the mouse is currently being dragged.
  bool dragging_ = false;

  // Whether the mouse has been dragged and released without [yet] doing
  // cleanup.
  bool dragged_ = false;

#if defined(USE_AURA)
  // These are used in order to clean up extraneous drags on Aura platforms;
  // without this it is possible for a drag loop to start and not exit,
  // preventing a test from completing.
  class DragEnder;
  std::unique_ptr<DragEnder> drag_ender_;
  class NativeWindowRef;
  const std::unique_ptr<NativeWindowRef> native_window_;
#endif

  base::WeakPtrFactory<InteractionTestUtilMouse> weak_ptr_factory_{this};
};

template <typename T, typename... Args>
void InteractionTestUtilMouse::PerformGestures(T result_callback,
                                               Args... gestures) {
  MouseGestures gesture_list;
  (AddGestures(gesture_list, std::move(gestures)), ...);
  PerformGesturesImpl(
      std::move(gesture_list),
      base::RectifyCallback<GestureCallback>(std::move(result_callback)));
}

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_MOUSE_H_
