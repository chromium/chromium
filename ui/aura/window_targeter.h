// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TARGETER_H_
#define UI_AURA_WINDOW_TARGETER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/events/event_targeter.h"
#include "ui/gfx/geometry/insets.h"

namespace gfx {
class Point;
class Rect;
}

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace aura {

class Window;

class AURA_EXPORT WindowTargeter : public ui::EventTargeter {
 public:
  WindowTargeter();

  WindowTargeter(const WindowTargeter&) = delete;
  WindowTargeter& operator=(const WindowTargeter&) = delete;

  ~WindowTargeter() override;

  using HitTestRects = std::vector<gfx::Rect>;

  // Returns true if |window| or one of its descendants can be a target of
  // |event|. This requires that |window| and its descendants are not
  // prohibited from accepting the event, and that the event is within an
  // actionable region of the target's bounds. Note that the location etc. of
  // |event| is in |window|'s parent's coordinate system.
  virtual bool SubtreeShouldBeExploredForEvent(Window* window,
                                               const ui::LocatedEvent& event);

  // Returns true if the |target| is accepting LocatedEvents, false otherwise.
  // |hit_test_rect_mouse| and |hit_test_rect_touch| must be not null and return
  // the bounds that can be used for hit testing. The default implementation
  // extends the |target|'s |bounds()| by insets provided with SetInsets().
  // This can be used to extend the hit-test area for touch events and make
  // targeting windows with imprecise input devices easier.
  // Returned rectangles are in |target|'s parent's coordinates.
  virtual bool GetHitTestRects(Window* target,
                               gfx::Rect* hit_test_rect_mouse,
                               gfx::Rect* hit_test_rect_touch) const;

  // Returns additional hit-test areas or nullptr when there are none. Used when
  // a window needs a complex shape hit-test area. This additional area is
  // clipped to |hit_test_rect_mouse| returned by GetHitTestRects or the window
  // bounds when GetHitTestRects is not overridden.
  // Returned rectangles are in |target|'s coordinates.
  virtual std::unique_ptr<HitTestRects> GetExtraHitTestShapeRects(
      Window* target) const;

  // Sets additional mouse and touch insets that are factored into the hit-test
  // regions returned by GetHitTestRects.
  void SetInsets(const gfx::Insets& mouse_and_touch_extend);
  void SetInsets(const gfx::Insets& mouse_extend,
                 const gfx::Insets& touch_extend);

  // If there is a target that takes priority over normal WindowTargeter (such
  // as a capture window) this returns it.
  Window* GetPriorityTargetInRootWindow(Window* root_window,
                                        const ui::LocatedEvent& event);

  Window* FindTargetInRootWindow(Window* root_window,
                                 const ui::LocatedEvent& event);

  // ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override;
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override;
  ui::EventSink* GetNewEventSinkForEvent(const ui::EventTarget* current_root,
                                         ui::EventTarget* target,
                                         ui::Event* in_out_event) override;

  Window* FindTargetForKeyEvent(Window* root_window);

 protected:
  aura::Window* window() { return window_; }
  const aura::Window* window() const { return window_; }

  // This is called by Window when the targeter is set on a window.
  virtual void OnInstalled(Window* window);

  // Same as FindTargetForEvent(), but used for positional events. The location
  // etc. of |event| are in |window|'s coordinate system. When finding the
  // target for the event, the targeter can mutate the |event| (e.g. change the
  // coordinate to be in the returned target's coordinate system) so that it can
  // be dispatched to the target without any further modification.
  virtual Window* FindTargetForLocatedEvent(Window* window,
                                            ui::LocatedEvent* event);

  // Returns false if neither |window| nor any of its descendants are allowed
  // to accept |event| for reasons unrelated to the event's location or the
  // target's bounds. For example, overrides of this function may consider
  // attributes such as the visibility or enabledness of |window|. Note that
  // the location etc. of |event| is in |window|'s parent's coordinate system.
  virtual bool SubtreeCanAcceptEvent(Window* window,
                                     const ui::LocatedEvent& event) const;

  // Returns whether the location of the event is in an actionable region of the
  // target. Note that the location etc. of |event| is in the |window|'s
  // parent's coordinate system.
  virtual bool EventLocationInsideBounds(Window* target,
                                         const ui::LocatedEvent& event) const;

  // Returns true if the hit testing (GetHitTestRects()) should use the
  // extended bounds.
  virtual bool ShouldUseExtendedBounds(const aura::Window* w) const;

  const gfx::Insets& mouse_extend() const { return mouse_extend_; }
  const gfx::Insets& touch_extend() const { return touch_extend_; }

  static gfx::Point ConvertEventLocationToWindowCoordinates(
      Window* window,
      const ui::LocatedEvent& event);

 private:
  // To call OnInstalled().
  friend class Window;

  enum class BoundsType {
    kMouse,
    kTouch,
    kGesture,
  };

  Window* FindTargetForNonKeyEvent(Window* root_window, ui::Event* event);
  Window* FindTargetForLocatedEventRecursively(Window* root_window,
                                               ui::LocatedEvent* event);

  // Whether |point| is inside |window| or its descendents using |bounds_type|
  // as a rect source. |point| should be relative to |window|.
  bool PointInsideBounds(Window* window,
                         BoundsType bounds_type,
                         const gfx::Point& point) const;

  // The Window this WindowTargeter is installed on. Null if not attached to a
  // Window.
  raw_ptr<aura::Window> window_ = nullptr;

  gfx::Insets mouse_extend_;
  gfx::Insets touch_extend_;
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TARGETER_H_
