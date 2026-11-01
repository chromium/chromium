// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ENV_INPUT_STATE_CONTROLLER_H_
#define UI_AURA_ENV_INPUT_STATE_CONTROLLER_H_

#include <stdint.h>

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_observer.h"
#include "ui/events/pointer_details.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
class MouseEvent;
class TouchEvent;
}

namespace aura {
namespace test {
class EnvTestHelper;
}

class Env;
class Window;

class AURA_EXPORT EnvInputStateController : public WindowObserver {
 public:
  explicit EnvInputStateController(Env* env);

  EnvInputStateController(const EnvInputStateController&) = delete;
  EnvInputStateController& operator=(const EnvInputStateController&) = delete;

  ~EnvInputStateController() override;

  // `window` is the root window the event was dispatched through, and `target`
  // is the window the event was dispatched to (which may be null).
  void UpdateStateForMouseEvent(const Window* window,
                                Window* target,
                                const ui::MouseEvent& event);
  void UpdateStateForTouchEvent(Window* window, const ui::TouchEvent& event);
  void SetLastMouseLocation(const Window* root_window,
                            const gfx::Point& location_in_root) const;
  uint32_t touch_ids_down() const { return touch_ids_down_; }

  // See Env::ConsumeTouchForDrag().
  std::optional<gfx::Point> ConsumeTouchForDrag(const Window* window);

  // See Env::ConsumeLeftMousePressForDrag(). Unlike the Env method, this does
  // not check whether the left mouse button is still down.
  bool ConsumeLeftMousePressForDrag(const Window* window);

 private:
  friend class test::EnvTestHelper;

  // A touch that has been pressed and not yet released or cancelled.
  struct ActiveTouch {
    // The window the touch press was dispatched to, or null if that window
    // has since been destroyed.
    raw_ptr<Window> target = nullptr;
    // The most recent location of the touch, in screen coordinates.
    gfx::Point location_in_screen;
    // Whether this touch has already been used to start a drag.
    bool used_for_drag = false;
  };

  // WindowObserver:
  void OnWindowDestroying(Window* window) override;

  void AddActiveTouch(ui::PointerId id,
                      Window* window,
                      const gfx::Point& location_in_screen);
  void RemoveActiveTouch(ui::PointerId id);
  void ResetActiveTouches();

  // Records that the left mouse button was pressed on `window`, which may be
  // null if the press should not be associated with any window.
  void SetLeftMousePress(Window* window);
  void ResetLeftMousePress();

  // Starts observing `window` if it is not already observed.
  void ObserveTarget(Window* window);
  // Stops observing `window` if no active touch or mouse press targets it.
  void MaybeStopObservingTarget(Window* window);

  raw_ptr<Env> env_;
  // Touch ids that are currently down.
  uint32_t touch_ids_down_ = 0;

  base::flat_map<ui::PointerId, ActiveTouch> active_touches_;

  // The window the most recent left mouse press was dispatched to, or null if
  // there is no such press, the press was non-client, the left button has
  // since been released, or the window has since been destroyed.
  raw_ptr<Window> left_mouse_press_target_ = nullptr;
  // Whether the left mouse press on `left_mouse_press_target_` has already been
  // used to start a drag.
  bool left_mouse_press_used_for_drag_ = false;

  base::ScopedMultiSourceObservation<Window, WindowObserver>
      target_observations_{this};
};

}  // namespace aura

#endif  // UI_AURA_ENV_INPUT_STATE_CONTROLLER_H_
