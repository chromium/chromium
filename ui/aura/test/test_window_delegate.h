// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOW_DELEGATE_H_
#define UI_AURA_TEST_TEST_WINDOW_DELEGATE_H_

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
namespace test {

// WindowDelegate implementation with all methods stubbed out.
class TestWindowDelegate : public WindowDelegate {
 public:
  TestWindowDelegate();

  TestWindowDelegate(const TestWindowDelegate&) = delete;
  TestWindowDelegate& operator=(const TestWindowDelegate&) = delete;

  ~TestWindowDelegate() override;

  // Returns a TestWindowDelegate that delete itself when
  // the associated window is destroyed.
  static TestWindowDelegate* CreateSelfDestroyingDelegate();

  void set_window_component(int window_component) {
    window_component_ = window_component;
  }

  void set_minimum_size(const gfx::Size& minimum_size) {
    minimum_size_ = minimum_size;
  }

  void set_maximum_size(const gfx::Size& maximum_size) {
    maximum_size_ = maximum_size;
  }

  // Sets the return value for CanFocus(). Default is true.
  void set_can_focus(bool can_focus) { can_focus_ = can_focus; }

  void set_on_occlusion_changed(base::RepeatingClosure callback) {
    on_occlusion_changed_ = std::move(callback);
  }

  // Overridden from WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnWindowDestroying(Window* window) override;
  void OnWindowDestroyed(Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  void OnWindowOcclusionChanged(
      Window::OcclusionState old_occlusion_state,
      Window::OcclusionState new_occlusion_state) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(SkPath* mask) const override;

 private:
  int window_component_;
  bool delete_on_destroyed_;
  gfx::Size minimum_size_;
  gfx::Size maximum_size_;
  bool can_focus_;

  base::RepeatingClosure on_occlusion_changed_;
};

// A simple WindowDelegate implementation for these tests. It owns itself
// (deletes itself when the Window it is attached to is destroyed).
class ColorTestWindowDelegate : public TestWindowDelegate {
 public:
  explicit ColorTestWindowDelegate(SkColor color);

  ColorTestWindowDelegate(const ColorTestWindowDelegate&) = delete;
  ColorTestWindowDelegate& operator=(const ColorTestWindowDelegate&) = delete;

  ~ColorTestWindowDelegate() override;

  ui::KeyboardCode last_key_code() const { return last_key_code_; }

  // Overridden from TestWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnWindowDestroyed(Window* window) override;
  void OnPaint(const ui::PaintContext& context) override;

 private:
  SkColor color_;
  ui::KeyboardCode last_key_code_;
  gfx::Size window_size_;
};

// A simple WindowDelegate that has a hit-test mask.
class MaskedWindowDelegate : public TestWindowDelegate {
 public:
  explicit MaskedWindowDelegate(const gfx::Rect mask_rect);

  MaskedWindowDelegate(const MaskedWindowDelegate&) = delete;
  MaskedWindowDelegate& operator=(const MaskedWindowDelegate&) = delete;

  // Overridden from TestWindowDelegate:
  bool HasHitTestMask() const override;
  void GetHitTestMask(SkPath* mask) const override;

 private:
  gfx::Rect mask_rect_;
};

// Keeps track of mouse/key events.
class EventCountDelegate : public TestWindowDelegate {
 public:
  EventCountDelegate();

  EventCountDelegate(const EventCountDelegate&) = delete;
  EventCountDelegate& operator=(const EventCountDelegate&) = delete;

  // Overridden from TestWindowDelegate:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Returns the counts of mouse motion events in the
  // form of "<enter> <move> <leave>".
  std::string GetMouseMotionCountsAndReset();

  // Returns the counts of mouse button events in the
  // form of "<press> <release>".
  std::string GetMouseButtonCountsAndReset();

  // Returns the counts of key events in the form of
  // "<press> <release>".
  std::string GetKeyCountsAndReset();

  // Returns number of gesture events.
  int GetGestureCountAndReset();

 private:
  int mouse_enter_count_;
  int mouse_move_count_;
  int mouse_leave_count_;
  int mouse_press_count_;
  int mouse_release_count_;
  int key_press_count_;
  int key_release_count_;
  int gesture_count_;
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_WINDOW_DELEGATE_H_
