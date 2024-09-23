// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_window_delegate.h"

#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

namespace aura {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// TestWindowDelegate

TestWindowDelegate::TestWindowDelegate()
    : window_component_(HTCLIENT),
      delete_on_destroyed_(false),
      can_focus_(true) {
}

TestWindowDelegate::~TestWindowDelegate() {
}

// static
TestWindowDelegate* TestWindowDelegate::CreateSelfDestroyingDelegate() {
  TestWindowDelegate* delegate = new TestWindowDelegate;
  delegate->delete_on_destroyed_ = true;
  return delegate;
}

gfx::Size TestWindowDelegate::GetMinimumSize() const {
  return minimum_size_;
}

gfx::Size TestWindowDelegate::GetMaximumSize() const {
  return maximum_size_;
}

void TestWindowDelegate::OnBoundsChanged(const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
}

gfx::NativeCursor TestWindowDelegate::GetCursor(const gfx::Point& point) {
  return gfx::NativeCursor{};
}

int TestWindowDelegate::GetNonClientComponent(const gfx::Point& point) const {
  return window_component_;
}

bool TestWindowDelegate::ShouldDescendIntoChildForEventHandling(
      Window* child,
      const gfx::Point& location) {
  return true;
}

bool TestWindowDelegate::CanFocus() {
  return can_focus_;
}

void TestWindowDelegate::OnCaptureLost() {
}

void TestWindowDelegate::OnPaint(const ui::PaintContext& context) {
}

void TestWindowDelegate::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

void TestWindowDelegate::OnWindowDestroying(Window* window) {
}

void TestWindowDelegate::OnWindowDestroyed(Window* window) {
  if (delete_on_destroyed_)
    delete this;
}

void TestWindowDelegate::OnWindowTargetVisibilityChanged(bool visible) {
}

void TestWindowDelegate::OnWindowOcclusionChanged(
    Window::OcclusionState old_occlusion_state,
    Window::OcclusionState new_occlusion_state) {
  if (on_occlusion_changed_) {
    on_occlusion_changed_.Run();
  }
}

bool TestWindowDelegate::HasHitTestMask() const {
  return false;
}

void TestWindowDelegate::GetHitTestMask(SkPath* mask) const {}

////////////////////////////////////////////////////////////////////////////////
// ColorTestWindowDelegate

ColorTestWindowDelegate::ColorTestWindowDelegate(SkColor color)
    : color_(color),
      last_key_code_(ui::VKEY_UNKNOWN) {
}

ColorTestWindowDelegate::~ColorTestWindowDelegate() {
}

void ColorTestWindowDelegate::OnBoundsChanged(const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  window_size_ = new_bounds.size();
}

void ColorTestWindowDelegate::OnKeyEvent(ui::KeyEvent* event) {
  last_key_code_ = event->key_code();
  event->SetHandled();
}

void ColorTestWindowDelegate::OnWindowDestroyed(Window* window) {
  delete this;
}

void ColorTestWindowDelegate::OnPaint(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, window_size_);
  recorder.canvas()->DrawColor(color_, SkBlendMode::kSrc);
}

////////////////////////////////////////////////////////////////////////////////
// MaskedWindowDelegate

MaskedWindowDelegate::MaskedWindowDelegate(const gfx::Rect mask_rect)
    : mask_rect_(mask_rect) {
}

bool MaskedWindowDelegate::HasHitTestMask() const {
  return true;
}

void MaskedWindowDelegate::GetHitTestMask(SkPath* mask) const {
  mask->addRect(RectToSkRect(mask_rect_));
}

////////////////////////////////////////////////////////////////////////////////
// EventCountDelegate

EventCountDelegate::EventCountDelegate()
    : mouse_enter_count_(0),
      mouse_move_count_(0),
      mouse_leave_count_(0),
      mouse_press_count_(0),
      mouse_release_count_(0),
      key_press_count_(0),
      key_release_count_(0),
      gesture_count_(0) {
}

void EventCountDelegate::OnKeyEvent(ui::KeyEvent* event) {
  switch (event->type()) {
    case ui::EventType::kKeyPressed:
      key_press_count_++;
      break;
    case ui::EventType::kKeyReleased:
      key_release_count_++;
      break;
    default:
      break;
  }
}

void EventCountDelegate::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::EventType::kMouseMoved:
      mouse_move_count_++;
      break;
    case ui::EventType::kMouseEntered:
      mouse_enter_count_++;
      break;
    case ui::EventType::kMouseExited:
      mouse_leave_count_++;
      break;
    case ui::EventType::kMousePressed:
      mouse_press_count_++;
      break;
    case ui::EventType::kMouseReleased:
      mouse_release_count_++;
      break;
    default:
      break;
  }
}

void EventCountDelegate::OnGestureEvent(ui::GestureEvent* event) {
  gesture_count_++;
}

std::string EventCountDelegate::GetMouseMotionCountsAndReset() {
  std::string result = base::StringPrintf("%d %d %d",
                                          mouse_enter_count_,
                                          mouse_move_count_,
                                          mouse_leave_count_);
  mouse_enter_count_ = 0;
  mouse_move_count_ = 0;
  mouse_leave_count_ = 0;
  return result;
}

std::string EventCountDelegate::GetMouseButtonCountsAndReset() {
  std::string result = base::StringPrintf("%d %d",
                                          mouse_press_count_,
                                          mouse_release_count_);
  mouse_press_count_ = 0;
  mouse_release_count_ = 0;
  return result;
}


std::string EventCountDelegate::GetKeyCountsAndReset() {
  std::string result = base::StringPrintf("%d %d",
                                          key_press_count_,
                                          key_release_count_);
  key_press_count_ = 0;
  key_release_count_ = 0;
  return result;
}

int EventCountDelegate::GetGestureCountAndReset() {
  int gesture_count = gesture_count_;
  gesture_count_ = 0;
  return gesture_count;
}

}  // namespace test
}  // namespace aura
