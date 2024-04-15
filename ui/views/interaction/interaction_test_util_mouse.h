// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_MOUSE_H_
#define UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_MOUSE_H_

#include <list>
#include <memory>
#include <set>
#include <utility>
#include <variant>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
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

  // These represent mouse gestures of different types. They are implementation
  // details; prefer to use the static factory methods below.
  using MouseButtonGesture =
      std::pair<ui_controls::MouseButton, ui_controls::MouseButtonState>;
  using MouseMoveGesture = gfx::Point;
  using MouseGesture = std::variant<MouseMoveGesture, MouseButtonGesture>;
  using MouseGestures = std::list<MouseGesture>;

  // These factory methods create individual or compound gestures. They can be
  // chained together. Prefer these to directly constructing a MouseGesture.
  static MouseGesture MoveTo(gfx::Point point);
  static MouseGesture MouseDown(ui_controls::MouseButton button);
  static MouseGesture MouseUp(ui_controls::MouseButton button);
  static MouseGestures Click(ui_controls::MouseButton button);
  static MouseGestures DragAndHold(gfx::Point destination);
  static MouseGestures DragAndRelease(gfx::Point destination);

  // Set or get touch mode.
  //
  // `SetTouchMode(true)` returns false if touch is not supported. If it
  // succeeds, subsequent mouse inputs will be converted to equivalent touch
  // inputs.
  //
  // Notes:
  //  - This is an experimental feature and the API is subject to change.
  //     - See tracking bug at crbug.com/1428292 for current status.
  //  - Currently only Ash Chrome is supported.
  //  - Hover is not yet supported, only tap [up/down] and drag.
  //    - Moves without a finger down affect the next tap input but do not send
  //      events.
  //
  // To use this in an InteractiveViewsTest or InteractiveBrowserTest, use the
  // following syntax:
  //
  //   Check([this](){ return test_impl().mouse_util().SetTouchMode(true); })
  //
  // Afterwards, you can use mouse verbs as normal and they will convert to
  // equivalent touch inputs. We suggest using `Check()` so that the test will
  // fail if it's accidentally run on a system that doesn't yet support it.
  //
  // Alternatively, you can write a parameterized test which selectively tries
  // the test in touch-on and touch-off modes for platforms that support it, but
  // only in touch-off mode for those that don't. In these cases, the `Check()`
  // above changes to something like `...SetTouchMode(GetParam())`.
  bool SetTouchMode(bool touch_mode);
  bool GetTouchMode() const;

  // Perform the gesture or gestures specified, returns true on success.
  template <typename... Args>
  bool PerformGestures(gfx::NativeWindow window_hint, Args... gestures);

  // Cancels any pending actions and cleans up any resulting mouse state (i.e.
  // releases any buttons which were pressed).
  void CancelAllGestures();

 private:
  explicit InteractionTestUtilMouse(gfx::NativeWindow window);

  // Helper methods for adding gestures to a gesture list.
  static void AddGestures(MouseGestures& gestures, MouseGesture to_add);
  static void AddGestures(MouseGestures& gestures, MouseGestures to_add);

  bool PerformGesturesImpl(MouseGestures gestures,
                           gfx::NativeWindow window_hint);

  bool ShouldCancelDrag() const;
  void CancelFutureDrag();
  void CancelDragNow();

  bool SendButtonPress(const MouseButtonGesture& gesture,
                       gfx::NativeWindow window_hint,
                       bool sync,
                       base::OnceClosure on_complete);
  bool SendMove(const MouseMoveGesture& gesture,
                gfx::NativeWindow window_hint,
                bool sync,
                base::OnceClosure on_complete);

  // The set of mouse buttons currently depressed. Used to clean up on abort.
  std::set<ui_controls::MouseButton> buttons_down_;

  // Whether gestures are being executed.
  bool performing_gestures_ = false;

  // Whether the current sequence is canceled.
  bool canceled_ = false;

  // Whether we're in touch mode. In touch mode, touch events are sent instead
  // of mouse events. Moves without fingers down will not send events (but see
  // `touch_hover_point_` below).
  bool touch_mode_ = false;

  // Tracks the next place touch input should take place. It is affected by all
  // moves, regardless of whether any fingers are down, and you can use MoveTo
  // with no fingers to reposition the point.
  gfx::Point touch_hover_point_;

#if defined(USE_AURA)
  // Whether the mouse is currently being dragged.
  bool dragging_ = false;

  // These are used in order to clean up extraneous drags on Aura platforms;
  // without this it is possible for a drag loop to start and not exit,
  // preventing a test from completing.
  class DragEnder;
  const std::unique_ptr<DragEnder> drag_ender_;
#endif

  base::WeakPtrFactory<InteractionTestUtilMouse> weak_ptr_factory_{this};
};

template <typename... Args>
bool InteractionTestUtilMouse::PerformGestures(gfx::NativeWindow window_hint,
                                               Args... gestures) {
  MouseGestures gesture_list;
  (AddGestures(gesture_list, std::move(gestures)), ...);
  return PerformGesturesImpl(std::move(gesture_list), window_hint);
}

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_INTERACTION_INTERACTION_TEST_UTIL_MOUSE_H_
