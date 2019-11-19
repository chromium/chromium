// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"

#include <limits.h>

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/visibility_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/aura_test_utils.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/window_test_api.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/overlay_transform_utils.h"
#include "ui/gfx/skia_util.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(const char*)

namespace {

enum class DeletionOrder {
  LAYOUT_MANAGER_FIRST,
  PROPERTY_FIRST,
  UNKNOWN,
};

class DeletionTracker {
 public:
  DeletionTracker() {}
  ~DeletionTracker() {}

  DeletionOrder order() const { return order_; }
  bool property_deleted() const { return property_deleted_; }
  bool layout_manager_deleted() const { return layout_manager_deleted_; }

  void PropertyDeleted() {
    property_deleted_ = true;
    if (order_ == DeletionOrder::UNKNOWN)
      order_ = DeletionOrder::PROPERTY_FIRST;
  }

  void LayoutManagerDeleted() {
    layout_manager_deleted_ = true;
    if (order_ == DeletionOrder::UNKNOWN)
      order_ = DeletionOrder::LAYOUT_MANAGER_FIRST;
  }

 private:
  bool property_deleted_ = false;
  bool layout_manager_deleted_ = false;
  DeletionOrder order_ = DeletionOrder::UNKNOWN;

  DISALLOW_COPY_AND_ASSIGN(DeletionTracker);
};

class DeletionTestProperty {
 public:
  explicit DeletionTestProperty(DeletionTracker* tracker) : tracker_(tracker) {}
  ~DeletionTestProperty() { tracker_->PropertyDeleted(); }

 private:
  DeletionTracker* tracker_;

  DISALLOW_COPY_AND_ASSIGN(DeletionTestProperty);
};

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(DeletionTestProperty,
                                   kDeletionTestPropertyKey,
                                   nullptr)

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(DeletionTestProperty*)

namespace aura {
namespace test {
namespace {

class WindowTest : public AuraTestBase {
 public:
  WindowTest() : max_separation_(0) {
  }

  void SetUp() override {
    AuraTestBase::SetUp();
    // TODO: there needs to be an easier way to do this.
    max_separation_ = ui::GestureConfiguration::GetInstance()
                          ->max_separation_for_gesture_touches_in_pixels();
    ui::GestureConfiguration::GetInstance()
        ->set_max_separation_for_gesture_touches_in_pixels(0);
  }

  void TearDown() override {
    AuraTestBase::TearDown();
    ui::GestureConfiguration::GetInstance()
        ->set_max_separation_for_gesture_touches_in_pixels(max_separation_);
  }

 private:
  float max_separation_;

  DISALLOW_COPY_AND_ASSIGN(WindowTest);
};

// Used for verifying destruction methods are invoked.
class DestroyTrackingDelegateImpl : public TestWindowDelegate {
 public:
  DestroyTrackingDelegateImpl()
      : destroying_count_(0),
        destroyed_count_(0),
        in_destroying_(false) {}

  void clear_destroying_count() { destroying_count_ = 0; }
  int destroying_count() const { return destroying_count_; }

  void clear_destroyed_count() { destroyed_count_ = 0; }
  int destroyed_count() const { return destroyed_count_; }

  bool in_destroying() const { return in_destroying_; }

  void OnWindowDestroying(Window* window) override {
    EXPECT_FALSE(in_destroying_);
    in_destroying_ = true;
    destroying_count_++;
  }

  void OnWindowDestroyed(Window* window) override {
    EXPECT_TRUE(in_destroying_);
    in_destroying_ = false;
    destroyed_count_++;
  }

 private:
  int destroying_count_;
  int destroyed_count_;
  bool in_destroying_;

  DISALLOW_COPY_AND_ASSIGN(DestroyTrackingDelegateImpl);
};

// Used to verify that when OnWindowDestroying is invoked the parent is also
// is in the process of being destroyed.
class ChildWindowDelegateImpl : public DestroyTrackingDelegateImpl {
 public:
  explicit ChildWindowDelegateImpl(
      DestroyTrackingDelegateImpl* parent_delegate)
      : parent_delegate_(parent_delegate) {
  }

  void OnWindowDestroying(Window* window) override {
    EXPECT_TRUE(parent_delegate_->in_destroying());
    DestroyTrackingDelegateImpl::OnWindowDestroying(window);
  }

 private:
  DestroyTrackingDelegateImpl* parent_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChildWindowDelegateImpl);
};

// Used to verify that a Window is removed from its parent when
// OnWindowDestroyed is called.
class DestroyOrphanDelegate : public TestWindowDelegate {
 public:
  DestroyOrphanDelegate() : window_(NULL) {
  }

  void set_window(Window* window) { window_ = window; }

  void OnWindowDestroyed(Window* window) override {
    EXPECT_FALSE(window_->parent());
  }

 private:
  Window* window_;
  DISALLOW_COPY_AND_ASSIGN(DestroyOrphanDelegate);
};

// Used in verifying mouse capture.
class CaptureWindowDelegateImpl : public TestWindowDelegate {
 public:
  CaptureWindowDelegateImpl() {
    ResetCounts();
  }

  void ResetCounts() {
    capture_changed_event_count_ = 0;
    capture_lost_count_ = 0;
    mouse_event_count_ = 0;
    touch_event_count_ = 0;
    gesture_event_count_ = 0;
  }

  int capture_changed_event_count() const {
    return capture_changed_event_count_;
  }
  int capture_lost_count() const { return capture_lost_count_; }
  int mouse_event_count() const { return mouse_event_count_; }
  int touch_event_count() const { return touch_event_count_; }
  int gesture_event_count() const { return gesture_event_count_; }

  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED)
      capture_changed_event_count_++;
    mouse_event_count_++;
  }
  void OnTouchEvent(ui::TouchEvent* event) override { touch_event_count_++; }
  void OnGestureEvent(ui::GestureEvent* event) override {
    gesture_event_count_++;
  }
  void OnCaptureLost() override { capture_lost_count_++; }

 private:
  int capture_changed_event_count_;
  int capture_lost_count_;
  int mouse_event_count_;
  int touch_event_count_;
  int gesture_event_count_;

  DISALLOW_COPY_AND_ASSIGN(CaptureWindowDelegateImpl);
};

// Keeps track of the location of the gesture.
class GestureTrackPositionDelegate : public TestWindowDelegate {
 public:
  GestureTrackPositionDelegate() {}

  void OnGestureEvent(ui::GestureEvent* event) override {
    position_ = event->location();
    event->StopPropagation();
  }

  const gfx::Point& position() const { return position_; }

 private:
  gfx::Point position_;

  DISALLOW_COPY_AND_ASSIGN(GestureTrackPositionDelegate);
};

base::TimeTicks getTime() {
  return ui::EventTimeForNow();
}

class SelfEventHandlingWindowDelegate : public TestWindowDelegate {
 public:
  SelfEventHandlingWindowDelegate() {}

  bool ShouldDescendIntoChildForEventHandling(
      Window* child,
      const gfx::Point& location) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SelfEventHandlingWindowDelegate);
};

// The delegate deletes itself when the window is being destroyed.
class DestroyWindowDelegate : public TestWindowDelegate {
 public:
  DestroyWindowDelegate() {}

 private:
  ~DestroyWindowDelegate() override {}

  // Overridden from WindowDelegate.
  void OnWindowDestroyed(Window* window) override { delete this; }

  DISALLOW_COPY_AND_ASSIGN(DestroyWindowDelegate);
};

void OffsetBounds(Window* window, int horizontal, int vertical) {
  gfx::Rect bounds = window->bounds();
  bounds.Offset(horizontal, vertical);
  window->SetBounds(bounds);
}

TEST_F(WindowTest, GetChildById) {
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  std::unique_ptr<Window> w11(CreateTestWindowWithId(11, w1.get()));
  std::unique_ptr<Window> w111(CreateTestWindowWithId(111, w11.get()));
  std::unique_ptr<Window> w12(CreateTestWindowWithId(12, w1.get()));

  EXPECT_EQ(NULL, w1->GetChildById(57));
  EXPECT_EQ(w12.get(), w1->GetChildById(12));
  EXPECT_EQ(w111.get(), w1->GetChildById(111));
}

// Make sure that Window::Contains correctly handles children, grandchildren,
// and not containing NULL or parents.
TEST_F(WindowTest, Contains) {
  Window parent(NULL);
  parent.Init(ui::LAYER_NOT_DRAWN);
  Window child1(NULL);
  child1.Init(ui::LAYER_NOT_DRAWN);
  Window child2(NULL);
  child2.Init(ui::LAYER_NOT_DRAWN);

  parent.AddChild(&child1);
  child1.AddChild(&child2);

  EXPECT_TRUE(parent.Contains(&parent));
  EXPECT_TRUE(parent.Contains(&child1));
  EXPECT_TRUE(parent.Contains(&child2));

  EXPECT_FALSE(parent.Contains(NULL));
  EXPECT_FALSE(child1.Contains(&parent));
  EXPECT_FALSE(child2.Contains(&child1));
}

TEST_F(WindowTest, ContainsPointInRoot) {
  std::unique_ptr<Window> w(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(10, 10, 5, 5), root_window()));
  EXPECT_FALSE(w->ContainsPointInRoot(gfx::Point(9, 9)));
  EXPECT_TRUE(w->ContainsPointInRoot(gfx::Point(10, 10)));
  EXPECT_TRUE(w->ContainsPointInRoot(gfx::Point(14, 14)));
  EXPECT_FALSE(w->ContainsPointInRoot(gfx::Point(15, 15)));
  EXPECT_FALSE(w->ContainsPointInRoot(gfx::Point(20, 20)));
}

TEST_F(WindowTest, ContainsPoint) {
  std::unique_ptr<Window> w(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(10, 10, 5, 5), root_window()));
  EXPECT_TRUE(w->ContainsPoint(gfx::Point(0, 0)));
  EXPECT_TRUE(w->ContainsPoint(gfx::Point(4, 4)));
  EXPECT_FALSE(w->ContainsPoint(gfx::Point(5, 5)));
  EXPECT_FALSE(w->ContainsPoint(gfx::Point(10, 10)));
}

TEST_F(WindowTest, ConvertPointToWindow) {
  // Window::ConvertPointToWindow is mostly identical to
  // Layer::ConvertPointToLayer, except NULL values for |source| are permitted,
  // in which case the function just returns.
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  gfx::Point reference_point(100, 100);
  gfx::Point test_point = reference_point;
  Window::ConvertPointToTarget(NULL, w1.get(), &test_point);
  EXPECT_EQ(reference_point, test_point);
}

TEST_F(WindowTest, MoveCursorTo) {
  std::unique_ptr<Window> w1(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500), root_window()));
  std::unique_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(5, 5, 100, 100), w1.get()));
  std::unique_ptr<Window> w111(
      CreateTestWindow(SK_ColorCYAN, 111, gfx::Rect(5, 5, 75, 75), w11.get()));
  std::unique_ptr<Window> w1111(
      CreateTestWindow(SK_ColorRED, 1111, gfx::Rect(5, 5, 50, 50), w111.get()));

  Window* root = root_window();
  root->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("10,10",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
  w1->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("20,20",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
  w11->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("25,25",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
  w111->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("30,30",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
  w1111->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("35,35",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
}

TEST_F(WindowTest, ContainsMouse) {
  std::unique_ptr<Window> w(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500), root_window()));
  w->Show();
  WindowTestApi w_test_api(w.get());
  Window* root = root_window();
  root->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_TRUE(w_test_api.ContainsMouse());
  root->MoveCursorTo(gfx::Point(9, 10));
  EXPECT_FALSE(w_test_api.ContainsMouse());
}

// Tests that the root window gets a valid LocalSurfaceId.
TEST_F(WindowTest, RootWindowHasValidLocalSurfaceId) {
  EXPECT_TRUE(root_window()
                  ->GetLocalSurfaceIdAllocation()
                  .local_surface_id()
                  .is_valid());
}

TEST_F(WindowTest, WindowEmbeddingClientHasValidLocalSurfaceId) {
  std::unique_ptr<Window> window(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(10, 10, 300, 200), root_window()));
  test::WindowTestApi(window.get()).DisableFrameSinkRegistration();
  window->SetEmbedFrameSinkId(viz::FrameSinkId(0, 1));
  EXPECT_TRUE(
      window->GetLocalSurfaceIdAllocation().local_surface_id().is_valid());
}

// Test Window::ConvertPointToWindow() with transform to root_window.
TEST_F(WindowTest, MoveCursorToWithTransformRootWindow) {
  gfx::Transform transform;
  transform.Translate(100.0, 100.0);
  transform = transform * OverlayTransformToTransform(
                              gfx::OVERLAY_TRANSFORM_ROTATE_90, gfx::SizeF());
  transform.Scale(2.0, 5.0);
  host()->SetRootTransform(transform);
  host()->MoveCursorToLocationInDIP(gfx::Point(10, 10));
#if !defined(OS_WIN)
  // TODO(yoshiki): fix this to build on Windows. See crbug.com/133413.OD
  EXPECT_EQ("50,120", QueryLatestMousePositionRequestInHost(host()).ToString());
#endif
  EXPECT_EQ("10,10",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
}

// Tests Window::ConvertPointToWindow() with transform to non-root windows.
TEST_F(WindowTest, MoveCursorToWithTransformWindow) {
  std::unique_ptr<Window> w1(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500), root_window()));

  gfx::Transform transform1;
  transform1.Scale(2, 2);
  w1->SetTransform(transform1);
  w1->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("30,30",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());

  gfx::Transform transform2;
  transform2.Translate(-10, 20);
  w1->SetTransform(transform2);
  w1->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("10,40",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());

  gfx::Transform transform3;
  transform3 = transform3 * OverlayTransformToTransform(
                                gfx::OVERLAY_TRANSFORM_ROTATE_90, gfx::SizeF());
  w1->SetTransform(transform3);
  w1->MoveCursorTo(gfx::Point(5, 5));
  EXPECT_EQ("5,15",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());

  gfx::Transform transform4;
  transform4.Translate(100.0, 100.0);
  transform4 = transform4 * OverlayTransformToTransform(
                                gfx::OVERLAY_TRANSFORM_ROTATE_90, gfx::SizeF());
  transform4.Scale(2.0, 5.0);
  w1->SetTransform(transform4);
  w1->MoveCursorTo(gfx::Point(10, 10));
  EXPECT_EQ("60,130",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
}

// Test Window::ConvertPointToWindow() with complex transforms to both root and
// non-root windows.
// Test Window::ConvertPointToWindow() with transform to root_window.
TEST_F(WindowTest, MoveCursorToWithComplexTransform) {
  std::unique_ptr<Window> w1(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500), root_window()));
  std::unique_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(5, 5, 100, 100), w1.get()));
  std::unique_ptr<Window> w111(
      CreateTestWindow(SK_ColorCYAN, 111, gfx::Rect(5, 5, 75, 75), w11.get()));
  std::unique_ptr<Window> w1111(
      CreateTestWindow(SK_ColorRED, 1111, gfx::Rect(5, 5, 50, 50), w111.get()));

  // The root window expects transforms that produce integer rects.
  gfx::Transform root_transform;
  root_transform.Translate(60.0, 70.0);
  root_transform =
      root_transform * OverlayTransformToTransform(
                           gfx::OVERLAY_TRANSFORM_ROTATE_270, gfx::SizeF());
  root_transform.Translate(-50.0, -50.0);
  root_transform.Scale(2.0, 3.0);

  gfx::Transform transform;
  transform.Translate(10.0, 20.0);
  transform.Rotate(10.0);
  transform.Scale(0.3f, 0.5f);
  host()->SetRootTransform(root_transform);
  w1->SetTransform(transform);
  w11->SetTransform(transform);
  w111->SetTransform(transform);
  w1111->SetTransform(transform);

  w1111->MoveCursorTo(gfx::Point(10, 10));

#if !defined(OS_WIN)
  // TODO(yoshiki): fix this to build on Windows. See crbug.com/133413.
  EXPECT_EQ("169,80", QueryLatestMousePositionRequestInHost(host()).ToString());
#endif
  EXPECT_EQ("20,53",
            display::Screen::GetScreen()->GetCursorScreenPoint().ToString());
}

// Tests that we do not crash when a Window is destroyed by going out of
// scope (as opposed to being explicitly deleted by its WindowDelegate).
TEST_F(WindowTest, NoCrashOnWindowDelete) {
  CaptureWindowDelegateImpl delegate;
  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), root_window()));
}

TEST_F(WindowTest, GetEventHandlerForPoint) {
  std::unique_ptr<Window> w1(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(10, 10, 500, 500), root_window()));
  std::unique_ptr<Window> w11(
      CreateTestWindow(SK_ColorGREEN, 11, gfx::Rect(5, 5, 100, 100), w1.get()));
  std::unique_ptr<Window> w111(
      CreateTestWindow(SK_ColorCYAN, 111, gfx::Rect(5, 5, 75, 75), w11.get()));
  std::unique_ptr<Window> w1111(
      CreateTestWindow(SK_ColorRED, 1111, gfx::Rect(5, 5, 50, 50), w111.get()));
  std::unique_ptr<Window> w12(CreateTestWindow(
      SK_ColorMAGENTA, 12, gfx::Rect(10, 420, 25, 25), w1.get()));
  std::unique_ptr<Window> w121(
      CreateTestWindow(SK_ColorYELLOW, 121, gfx::Rect(5, 5, 5, 5), w12.get()));
  std::unique_ptr<Window> w13(
      CreateTestWindow(SK_ColorGRAY, 13, gfx::Rect(5, 470, 50, 50), w1.get()));

  Window* root = root_window();
  w1->parent()->SetBounds(gfx::Rect(500, 500));
  EXPECT_EQ(NULL, root->GetEventHandlerForPoint(gfx::Point(5, 5)));
  EXPECT_EQ(w1.get(), root->GetEventHandlerForPoint(gfx::Point(11, 11)));
  EXPECT_EQ(w11.get(), root->GetEventHandlerForPoint(gfx::Point(16, 16)));
  EXPECT_EQ(w111.get(), root->GetEventHandlerForPoint(gfx::Point(21, 21)));
  EXPECT_EQ(w1111.get(), root->GetEventHandlerForPoint(gfx::Point(26, 26)));
  EXPECT_EQ(w12.get(), root->GetEventHandlerForPoint(gfx::Point(21, 431)));
  EXPECT_EQ(w121.get(), root->GetEventHandlerForPoint(gfx::Point(26, 436)));
  EXPECT_EQ(w13.get(), root->GetEventHandlerForPoint(gfx::Point(26, 481)));
}

TEST_F(WindowTest, GetEventHandlerForPointInCornerOfChildBounds) {
  // If our child is flush to our top-left corner it gets events just inside the
  // window edges.
  std::unique_ptr<Window> parent(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(10, 20, 400, 500), root_window()));
  std::unique_ptr<Window> child(
      CreateTestWindow(SK_ColorRED, 2, gfx::Rect(0, 0, 60, 70), parent.get()));
  EXPECT_EQ(child.get(), parent->GetEventHandlerForPoint(gfx::Point(0, 0)));
  EXPECT_EQ(child.get(), parent->GetEventHandlerForPoint(gfx::Point(1, 1)));
}

TEST_F(WindowTest, GetEventHandlerForPointWithOverrideDescendingOrder) {
  std::unique_ptr<SelfEventHandlingWindowDelegate> parent_delegate(
      new SelfEventHandlingWindowDelegate);
  std::unique_ptr<Window> parent(CreateTestWindowWithDelegate(
      parent_delegate.get(), 1, gfx::Rect(10, 20, 400, 500), root_window()));
  std::unique_ptr<Window> child(CreateTestWindow(
      SK_ColorRED, 2, gfx::Rect(0, 0, 390, 480), parent.get()));

  // We can override ShouldDescendIntoChildForEventHandling to make the parent
  // grab all events.
  EXPECT_EQ(parent.get(), parent->GetEventHandlerForPoint(gfx::Point(0, 0)));
  EXPECT_EQ(parent.get(), parent->GetEventHandlerForPoint(gfx::Point(50, 50)));
}

TEST_F(WindowTest, GetToplevelWindow) {
  const gfx::Rect kBounds(0, 0, 10, 10);
  TestWindowDelegate delegate;

  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  std::unique_ptr<Window> w11(
      CreateTestWindowWithDelegate(&delegate, 11, kBounds, w1.get()));
  std::unique_ptr<Window> w111(CreateTestWindowWithId(111, w11.get()));
  std::unique_ptr<Window> w1111(
      CreateTestWindowWithDelegate(&delegate, 1111, kBounds, w111.get()));

  EXPECT_TRUE(root_window()->GetToplevelWindow() == NULL);
  EXPECT_TRUE(w1->GetToplevelWindow() == NULL);
  EXPECT_EQ(w11.get(), w11->GetToplevelWindow());
  EXPECT_EQ(w11.get(), w111->GetToplevelWindow());
  EXPECT_EQ(w11.get(), w1111->GetToplevelWindow());
}

class AddedToRootWindowObserver : public WindowObserver {
 public:
  AddedToRootWindowObserver() : called_(false) {}

  void OnWindowAddedToRootWindow(Window* window) override { called_ = true; }

  bool called() const { return called_; }

 private:
  bool called_;

  DISALLOW_COPY_AND_ASSIGN(AddedToRootWindowObserver);
};

TEST_F(WindowTest, WindowAddedToRootWindowShouldNotifyChildAndNotParent) {
  AddedToRootWindowObserver parent_observer;
  AddedToRootWindowObserver child_observer;
  std::unique_ptr<Window> parent_window(
      CreateTestWindowWithId(1, root_window()));
  std::unique_ptr<Window> child_window(new Window(NULL));
  child_window->Init(ui::LAYER_TEXTURED);
  child_window->Show();

  parent_window->AddObserver(&parent_observer);
  child_window->AddObserver(&child_observer);

  parent_window->AddChild(child_window.get());

  EXPECT_FALSE(parent_observer.called());
  EXPECT_TRUE(child_observer.called());

  parent_window->RemoveObserver(&parent_observer);
  child_window->RemoveObserver(&child_observer);
}

// Various destruction assertions.
TEST_F(WindowTest, DestroyTest) {
  DestroyTrackingDelegateImpl parent_delegate;
  ChildWindowDelegateImpl child_delegate(&parent_delegate);
  {
    std::unique_ptr<Window> parent(CreateTestWindowWithDelegate(
        &parent_delegate, 0, gfx::Rect(), root_window()));
    CreateTestWindowWithDelegate(&child_delegate, 0, gfx::Rect(), parent.get());
  }
  // Both the parent and child should have been destroyed.
  EXPECT_EQ(1, parent_delegate.destroying_count());
  EXPECT_EQ(1, parent_delegate.destroyed_count());
  EXPECT_EQ(1, child_delegate.destroying_count());
  EXPECT_EQ(1, child_delegate.destroyed_count());
}

// Tests that a window is orphaned before OnWindowDestroyed is called.
TEST_F(WindowTest, OrphanedBeforeOnDestroyed) {
  TestWindowDelegate parent_delegate;
  DestroyOrphanDelegate child_delegate;
  {
    std::unique_ptr<Window> parent(CreateTestWindowWithDelegate(
        &parent_delegate, 0, gfx::Rect(), root_window()));
    std::unique_ptr<Window> child(CreateTestWindowWithDelegate(
        &child_delegate, 0, gfx::Rect(), parent.get()));
    child_delegate.set_window(child.get());
  }
}

// Make sure StackChildAtTop moves both the window and layer to the front.
TEST_F(WindowTest, StackChildAtTop) {
  Window parent(NULL);
  parent.Init(ui::LAYER_NOT_DRAWN);
  Window child1(NULL);
  child1.Init(ui::LAYER_NOT_DRAWN);
  Window child2(NULL);
  child2.Init(ui::LAYER_NOT_DRAWN);

  parent.AddChild(&child1);
  parent.AddChild(&child2);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[0]);
  EXPECT_EQ(&child2, parent.children()[1]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[1]);

  parent.StackChildAtTop(&child1);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[1]);
  EXPECT_EQ(&child2, parent.children()[0]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
}

// Make sure StackChildBelow works.
TEST_F(WindowTest, StackChildBelow) {
  Window parent(NULL);
  parent.Init(ui::LAYER_NOT_DRAWN);
  Window child1(NULL);
  child1.Init(ui::LAYER_NOT_DRAWN);
  child1.set_id(1);
  Window child2(NULL);
  child2.Init(ui::LAYER_NOT_DRAWN);
  child2.set_id(2);
  Window child3(NULL);
  child3.Init(ui::LAYER_NOT_DRAWN);
  child3.set_id(3);

  parent.AddChild(&child1);
  parent.AddChild(&child2);
  parent.AddChild(&child3);
  EXPECT_EQ("1 2 3", ChildWindowIDsAsString(&parent));

  parent.StackChildBelow(&child1, &child2);
  EXPECT_EQ("1 2 3", ChildWindowIDsAsString(&parent));

  parent.StackChildBelow(&child2, &child1);
  EXPECT_EQ("2 1 3", ChildWindowIDsAsString(&parent));

  parent.StackChildBelow(&child3, &child2);
  EXPECT_EQ("3 2 1", ChildWindowIDsAsString(&parent));

  parent.StackChildBelow(&child3, &child1);
  EXPECT_EQ("2 3 1", ChildWindowIDsAsString(&parent));
}

// Various assertions for StackChildAbove.
TEST_F(WindowTest, StackChildAbove) {
  Window parent(NULL);
  parent.Init(ui::LAYER_NOT_DRAWN);
  Window child1(NULL);
  child1.Init(ui::LAYER_NOT_DRAWN);
  Window child2(NULL);
  child2.Init(ui::LAYER_NOT_DRAWN);
  Window child3(NULL);
  child3.Init(ui::LAYER_NOT_DRAWN);

  parent.AddChild(&child1);
  parent.AddChild(&child2);

  // Move 1 in front of 2.
  parent.StackChildAbove(&child1, &child2);
  ASSERT_EQ(2u, parent.children().size());
  EXPECT_EQ(&child2, parent.children()[0]);
  EXPECT_EQ(&child1, parent.children()[1]);
  ASSERT_EQ(2u, parent.layer()->children().size());
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);

  // Add 3, resulting in order [2, 1, 3], then move 2 in front of 1, resulting
  // in [1, 2, 3].
  parent.AddChild(&child3);
  parent.StackChildAbove(&child2, &child1);
  ASSERT_EQ(3u, parent.children().size());
  EXPECT_EQ(&child1, parent.children()[0]);
  EXPECT_EQ(&child2, parent.children()[1]);
  EXPECT_EQ(&child3, parent.children()[2]);
  ASSERT_EQ(3u, parent.layer()->children().size());
  EXPECT_EQ(child1.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child2.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child3.layer(), parent.layer()->children()[2]);

  // Move 1 in front of 3, resulting in [2, 3, 1].
  parent.StackChildAbove(&child1, &child3);
  ASSERT_EQ(3u, parent.children().size());
  EXPECT_EQ(&child2, parent.children()[0]);
  EXPECT_EQ(&child3, parent.children()[1]);
  EXPECT_EQ(&child1, parent.children()[2]);
  ASSERT_EQ(3u, parent.layer()->children().size());
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child3.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child1.layer(), parent.layer()->children()[2]);

  // Moving 1 in front of 2 should lower it, resulting in [2, 1, 3].
  parent.StackChildAbove(&child1, &child2);
  ASSERT_EQ(3u, parent.children().size());
  EXPECT_EQ(&child2, parent.children()[0]);
  EXPECT_EQ(&child1, parent.children()[1]);
  EXPECT_EQ(&child3, parent.children()[2]);
  ASSERT_EQ(3u, parent.layer()->children().size());
  EXPECT_EQ(child2.layer(), parent.layer()->children()[0]);
  EXPECT_EQ(child1.layer(), parent.layer()->children()[1]);
  EXPECT_EQ(child3.layer(), parent.layer()->children()[2]);
}

// Various capture assertions.
TEST_F(WindowTest, CaptureTests) {
  CaptureWindowDelegateImpl delegate;
  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), root_window()));
  EXPECT_FALSE(window->HasCapture());

  delegate.ResetCounts();

  // Do a capture.
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());
  EXPECT_EQ(0, delegate.capture_lost_count());
  EXPECT_EQ(0, delegate.capture_changed_event_count());
  ui::test::EventGenerator generator(root_window(), gfx::Point(50, 50));
  generator.PressLeftButton();
  EXPECT_EQ(1, delegate.mouse_event_count());
  generator.ReleaseLeftButton();

  EXPECT_EQ(2, delegate.mouse_event_count());
  delegate.ResetCounts();

  ui::TouchEvent touchev(
      ui::ET_TOUCH_PRESSED, gfx::Point(50, 50), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&touchev);
  EXPECT_EQ(1, delegate.touch_event_count());
  delegate.ResetCounts();

  window->ReleaseCapture();
  EXPECT_FALSE(window->HasCapture());
  EXPECT_EQ(1, delegate.capture_lost_count());
  EXPECT_EQ(1, delegate.capture_changed_event_count());
  EXPECT_EQ(1, delegate.mouse_event_count());
  EXPECT_EQ(0, delegate.touch_event_count());

  generator.PressLeftButton();
  EXPECT_EQ(1, delegate.mouse_event_count());

  ui::TouchEvent touchev2(
      ui::ET_TOUCH_PRESSED, gfx::Point(250, 250), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  DispatchEventUsingWindowDispatcher(&touchev2);
  EXPECT_EQ(0, delegate.touch_event_count());

  // Removing the capture window from parent should reset the capture window
  // in the root window.
  window->SetCapture();
  EXPECT_EQ(window.get(), aura::client::GetCaptureWindow(root_window()));
  window->parent()->RemoveChild(window.get());
  EXPECT_FALSE(window->HasCapture());
  EXPECT_EQ(NULL, aura::client::GetCaptureWindow(root_window()));
}

TEST_F(WindowTest, TouchCaptureCancelsOtherTouches) {
  CaptureWindowDelegateImpl delegate1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &delegate1, 0, gfx::Rect(0, 0, 50, 50), root_window()));
  CaptureWindowDelegateImpl delegate2;
  std::unique_ptr<Window> w2(CreateTestWindowWithDelegate(
      &delegate2, 0, gfx::Rect(50, 50, 50, 50), root_window()));

  // Press on w1.
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&press1);
  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN.
  EXPECT_EQ(2, delegate1.gesture_event_count());
  delegate1.ResetCounts();

  // Capturing to w2 should cause the touch to be canceled.
  w2->SetCapture();
  EXPECT_EQ(1, delegate1.touch_event_count());
  EXPECT_EQ(0, delegate2.touch_event_count());
  delegate1.ResetCounts();
  delegate2.ResetCounts();

  // Events are now untargetted.
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(10, 20), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_EQ(0, delegate1.gesture_event_count());
  EXPECT_EQ(0, delegate1.touch_event_count());
  EXPECT_EQ(0, delegate2.gesture_event_count());
  EXPECT_EQ(0, delegate2.touch_event_count());

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(10, 20), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_EQ(0, delegate1.gesture_event_count());
  EXPECT_EQ(0, delegate2.gesture_event_count());

  // A new press is captured by w2.
  ui::TouchEvent press2(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&press2);
  EXPECT_EQ(0, delegate1.gesture_event_count());
  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN.
  EXPECT_EQ(2, delegate2.gesture_event_count());
  delegate1.ResetCounts();
  delegate2.ResetCounts();

  // And releasing capture changes nothing.
  w2->ReleaseCapture();
  EXPECT_EQ(0, delegate1.gesture_event_count());
  EXPECT_EQ(0, delegate1.touch_event_count());
  EXPECT_EQ(0, delegate2.gesture_event_count());
  EXPECT_EQ(0, delegate2.touch_event_count());
}

TEST_F(WindowTest, TouchCaptureDoesntCancelCapturedTouches) {
  CaptureWindowDelegateImpl delegate;
  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 50, 50), root_window()));
  base::TimeTicks time = getTime();
  const int kTimeDelta = 100;

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), time,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&press);

  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN.
  EXPECT_EQ(2, delegate.gesture_event_count());
  EXPECT_EQ(1, delegate.touch_event_count());
  delegate.ResetCounts();

  window->SetCapture();
  EXPECT_EQ(0, delegate.gesture_event_count());
  EXPECT_EQ(0, delegate.touch_event_count());
  delegate.ResetCounts();

  // On move We will get TOUCH_MOVED, GESTURE_TAP_CANCEL,
  // GESTURE_SCROLL_START and GESTURE_SCROLL_UPDATE.
  time += base::TimeDelta::FromMilliseconds(kTimeDelta);
  ui::TouchEvent move(
      ui::ET_TOUCH_MOVED, gfx::Point(10, 20), time,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&move);
  EXPECT_EQ(1, delegate.touch_event_count());
  EXPECT_EQ(3, delegate.gesture_event_count());
  delegate.ResetCounts();

  // Release capture shouldn't change anything.
  window->ReleaseCapture();
  EXPECT_EQ(0, delegate.touch_event_count());
  EXPECT_EQ(0, delegate.gesture_event_count());
  delegate.ResetCounts();

  // On move we still get TOUCH_MOVED and GESTURE_SCROLL_UPDATE.
  time += base::TimeDelta::FromMilliseconds(kTimeDelta);
  ui::TouchEvent move2(
      ui::ET_TOUCH_MOVED, gfx::Point(10, 30), time,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&move2);
  EXPECT_EQ(1, delegate.touch_event_count());
  EXPECT_EQ(1, delegate.gesture_event_count());
  delegate.ResetCounts();

  // And on release we get TOUCH_RELEASED, GESTURE_SCROLL_END, GESTURE_END
  time += base::TimeDelta::FromMilliseconds(kTimeDelta);
  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(10, 20), time,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&release);
  EXPECT_EQ(1, delegate.touch_event_count());
  EXPECT_EQ(2, delegate.gesture_event_count());
}

// Assertions around SetCapture() and touch/gestures.
TEST_F(WindowTest, TransferCaptureTouchEvents) {
  // Touch on |w1|.
  CaptureWindowDelegateImpl d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 0, gfx::Rect(0, 0, 20, 20), root_window()));
  ui::TouchEvent p1(
      ui::ET_TOUCH_PRESSED, gfx::Point(10, 10), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&p1);
  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN.
  EXPECT_EQ(1, d1.touch_event_count());
  EXPECT_EQ(2, d1.gesture_event_count());
  d1.ResetCounts();

  // Touch on |w2| with a different id.
  CaptureWindowDelegateImpl d2;
  std::unique_ptr<Window> w2(CreateTestWindowWithDelegate(
      &d2, 0, gfx::Rect(40, 0, 40, 20), root_window()));
  ui::TouchEvent p2(
      ui::ET_TOUCH_PRESSED, gfx::Point(41, 10), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  DispatchEventUsingWindowDispatcher(&p2);
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  // We will get both GESTURE_BEGIN and GESTURE_TAP_DOWN for new target window.
  EXPECT_EQ(1, d2.touch_event_count());
  EXPECT_EQ(2, d2.gesture_event_count());
  d1.ResetCounts();
  d2.ResetCounts();

  // Set capture on |w2|, this should send a cancel (TAP_CANCEL, END) to |w1|
  // but not |w2|.
  w2->SetCapture();
  EXPECT_EQ(1, d1.touch_event_count());
  EXPECT_EQ(2, d1.gesture_event_count());
  EXPECT_EQ(0, d2.touch_event_count());
  EXPECT_EQ(0, d2.gesture_event_count());
  d1.ResetCounts();
  d2.ResetCounts();

  CaptureWindowDelegateImpl d3;
  std::unique_ptr<Window> w3(CreateTestWindowWithDelegate(
      &d3, 0, gfx::Rect(0, 0, 100, 101), root_window()));
  // Set capture on |w3|. All touches have already been cancelled.
  w3->SetCapture();
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  EXPECT_EQ(1, d2.touch_event_count());
  EXPECT_EQ(2, d2.gesture_event_count());
  EXPECT_EQ(0, d3.touch_event_count());
  EXPECT_EQ(0, d3.gesture_event_count());
  d2.ResetCounts();

  // Move touch id originally associated with |w2|. The touch has been
  // cancelled, so no events should be dispatched.
  ui::TouchEvent m3(
      ui::ET_TOUCH_MOVED, gfx::Point(110, 105), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  DispatchEventUsingWindowDispatcher(&m3);
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  EXPECT_EQ(0, d2.touch_event_count());
  EXPECT_EQ(0, d2.gesture_event_count());
  EXPECT_EQ(0, d3.touch_event_count());
  EXPECT_EQ(0, d3.gesture_event_count());

  // When we release capture, no touches are canceled.
  w3->ReleaseCapture();
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  EXPECT_EQ(0, d2.touch_event_count());
  EXPECT_EQ(0, d2.gesture_event_count());
  EXPECT_EQ(0, d3.touch_event_count());
  EXPECT_EQ(0, d3.gesture_event_count());

  // The touch has been cancelled, so no events are dispatched.
  ui::TouchEvent m4(
      ui::ET_TOUCH_MOVED, gfx::Point(120, 105), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));
  DispatchEventUsingWindowDispatcher(&m4);
  EXPECT_EQ(0, d1.touch_event_count());
  EXPECT_EQ(0, d1.gesture_event_count());
  EXPECT_EQ(0, d2.touch_event_count());
  EXPECT_EQ(0, d2.gesture_event_count());
  EXPECT_EQ(0, d3.touch_event_count());
  EXPECT_EQ(0, d3.gesture_event_count());
}

// Changes capture while capture is already ongoing.
TEST_F(WindowTest, ChangeCaptureWhileMouseDown) {
  CaptureWindowDelegateImpl delegate;
  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), root_window()));
  CaptureWindowDelegateImpl delegate2;
  std::unique_ptr<Window> w2(CreateTestWindowWithDelegate(
      &delegate2, 0, gfx::Rect(20, 20, 20, 20), root_window()));

  // Execute the scheduled draws so that mouse events are not
  // aggregated.
  RunAllPendingInMessageLoop();

  EXPECT_FALSE(window->HasCapture());

  // Do a capture.
  delegate.ResetCounts();
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());
  EXPECT_EQ(0, delegate.capture_lost_count());
  EXPECT_EQ(0, delegate.capture_changed_event_count());
  ui::test::EventGenerator generator(root_window(), gfx::Point(50, 50));
  generator.PressLeftButton();
  EXPECT_EQ(0, delegate.capture_lost_count());
  EXPECT_EQ(0, delegate.capture_changed_event_count());
  EXPECT_EQ(1, delegate.mouse_event_count());

  // Set capture to |w2|, should implicitly unset capture for |window|.
  delegate.ResetCounts();
  delegate2.ResetCounts();
  w2->SetCapture();

  generator.MoveMouseTo(gfx::Point(40, 40), 2);
  EXPECT_EQ(1, delegate.capture_lost_count());
  EXPECT_EQ(1, delegate.capture_changed_event_count());
  EXPECT_EQ(1, delegate.mouse_event_count());
  EXPECT_EQ(2, delegate2.mouse_event_count());
}

// Verifies capture is reset when a window is destroyed.
TEST_F(WindowTest, ReleaseCaptureOnDestroy) {
  CaptureWindowDelegateImpl delegate;
  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 20, 20), root_window()));
  EXPECT_FALSE(window->HasCapture());

  // Do a capture.
  window->SetCapture();
  EXPECT_TRUE(window->HasCapture());

  // Destroy the window.
  window.reset();

  // Make sure the root window doesn't reference the window anymore.
  EXPECT_EQ(NULL, host()->dispatcher()->mouse_pressed_handler());
  EXPECT_EQ(NULL, aura::client::GetCaptureWindow(root_window()));
}

TEST_F(WindowTest, GetBoundsInRootWindow) {
  std::unique_ptr<Window> viewport(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 300, 300), root_window()));
  std::unique_ptr<Window> child(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 100, 100), viewport.get()));
  // Sanity check.
  EXPECT_EQ("0,0 100x100", child->GetBoundsInRootWindow().ToString());

  // The |child| window's screen bounds should move along with the |viewport|.
  viewport->SetBounds(gfx::Rect(-100, -100, 300, 300));
  EXPECT_EQ("-100,-100 100x100", child->GetBoundsInRootWindow().ToString());

  // The |child| window is moved to the 0,0 in screen coordinates.
  // |GetBoundsInRootWindow()| should return 0,0.
  child->SetBounds(gfx::Rect(100, 100, 100, 100));
  EXPECT_EQ("0,0 100x100", child->GetBoundsInRootWindow().ToString());
}

TEST_F(WindowTest, GetBoundsInRootWindowWithLayers) {
  std::unique_ptr<Window> viewport(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 300, 300), root_window()));

  std::unique_ptr<Window> widget(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 200, 200), viewport.get()));

  std::unique_ptr<Window> child(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 100, 100), widget.get()));

  // Sanity check.
  EXPECT_EQ("0,0 100x100", child->GetBoundsInRootWindow().ToString());

  // The |child| window's screen bounds should move along with the |viewport|.
  OffsetBounds(viewport.get(), -100, -100);
  EXPECT_EQ("-100,-100 100x100", child->GetBoundsInRootWindow().ToString());

  OffsetBounds(widget.get(), 50, 50);
  EXPECT_EQ("-50,-50 100x100", child->GetBoundsInRootWindow().ToString());

  // The |child| window is moved to the 0,0 in screen coordinates.
  // |GetBoundsInRootWindow()| should return 0,0.
  OffsetBounds(child.get(), 50, 50);
  EXPECT_EQ("0,0 100x100", child->GetBoundsInRootWindow().ToString());
}

TEST_F(WindowTest, GetBoundsInRootWindowWithLayersAndTranslations) {
  std::unique_ptr<Window> viewport(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 300, 300), root_window()));

  std::unique_ptr<Window> widget(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 200, 200), viewport.get()));

  std::unique_ptr<Window> child(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 100, 100), widget.get()));

  // Sanity check.
  EXPECT_EQ("0,0 100x100", child->GetBoundsInRootWindow().ToString());

  // The |child| window's screen bounds should move along with the |viewport|.
  viewport->SetBounds(gfx::Rect(-100, -100, 300, 300));
  EXPECT_EQ("-100,-100 100x100", child->GetBoundsInRootWindow().ToString());

  widget->SetBounds(gfx::Rect(50, 50, 200, 200));
  EXPECT_EQ("-50,-50 100x100", child->GetBoundsInRootWindow().ToString());

  // The |child| window is moved to the 0,0 in screen coordinates.
  // |GetBoundsInRootWindow()| should return 0,0.
  child->SetBounds(gfx::Rect(50, 50, 100, 100));
  EXPECT_EQ("0,0 100x100", child->GetBoundsInRootWindow().ToString());

  gfx::Transform transform1;
  transform1.Translate(-10, 20);
  viewport->SetTransform(transform1);
  EXPECT_EQ("-10,20 100x100", child->GetBoundsInRootWindow().ToString());

  gfx::Transform transform2;
  transform2.Translate(40, 100);
  widget->SetTransform(transform2);
  EXPECT_EQ("30,120 100x100", child->GetBoundsInRootWindow().ToString());

  // Testing potentially buggy place
  gfx::Transform transform3;
  transform3.Translate(-30, -120);
  child->SetTransform(transform3);
  EXPECT_EQ("0,0 100x100", child->GetBoundsInRootWindow().ToString());
}

// TODO(tdanderson): Remove this class and use
//                   test::EventCountDelegate in its place.
class MouseEnterExitWindowDelegate : public TestWindowDelegate {
 public:
  MouseEnterExitWindowDelegate() : entered_(false), exited_(false) {}

  void OnMouseEvent(ui::MouseEvent* event) override {
    switch (event->type()) {
      case ui::ET_MOUSE_ENTERED:
        EXPECT_TRUE(event->flags() & ui::EF_IS_SYNTHESIZED);
        entered_ = true;
        break;
      case ui::ET_MOUSE_EXITED:
        EXPECT_TRUE(event->flags() & ui::EF_IS_SYNTHESIZED);
        exited_ = true;
        break;
      default:
        break;
    }
  }

  bool entered() const { return entered_; }
  bool exited() const { return exited_; }

  // Clear the entered / exited states.
  void ResetExpectations() {
    entered_ = false;
    exited_ = false;
  }

 private:
  bool entered_;
  bool exited_;

  DISALLOW_COPY_AND_ASSIGN(MouseEnterExitWindowDelegate);
};

// Verifies that the WindowDelegate receives MouseExit and MouseEnter events for
// mouse transitions from window to window.
TEST_F(WindowTest, MouseEnterExit) {
  MouseEnterExitWindowDelegate d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(10, 10, 50, 50), root_window()));
  MouseEnterExitWindowDelegate d2;
  std::unique_ptr<Window> w2(CreateTestWindowWithDelegate(
      &d2, 2, gfx::Rect(70, 70, 50, 50), root_window()));

  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  EXPECT_FALSE(d2.entered());
  EXPECT_FALSE(d2.exited());

  generator.MoveMouseToCenterOf(w2.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_TRUE(d1.exited());
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());
}

// Verifies that the WindowDelegate receives MouseExit from ET_MOUSE_EXITED.
TEST_F(WindowTest, WindowTreeHostExit) {
  MouseEnterExitWindowDelegate d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(10, 10, 50, 50), root_window()));

  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  d1.ResetExpectations();

  ui::MouseEvent exit_event(ui::ET_MOUSE_EXITED, gfx::Point(), gfx::Point(),
                            ui::EventTimeForNow(), 0, 0);
  DispatchEventUsingWindowDispatcher(&exit_event);
  EXPECT_FALSE(d1.entered());
  EXPECT_TRUE(d1.exited());
}

// Verifies that the WindowDelegate receives MouseExit and MouseEnter events for
// mouse transitions from window to window, even if the entered window sets
// and releases capture.
TEST_F(WindowTest, MouseEnterExitWithClick) {
  MouseEnterExitWindowDelegate d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(10, 10, 50, 50), root_window()));
  MouseEnterExitWindowDelegate d2;
  std::unique_ptr<Window> w2(CreateTestWindowWithDelegate(
      &d2, 2, gfx::Rect(70, 70, 50, 50), root_window()));

  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  EXPECT_FALSE(d2.entered());
  EXPECT_FALSE(d2.exited());

  // Emmulate what Views does on a click by grabbing and releasing capture.
  generator.PressLeftButton();
  w1->SetCapture();
  w1->ReleaseCapture();
  generator.ReleaseLeftButton();

  generator.MoveMouseToCenterOf(w2.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_TRUE(d1.exited());
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());
}

TEST_F(WindowTest, MouseEnterExitWhenDeleteWithCapture) {
  MouseEnterExitWindowDelegate delegate;
  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(10, 10, 50, 50), root_window()));

  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(window.get());
  EXPECT_TRUE(delegate.entered());
  EXPECT_FALSE(delegate.exited());

  // Emmulate what Views does on a click by grabbing and releasing capture.
  generator.PressLeftButton();
  window->SetCapture();

  delegate.ResetExpectations();
  generator.MoveMouseTo(0, 0);
  EXPECT_FALSE(delegate.entered());
  EXPECT_FALSE(delegate.exited());

  delegate.ResetExpectations();
  window.reset();
  EXPECT_FALSE(delegate.entered());
  EXPECT_FALSE(delegate.exited());
}

// Verifies that the correct enter / exits are sent if windows appear and are
// deleted under the current mouse position.
TEST_F(WindowTest, MouseEnterExitWithWindowAppearAndDelete) {
  MouseEnterExitWindowDelegate d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(10, 10, 50, 50), root_window()));

  // The cursor is moved into the bounds of |w1|. We expect the delegate
  // of |w1| to see an ET_MOUSE_ENTERED event.
  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  d1.ResetExpectations();

  MouseEnterExitWindowDelegate d2;
  {
    std::unique_ptr<Window> w2(CreateTestWindowWithDelegate(
        &d2, 2, gfx::Rect(10, 10, 50, 50), root_window()));
    // Enters / exits can be sent asynchronously.
    RunAllPendingInMessageLoop();

    // |w2| appears over top of |w1|. We expect the delegate of |w1| to see
    // an ET_MOUSE_EXITED and the delegate of |w2| to see an ET_MOUSE_ENTERED.
    EXPECT_FALSE(d1.entered());
    EXPECT_TRUE(d1.exited());
    EXPECT_TRUE(d2.entered());
    EXPECT_FALSE(d2.exited());
    d1.ResetExpectations();
    d2.ResetExpectations();
  }

  // Enters / exits can be sent asynchronously.
  RunAllPendingInMessageLoop();

  // |w2| has been destroyed, so its delegate should see no further events.
  // The delegate of |w1| should see an ET_MOUSE_ENTERED event.
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());
  EXPECT_FALSE(d2.entered());
  EXPECT_FALSE(d2.exited());
}

// Verifies that enter / exits are sent if windows appear and are hidden
// under the current mouse position..
TEST_F(WindowTest, MouseEnterExitWithHide) {
  MouseEnterExitWindowDelegate d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(10, 10, 50, 50), root_window()));

  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w1.get());
  EXPECT_TRUE(d1.entered());
  EXPECT_FALSE(d1.exited());

  MouseEnterExitWindowDelegate d2;
  std::unique_ptr<Window> w2(CreateTestWindowWithDelegate(
      &d2, 2, gfx::Rect(10, 10, 50, 50), root_window()));
  // Enters / exits can be send asynchronously.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(d1.entered());
  EXPECT_TRUE(d1.exited());
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());

  d1.ResetExpectations();
  w2->Hide();
  // Enters / exits can be send asynchronously.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(d2.exited());
  EXPECT_TRUE(d1.entered());
}

TEST_F(WindowTest, MouseEnterExitWithParentHide) {
  MouseEnterExitWindowDelegate d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(10, 10, 50, 50), root_window()));
  MouseEnterExitWindowDelegate d2;
  Window* w2 = CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(10, 10, 50, 50),
                                            w1.get());
  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w2);
  // Enters / exits can be send asynchronously.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());

  d2.ResetExpectations();
  w1->Hide();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(d2.entered());
  EXPECT_TRUE(d2.exited());

  w1.reset();
}

TEST_F(WindowTest, MouseEnterExitWithParentDelete) {
  MouseEnterExitWindowDelegate d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(10, 10, 50, 50), root_window()));
  MouseEnterExitWindowDelegate d2;
  Window* w2 = CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(10, 10, 50, 50),
                                            w1.get());
  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseToCenterOf(w2);

  // Enters / exits can be send asynchronously.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(d2.entered());
  EXPECT_FALSE(d2.exited());

  d2.ResetExpectations();
  w1.reset();
  RunAllPendingInMessageLoop();

  // Both windows are in the process of destroying, so their delegates should
  // not see any mouse events.
  EXPECT_FALSE(d1.entered());
  EXPECT_FALSE(d1.exited());
  EXPECT_FALSE(d2.entered());
  EXPECT_FALSE(d2.exited());
}

// Creates a window with a delegate (w111) that can handle events at a lower
// z-index than a window without a delegate (w12). w12 is sized to fill the
// entire bounds of the container. This test verifies that
// GetEventHandlerForPoint() skips w12 even though its bounds contain the event,
// because it has no children that can handle the event and it has no delegate
// allowing it to handle the event itself.
TEST_F(WindowTest, GetEventHandlerForPoint_NoDelegate) {
  TestWindowDelegate d111;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      NULL, 1, gfx::Rect(0, 0, 500, 500), root_window()));
  std::unique_ptr<Window> w11(CreateTestWindowWithDelegate(
      NULL, 11, gfx::Rect(0, 0, 500, 500), w1.get()));
  std::unique_ptr<Window> w111(CreateTestWindowWithDelegate(
      &d111, 111, gfx::Rect(50, 50, 450, 450), w11.get()));
  std::unique_ptr<Window> w12(CreateTestWindowWithDelegate(
      NULL, 12, gfx::Rect(0, 0, 500, 500), w1.get()));

  gfx::Point target_point = w111->bounds().CenterPoint();
  EXPECT_EQ(w111.get(), w1->GetEventHandlerForPoint(target_point));
}

class VisibilityWindowDelegate : public TestWindowDelegate {
 public:
  VisibilityWindowDelegate()
      : shown_(0),
        hidden_(0) {
  }

  int shown() const { return shown_; }
  int hidden() const { return hidden_; }
  void Clear() {
    shown_ = 0;
    hidden_ = 0;
  }

  void OnWindowTargetVisibilityChanged(bool visible) override {
    if (visible)
      shown_++;
    else
      hidden_++;
  }

 private:
  int shown_;
  int hidden_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityWindowDelegate);
};

// Verifies show/hide propagate correctly to children and the layer.
TEST_F(WindowTest, Visibility) {
  VisibilityWindowDelegate d;
  VisibilityWindowDelegate d2;
  std::unique_ptr<Window> w1(
      CreateTestWindowWithDelegate(&d, 1, gfx::Rect(), root_window()));
  std::unique_ptr<Window> w2(
      CreateTestWindowWithDelegate(&d2, 2, gfx::Rect(), w1.get()));
  std::unique_ptr<Window> w3(CreateTestWindowWithId(3, w2.get()));

  // Create shows all the windows.
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());
  EXPECT_EQ(1, d.shown());

  d.Clear();
  w1->Hide();
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());
  EXPECT_EQ(1, d.hidden());
  EXPECT_EQ(0, d.shown());

  w2->Show();
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());

  w3->Hide();
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());

  d.Clear();
  w1->Show();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_FALSE(w3->IsVisible());
  EXPECT_EQ(0, d.hidden());
  EXPECT_EQ(1, d.shown());

  w3->Show();
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());

  // Verify that if an ancestor isn't visible and we change the visibility of a
  // child window that OnChildWindowVisibilityChanged() is still invoked.
  w1->Hide();
  d2.Clear();
  w2->Hide();
  EXPECT_EQ(1, d2.hidden());
  EXPECT_EQ(0, d2.shown());
  d2.Clear();
  w2->Show();
  EXPECT_EQ(0, d2.hidden());
  EXPECT_EQ(1, d2.shown());
}

TEST_F(WindowTest, EventTargetingPolicy) {
  TestWindowDelegate d11;
  TestWindowDelegate d12;
  TestWindowDelegate d111;
  TestWindowDelegate d121;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      NULL, 1, gfx::Rect(0, 0, 500, 500), root_window()));
  std::unique_ptr<Window> w11(CreateTestWindowWithDelegate(
      &d11, 11, gfx::Rect(0, 0, 500, 500), w1.get()));
  std::unique_ptr<Window> w111(CreateTestWindowWithDelegate(
      &d111, 111, gfx::Rect(50, 50, 450, 450), w11.get()));
  std::unique_ptr<Window> w12(CreateTestWindowWithDelegate(
      &d12, 12, gfx::Rect(0, 0, 500, 500), w1.get()));
  std::unique_ptr<Window> w121(CreateTestWindowWithDelegate(
      &d121, 121, gfx::Rect(150, 150, 50, 50), w12.get()));

  EXPECT_EQ(w121.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  w12->SetEventTargetingPolicy(EventTargetingPolicy::kTargetOnly);
  EXPECT_EQ(w12.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  EXPECT_TRUE(w12.get()->layer()->accept_events());
  w12->SetEventTargetingPolicy(EventTargetingPolicy::kTargetAndDescendants);
  EXPECT_EQ(w12.get(), w1->GetEventHandlerForPoint(gfx::Point(10, 10)));
  EXPECT_TRUE(w12.get()->layer()->accept_events());
  w12->SetEventTargetingPolicy(EventTargetingPolicy::kNone);
  EXPECT_EQ(w11.get(), w1->GetEventHandlerForPoint(gfx::Point(10, 10)));
  EXPECT_FALSE(w12.get()->layer()->accept_events());

  w12->SetEventTargetingPolicy(EventTargetingPolicy::kTargetAndDescendants);

  EXPECT_EQ(w121.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  w121->SetEventTargetingPolicy(EventTargetingPolicy::kNone);
  EXPECT_EQ(w12.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  EXPECT_FALSE(w121.get()->layer()->accept_events());
  w12->SetEventTargetingPolicy(EventTargetingPolicy::kNone);
  EXPECT_EQ(w111.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  EXPECT_FALSE(w12.get()->layer()->accept_events());
  w111->SetEventTargetingPolicy(EventTargetingPolicy::kNone);
  EXPECT_EQ(w11.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  EXPECT_FALSE(w111.get()->layer()->accept_events());

  w11->SetEventTargetingPolicy(EventTargetingPolicy::kDescendantsOnly);
  EXPECT_EQ(nullptr, w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  EXPECT_TRUE(w11.get()->layer()->accept_events());

  w111->SetEventTargetingPolicy(EventTargetingPolicy::kTargetAndDescendants);
  EXPECT_EQ(w111.get(), w1->GetEventHandlerForPoint(gfx::Point(160, 160)));
  EXPECT_TRUE(w111.get()->layer()->accept_events());
}

TEST_F(WindowTest, ScopedEventTargetingBlockerTest) {
  // Test only when all event targeting blockers are removed from the window,
  // its event targeting policy will restore back to its original value.
  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      nullptr, 1, gfx::Rect(0, 0, 500, 500), root_window()));
  EXPECT_EQ(window->event_targeting_policy(),
            EventTargetingPolicy::kTargetAndDescendants);
  auto event_targeting_blocker1 =
      std::make_unique<ScopedWindowEventTargetingBlocker>(window.get());
  EXPECT_EQ(window->event_targeting_policy(), EventTargetingPolicy::kNone);
  auto event_targeting_blocker2 =
      std::make_unique<ScopedWindowEventTargetingBlocker>(window.get());
  EXPECT_EQ(window->event_targeting_policy(), EventTargetingPolicy::kNone);
  event_targeting_blocker2.reset();
  EXPECT_EQ(window->event_targeting_policy(), EventTargetingPolicy::kNone);
  event_targeting_blocker1.reset();
  EXPECT_EQ(window->event_targeting_policy(),
            EventTargetingPolicy::kTargetAndDescendants);

  // It's possible that the event target policy changes when there is an event
  // targeting blocker in place. In this case when the event targeting blocker
  // is removed from the window, the window should restore to the changed event
  // targeting policy.
  auto event_targeting_blocker3 =
      std::make_unique<ScopedWindowEventTargetingBlocker>(window.get());
  EXPECT_EQ(window->event_targeting_policy(), EventTargetingPolicy::kNone);
  window->SetEventTargetingPolicy(EventTargetingPolicy::kTargetOnly);
  EXPECT_EQ(window->event_targeting_policy(), EventTargetingPolicy::kNone);
  event_targeting_blocker3.reset();
  EXPECT_EQ(window->event_targeting_policy(),
            EventTargetingPolicy::kTargetOnly);
}

// Tests transformation on the root window.
TEST_F(WindowTest, Transform) {
  gfx::Size size = host()->GetBoundsInPixels().size();
  EXPECT_EQ(gfx::Rect(size), display::Screen::GetScreen()
                                 ->GetDisplayNearestPoint(gfx::Point())
                                 .bounds());

  // Rotate it clock-wise 90 degrees.
  host()->SetRootTransform(OverlayTransformToTransform(
      gfx::OVERLAY_TRANSFORM_ROTATE_90, gfx::SizeF(size)));

  // The size should be the transformed size.
  gfx::Size transformed_size(size.height(), size.width());
  EXPECT_EQ(transformed_size.ToString(),
            root_window()->bounds().size().ToString());
  EXPECT_EQ(gfx::Rect(transformed_size).ToString(),
            display::Screen::GetScreen()
                ->GetDisplayNearestPoint(gfx::Point())
                .bounds()
                .ToString());

  // Host size shouldn't change.
  EXPECT_EQ(size.ToString(), host()->GetBoundsInPixels().size().ToString());
}

TEST_F(WindowTest, TransformGesture) {
  gfx::Size size = host()->GetBoundsInPixels().size();

  std::unique_ptr<GestureTrackPositionDelegate> delegate(
      new GestureTrackPositionDelegate);
  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, gfx::Rect(0, 0, 20, 20), root_window()));

  // Rotate the root-window clock-wise 90 degrees.
  host()->SetRootTransform(OverlayTransformToTransform(
      gfx::OVERLAY_TRANSFORM_ROTATE_90, gfx::SizeF(size)));

  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(size.height() - 10, 10), getTime(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_EQ(gfx::Point(10, 10).ToString(), delegate->position().ToString());
}

TEST_F(WindowTest, Property) {
  std::unique_ptr<Window> w(CreateTestWindowWithId(0, root_window()));

  static const char native_prop_key[] = "fnord";

  // Non-existent properties should return the default values.
  EXPECT_EQ(nullptr, w->GetNativeWindowProperty(native_prop_key));

  w->SetNativeWindowProperty(native_prop_key, &*w);
  EXPECT_EQ(&*w, w->GetNativeWindowProperty(native_prop_key));
  w->SetNativeWindowProperty(native_prop_key, nullptr);
  EXPECT_EQ(nullptr, w->GetNativeWindowProperty(native_prop_key));
}

class DeletionTestLayoutManager : public LayoutManager {
 public:
  explicit DeletionTestLayoutManager(DeletionTracker* tracker)
      : tracker_(tracker) {}
  ~DeletionTestLayoutManager() override { tracker_->LayoutManagerDeleted(); }

 private:
  // LayoutManager:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(Window* child) override {}
  void OnWillRemoveWindowFromLayout(Window* child) override {}
  void OnWindowRemovedFromLayout(Window* child) override {}
  void OnChildWindowVisibilityChanged(Window* child, bool visible) override {}
  void SetChildBounds(Window* child,
                      const gfx::Rect& requested_bounds) override {}

  DeletionTracker* tracker_;

  DISALLOW_COPY_AND_ASSIGN(DeletionTestLayoutManager);
};

TEST_F(WindowTest, DeleteLayoutManagerBeforeOwnedProps) {
  DeletionTracker tracker;
  {
    Window w(nullptr);
    w.Init(ui::LAYER_NOT_DRAWN);
    w.SetLayoutManager(new DeletionTestLayoutManager(&tracker));
    w.SetProperty(kDeletionTestPropertyKey, new DeletionTestProperty(&tracker));
  }
  EXPECT_TRUE(tracker.property_deleted());
  EXPECT_TRUE(tracker.layout_manager_deleted());
  EXPECT_EQ(DeletionOrder::LAYOUT_MANAGER_FIRST, tracker.order());
}

TEST_F(WindowTest, SetBoundsInternalShouldCheckTargetBounds) {
  // We cannot short-circuit animations in this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> w1(
      CreateTestWindowWithBounds(gfx::Rect(0, 0, 100, 100), root_window()));

  EXPECT_TRUE(w1->layer());
  w1->layer()->GetAnimator()->set_disable_timer_for_test(true);
  ui::LayerAnimator* animator = w1->layer()->GetAnimator();

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
  EXPECT_EQ("0,0 100x100", w1->layer()->GetTargetBounds().ToString());

  // Animate to a different position.
  {
    ui::ScopedLayerAnimationSettings settings(w1->layer()->GetAnimator());
    w1->SetBounds(gfx::Rect(100, 100, 100, 100));
  }

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
  EXPECT_EQ("100,100 100x100", w1->layer()->GetTargetBounds().ToString());

  // Animate back to the first position. The animation hasn't started yet, so
  // the current bounds are still (0, 0, 100, 100), but the target bounds are
  // (100, 100, 100, 100). If we step the animator ahead, we should find that
  // we're at (0, 0, 100, 100). That is, the second animation should be applied.
  {
    ui::ScopedLayerAnimationSettings settings(w1->layer()->GetAnimator());
    w1->SetBounds(gfx::Rect(0, 0, 100, 100));
  }

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
  EXPECT_EQ("0,0 100x100", w1->layer()->GetTargetBounds().ToString());

  // Confirm that the target bounds are reached.
  base::TimeTicks start_time =
      w1->layer()->GetAnimator()->last_step_time();

  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_EQ("0,0 100x100", w1->bounds().ToString());
}

typedef std::pair<const void*, intptr_t> PropertyChangeInfo;

class WindowObserverTest : public WindowTest,
                           public WindowObserver {
 public:
  struct VisibilityInfo {
    bool window_visible;
    bool visible_param;
    int changed_count;
  };

  struct WindowBoundsInfo {
    int changed_count = 0;
    Window* window = nullptr;
    gfx::Rect old_bounds;
    gfx::Rect new_bounds;
    ui::PropertyChangeReason reason =
        ui::PropertyChangeReason::NOT_FROM_ANIMATION;
  };

  struct WindowOpacityInfo {
    int changed_count = 0;
    Window* window = nullptr;
    ui::PropertyChangeReason reason =
        ui::PropertyChangeReason::NOT_FROM_ANIMATION;
  };

  struct WindowTargetTransformChangingInfo {
    int changed_count = 0;
    Window* window = nullptr;
    gfx::Transform new_transform;
  };

  struct WindowTransformedInfo {
    int changed_count = 0;
    Window* window = nullptr;
    ui::PropertyChangeReason reason =
        ui::PropertyChangeReason::NOT_FROM_ANIMATION;
  };

  struct CountAndWindow {
    int count = 0;
    Window* window = nullptr;
  };

  WindowObserverTest() = default;
  ~WindowObserverTest() override = default;

  const VisibilityInfo* GetVisibilityInfo() const {
    return visibility_info_.get();
  }

  const WindowBoundsInfo& window_bounds_info() const {
    return window_bounds_info_;
  }

  const WindowOpacityInfo& window_opacity_info() const {
    return window_opacity_info_;
  }

  const CountAndWindow& alpha_shape_info() const { return alpha_shape_info_; }

  const WindowTargetTransformChangingInfo&
  window_target_transform_changing_info() const {
    return window_target_transform_changing_info_;
  }

  const WindowTransformedInfo& window_transformed_info() const {
    return window_transformed_info_;
  }

  const CountAndWindow& layer_recreated_info() const {
    return layer_recreated_info_;
  }

  void ResetVisibilityInfo() {
    visibility_info_.reset();
  }

  // Returns a description of the WindowObserver methods that have been invoked.
  std::string WindowObserverCountStateAndClear() {
    std::string result(base::StringPrintf("added=%d removing=%d removed=%d",
                                          added_count_, removing_count_,
                                          removed_count_));
    added_count_ = removing_count_ = removed_count_ = 0;
    return result;
  }

  int DestroyedCountAndClear() {
    int result = destroyed_count_;
    destroyed_count_ = 0;
    return result;
  }

  // Return a tuple of the arguments passed in OnPropertyChanged callback.
  PropertyChangeInfo PropertyChangeInfoAndClear() {
    PropertyChangeInfo result(property_key_, old_property_value_);
    property_key_ = NULL;
    old_property_value_ = -3;
    return result;
  }

 private:
  void OnWindowAdded(Window* new_window) override { added_count_++; }

  void OnWillRemoveWindow(Window* window) override { removing_count_++; }

  void OnWindowRemoved(Window* removed_window) override { removed_count_++; }

  void OnWindowVisibilityChanged(Window* window, bool visible) override {
    if (!visibility_info_) {
      visibility_info_.reset(new VisibilityInfo);
      visibility_info_->changed_count = 0;
    }
    visibility_info_->window_visible = window->IsVisible();
    visibility_info_->visible_param = visible;
    visibility_info_->changed_count++;
  }

  void OnWindowDestroyed(Window* window) override {
    EXPECT_FALSE(window->parent());
    destroyed_count_++;
  }

  void OnWindowPropertyChanged(Window* window,
                               const void* key,
                               intptr_t old) override {
    property_key_ = key;
    old_property_value_ = old;
  }

  void OnWindowBoundsChanged(Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    ++window_bounds_info_.changed_count;
    window_bounds_info_.window = window;
    window_bounds_info_.old_bounds = old_bounds;
    window_bounds_info_.new_bounds = new_bounds;
    window_bounds_info_.reason = reason;
  }

  void OnWindowOpacitySet(Window* window,
                          ui::PropertyChangeReason reason) override {
    ++window_opacity_info_.changed_count;
    window_opacity_info_.window = window;
    window_opacity_info_.reason = reason;
  }

  void OnWindowAlphaShapeSet(Window* window) override {
    ++alpha_shape_info_.count;
    alpha_shape_info_.window = window;
  }

  void OnWindowTargetTransformChanging(
      Window* window,
      const gfx::Transform& new_transform) override {
    ++window_target_transform_changing_info_.changed_count;
    window_target_transform_changing_info_.window = window;
    window_target_transform_changing_info_.new_transform = new_transform;
  }

  void OnWindowTransformed(Window* window,
                           ui::PropertyChangeReason reason) override {
    ++window_transformed_info_.changed_count;
    window_transformed_info_.window = window;
    window_transformed_info_.reason = reason;
  }

  void OnWindowLayerRecreated(Window* window) override {
    ++layer_recreated_info_.count;
    layer_recreated_info_.window = window;
  }

  int added_count_ = 0;
  int removing_count_ = 0;
  int removed_count_ = 0;
  int destroyed_count_ = 0;
  std::unique_ptr<VisibilityInfo> visibility_info_;
  const void* property_key_ = nullptr;
  intptr_t old_property_value_ = -3;
  std::vector<std::pair<int, int> > transform_notifications_;
  WindowBoundsInfo window_bounds_info_;
  WindowOpacityInfo window_opacity_info_;
  WindowTargetTransformChangingInfo window_target_transform_changing_info_;
  WindowTransformedInfo window_transformed_info_;
  CountAndWindow alpha_shape_info_;
  CountAndWindow layer_recreated_info_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserverTest);
};

// Various assertions for WindowObserver.
TEST_F(WindowObserverTest, WindowObserver) {
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  w1->AddObserver(this);

  // Create a new window as a child of w1, our observer should be notified.
  std::unique_ptr<Window> w2(CreateTestWindowWithId(2, w1.get()));
  EXPECT_EQ("added=1 removing=0 removed=0", WindowObserverCountStateAndClear());

  // Delete w2, which should result in the remove notifications.
  w2.reset();
  EXPECT_EQ("added=0 removing=1 removed=1", WindowObserverCountStateAndClear());

  // Create a window that isn't parented to w1, we shouldn't get any
  // notification.
  std::unique_ptr<Window> w3(CreateTestWindowWithId(3, root_window()));
  EXPECT_EQ("added=0 removing=0 removed=0", WindowObserverCountStateAndClear());

  // Similarly destroying w3 shouldn't notify us either.
  w3.reset();
  EXPECT_EQ("added=0 removing=0 removed=0", WindowObserverCountStateAndClear());
  w1->RemoveObserver(this);
}

// Test if OnWindowVisibilityChanged is invoked with expected
// parameters.
TEST_F(WindowObserverTest, WindowVisibility) {
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  std::unique_ptr<Window> w2(CreateTestWindowWithId(1, w1.get()));
  w2->AddObserver(this);

  // Hide should make the window invisible and the passed visible
  // parameter is false.
  w2->Hide();
  EXPECT_TRUE(GetVisibilityInfo());
  EXPECT_TRUE(GetVisibilityInfo());
  if (!GetVisibilityInfo())
    return;
  EXPECT_FALSE(GetVisibilityInfo()->window_visible);
  EXPECT_FALSE(GetVisibilityInfo()->visible_param);
  EXPECT_EQ(1, GetVisibilityInfo()->changed_count);

  // If parent isn't visible, showing window won't make the window visible, but
  // passed visible value must be true.
  w1->Hide();
  ResetVisibilityInfo();
  EXPECT_TRUE(!GetVisibilityInfo());
  w2->Show();
  EXPECT_TRUE(GetVisibilityInfo());
  if (!GetVisibilityInfo())
    return;
  EXPECT_FALSE(GetVisibilityInfo()->window_visible);
  EXPECT_TRUE(GetVisibilityInfo()->visible_param);
  EXPECT_EQ(1, GetVisibilityInfo()->changed_count);

  // If parent is visible, showing window will make the window
  // visible and the passed visible value is true.
  w1->Show();
  w2->Hide();
  ResetVisibilityInfo();
  w2->Show();
  EXPECT_TRUE(GetVisibilityInfo());
  if (!GetVisibilityInfo())
    return;
  EXPECT_TRUE(GetVisibilityInfo()->window_visible);
  EXPECT_TRUE(GetVisibilityInfo()->visible_param);
  EXPECT_EQ(1, GetVisibilityInfo()->changed_count);

  // Verify that the OnWindowVisibilityChanged only once
  // per visibility change.
  w2->Hide();
  EXPECT_EQ(2, GetVisibilityInfo()->changed_count);

  w2->Hide();
  EXPECT_EQ(2, GetVisibilityInfo()->changed_count);
}

// Test if OnWindowDestroyed is invoked as expected.
TEST_F(WindowObserverTest, WindowDestroyed) {
  // Delete a window should fire a destroyed notification.
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  w1->AddObserver(this);
  w1.reset();
  EXPECT_EQ(1, DestroyedCountAndClear());

  // Observe on child and delete parent window should fire a notification.
  std::unique_ptr<Window> parent(CreateTestWindowWithId(1, root_window()));
  Window* child = CreateTestWindowWithId(1, parent.get());  // owned by parent
  child->AddObserver(this);
  parent.reset();
  EXPECT_EQ(1, DestroyedCountAndClear());
}

TEST_F(WindowObserverTest, PropertyChanged) {
  // Setting property should fire a property change notification.
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  w1->AddObserver(this);

  static const WindowProperty<int> prop = {-2};
  static const char native_prop_key[] = "fnord";

  w1->SetProperty(&prop, 1);
  EXPECT_EQ(PropertyChangeInfo(&prop, -2), PropertyChangeInfoAndClear());
  w1->SetProperty(&prop, -2);
  EXPECT_EQ(PropertyChangeInfo(&prop, 1), PropertyChangeInfoAndClear());
  w1->SetProperty(&prop, 3);
  EXPECT_EQ(PropertyChangeInfo(&prop, -2), PropertyChangeInfoAndClear());
  w1->ClearProperty(&prop);
  EXPECT_EQ(PropertyChangeInfo(&prop, 3), PropertyChangeInfoAndClear());

  w1->SetNativeWindowProperty(native_prop_key, &*w1);
  EXPECT_EQ(PropertyChangeInfo(native_prop_key, 0),
            PropertyChangeInfoAndClear());
  w1->SetNativeWindowProperty(native_prop_key, NULL);
  EXPECT_EQ(PropertyChangeInfo(native_prop_key,
                               reinterpret_cast<intptr_t>(&*w1)),
            PropertyChangeInfoAndClear());

  // Sanity check to see if |PropertyChangeInfoAndClear| really clears.
  EXPECT_EQ(PropertyChangeInfo(
      reinterpret_cast<const void*>(NULL), -3), PropertyChangeInfoAndClear());
}

// Verify that WindowObserver::OnWindowBoundsChanged() is notified when the
// bounds of a Window's Layer change without an animation.
TEST_F(WindowObserverTest, WindowBoundsChanged) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  window->AddObserver(this);
  const gfx::Rect initial_bounds = window->bounds();
  constexpr gfx::Rect kTargetBounds(10, 20, 30, 40);
  window->layer()->SetBounds(kTargetBounds);
  ASSERT_EQ(1, window_bounds_info().changed_count);
  EXPECT_EQ(window.get(), window_bounds_info().window);
  EXPECT_EQ(initial_bounds, window_bounds_info().old_bounds);
  EXPECT_EQ(kTargetBounds, window_bounds_info().new_bounds);
  EXPECT_EQ(ui::PropertyChangeReason::NOT_FROM_ANIMATION,
            window_bounds_info().reason);
}

// Verify that WindowObserver::OnWindowBoundsChanged() is notified at every step
// of a bounds animation.
TEST_F(WindowObserverTest, WindowBoundsChangedAnimation) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  window->AddObserver(this);
  const gfx::Rect initial_bounds = window->bounds();
  constexpr gfx::Rect kTargetBounds(10, 20, 30, 40);
  const gfx::Rect step_bounds =
      gfx::Tween::RectValueBetween(0.5, initial_bounds, kTargetBounds);

  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  window->layer()->SetBounds(kTargetBounds);
  ASSERT_EQ(0, window_opacity_info().changed_count);

  window->layer()->GetAnimator()->Step(
      window->layer()->GetAnimator()->last_step_time() +
      settings.GetTransitionDuration() / 2);
  ASSERT_EQ(1, window_bounds_info().changed_count);
  EXPECT_EQ(window.get(), window_bounds_info().window);
  EXPECT_EQ(initial_bounds, window_bounds_info().old_bounds);
  EXPECT_EQ(step_bounds, window_bounds_info().new_bounds);
  EXPECT_EQ(ui::PropertyChangeReason::FROM_ANIMATION,
            window_bounds_info().reason);

  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::BOUNDS);
  ASSERT_EQ(2, window_bounds_info().changed_count);
  EXPECT_EQ(window.get(), window_bounds_info().window);
  EXPECT_EQ(step_bounds, window_bounds_info().old_bounds);
  EXPECT_EQ(kTargetBounds, window_bounds_info().new_bounds);
  EXPECT_EQ(ui::PropertyChangeReason::FROM_ANIMATION,
            window_bounds_info().reason);
}

// Verify that WindowObserver::OnWindowOpacitySet() is notified when the
// opacity of a Window's Layer changes without an animation.
TEST_F(WindowObserverTest, WindowOpacityChanged) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  window->AddObserver(this);
  window->layer()->SetOpacity(0.5f);
  ASSERT_EQ(1, window_opacity_info().changed_count);
  EXPECT_EQ(window.get(), window_opacity_info().window);
  EXPECT_EQ(ui::PropertyChangeReason::NOT_FROM_ANIMATION,
            window_opacity_info().reason);
}

// Verify that WindowObserver::OnWindowOpacitySet() is notified at the
// beginning and at the end of a threaded opacity animation.
TEST_F(WindowObserverTest, WindowOpacityChangedAnimation) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  window->AddObserver(this);

  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  window->layer()->SetOpacity(0.5f);
  ASSERT_EQ(1, window_opacity_info().changed_count);
  EXPECT_EQ(window.get(), window_opacity_info().window);
  EXPECT_EQ(ui::PropertyChangeReason::FROM_ANIMATION,
            window_opacity_info().reason);

  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::OPACITY);
  ASSERT_EQ(2, window_opacity_info().changed_count);
  EXPECT_EQ(window.get(), window_opacity_info().window);
  EXPECT_EQ(ui::PropertyChangeReason::FROM_ANIMATION,
            window_opacity_info().reason);
}

// Verify that WindowObserver::OnWindowAlphaShapeSet() is notified when an alpha
// shape is set for a window.
TEST_F(WindowObserverTest, WindowAlphaShapeChanged) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  window->AddObserver(this);

  auto shape = std::make_unique<ui::Layer::ShapeRects>();
  shape->emplace_back(0, 0, 10, 20);

  EXPECT_EQ(0, alpha_shape_info().count);
  EXPECT_EQ(nullptr, alpha_shape_info().window);
  window->layer()->SetAlphaShape(std::move(shape));
  EXPECT_EQ(1, alpha_shape_info().count);
  EXPECT_EQ(window.get(), alpha_shape_info().window);
}

// Verify that WindowObserver::OnWindow(TargetTransformChanging|Transformed)()
// are notified when SetTransform() is called and there is no animation.
TEST_F(WindowObserverTest, SetTransform) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  window->AddObserver(this);
  gfx::Transform target_transform;
  target_transform.Skew(10.0, 5.0);
  window->SetTransform(target_transform);

  ASSERT_EQ(1, window_target_transform_changing_info().changed_count);
  EXPECT_EQ(window.get(), window_target_transform_changing_info().window);
  EXPECT_EQ(target_transform,
            window_target_transform_changing_info().new_transform);

  ASSERT_EQ(1, window_transformed_info().changed_count);
  EXPECT_EQ(window.get(), window_transformed_info().window);
  EXPECT_EQ(ui::PropertyChangeReason::NOT_FROM_ANIMATION,
            window_transformed_info().reason);
}

// Verify that WindowObserver::OnWindowTransformed)() is notified at the
// beginning and at the end of a threaded transform animation. Verify that
// WindowObserver::OnWindowTargetTransformChanging() is notified when the
// threaded animation is started by SetTransform().
TEST_F(WindowObserverTest, SetTransformAnimation) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  window->AddObserver(this);

  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  gfx::Transform target_transform;
  target_transform.Skew(10.0, 5.0);
  window->SetTransform(target_transform);

  ASSERT_EQ(1, window_target_transform_changing_info().changed_count);
  EXPECT_EQ(window.get(), window_target_transform_changing_info().window);
  EXPECT_EQ(target_transform,
            window_target_transform_changing_info().new_transform);

  ASSERT_EQ(1, window_transformed_info().changed_count);
  EXPECT_EQ(window.get(), window_transformed_info().window);
  EXPECT_EQ(ui::PropertyChangeReason::FROM_ANIMATION,
            window_transformed_info().reason);

  window->layer()->GetAnimator()->StopAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM);

  EXPECT_EQ(1, window_target_transform_changing_info().changed_count);

  ASSERT_EQ(2, window_transformed_info().changed_count);
  EXPECT_EQ(window.get(), window_transformed_info().window);
  EXPECT_EQ(ui::PropertyChangeReason::FROM_ANIMATION,
            window_transformed_info().reason);
}

TEST_F(WindowObserverTest, OnWindowLayerRecreated) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  window->AddObserver(this);

  EXPECT_EQ(0, layer_recreated_info().count);
  std::unique_ptr<ui::Layer> old_layer = window->RecreateLayer();
  EXPECT_EQ(1, layer_recreated_info().count);
  EXPECT_EQ(window.get(), layer_recreated_info().window);
}

TEST_F(WindowObserverTest, OnWindowLayerRecreatedWithOpacityAnimation) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));

  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  window->layer()->SetOpacity(0.5);
  EXPECT_TRUE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::OPACITY));

  window->AddObserver(this);

  EXPECT_EQ(0, layer_recreated_info().count);
  EXPECT_EQ(0, window_opacity_info().changed_count);
  std::unique_ptr<ui::Layer> old_layer = window->RecreateLayer();
  EXPECT_EQ(1, layer_recreated_info().count);
  EXPECT_EQ(window.get(), layer_recreated_info().window);
  EXPECT_EQ(1, window_opacity_info().changed_count);
  EXPECT_EQ(window.get(), window_opacity_info().window);
  EXPECT_EQ(ui::PropertyChangeReason::FROM_ANIMATION,
            window_opacity_info().reason);
}

TEST_F(WindowObserverTest, OnWindowLayerRecreatedWithTransformAnimation) {
  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));

  ui::ScopedAnimationDurationScaleMode scoped_animation_duration_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  gfx::Transform target_transform;
  target_transform.Skew(10.0, 5.0);
  window->SetTransform(target_transform);
  EXPECT_TRUE(window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));

  window->AddObserver(this);

  EXPECT_EQ(0, layer_recreated_info().count);
  EXPECT_EQ(0, window_transformed_info().changed_count);
  std::unique_ptr<ui::Layer> old_layer = window->RecreateLayer();
  EXPECT_EQ(1, layer_recreated_info().count);
  EXPECT_EQ(window.get(), layer_recreated_info().window);
  EXPECT_EQ(1, window_transformed_info().changed_count);
  EXPECT_EQ(window.get(), window_transformed_info().window);
  EXPECT_EQ(ui::PropertyChangeReason::FROM_ANIMATION,
            window_transformed_info().reason);
}

TEST_F(WindowTest, AcquireLayer) {
  std::unique_ptr<Window> window1(CreateTestWindowWithId(1, root_window()));
  std::unique_ptr<Window> window2(CreateTestWindowWithId(2, root_window()));
  ui::Layer* parent = window1->parent()->layer();
  EXPECT_EQ(2U, parent->children().size());

  WindowTestApi window1_test_api(window1.get());
  WindowTestApi window2_test_api(window2.get());

  EXPECT_TRUE(window1_test_api.OwnsLayer());
  EXPECT_TRUE(window2_test_api.OwnsLayer());

  // After acquisition, window1 should not own its layer, but it should still
  // be available to the window.
  std::unique_ptr<ui::Layer> window1_layer(window1->AcquireLayer());
  EXPECT_FALSE(window1_test_api.OwnsLayer());
  EXPECT_TRUE(window1_layer.get() == window1->layer());

  // The acquired layer's owner should be set NULL and re-acquring
  // should return NULL.
  EXPECT_FALSE(window1_layer->owner());
  std::unique_ptr<ui::Layer> window1_layer_reacquired(window1->AcquireLayer());
  EXPECT_FALSE(window1_layer_reacquired.get());

  // Upon destruction, window1's layer should still be valid, and in the layer
  // hierarchy, but window2's should be gone, and no longer in the hierarchy.
  window1.reset();
  window2.reset();

  // This should be set by the window's destructor.
  EXPECT_TRUE(window1_layer->delegate() == NULL);
  EXPECT_EQ(1U, parent->children().size());
}

// Make sure that properties which should persist from the old layer to the new
// layer actually do.
TEST_F(WindowTest, RecreateLayer) {
  // Set properties to non default values.
  gfx::Rect window_bounds(100, 100);
  Window w(new ColorTestWindowDelegate(SK_ColorWHITE));
  w.set_id(1);
  w.Init(ui::LAYER_SOLID_COLOR);
  w.SetBounds(window_bounds);

  ui::Layer* layer = w.layer();
  layer->SetVisible(false);
  layer->SetMasksToBounds(true);

  ui::Layer child_layer;
  layer->Add(&child_layer);

  std::unique_ptr<ui::Layer> old_layer(w.RecreateLayer());
  layer = w.layer();
  EXPECT_EQ(ui::LAYER_SOLID_COLOR, layer->type());
  EXPECT_FALSE(layer->visible());
  EXPECT_EQ(1u, layer->children().size());
  EXPECT_TRUE(layer->GetMasksToBounds());
  EXPECT_EQ("0,0 100x100", w.bounds().ToString());
  EXPECT_EQ("0,0 100x100", layer->bounds().ToString());
}

// Verify that RecreateLayer() stacks the old layer above the newly creatd
// layer.
TEST_F(WindowTest, RecreateLayerZOrder) {
  std::unique_ptr<Window> w(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(0, 0, 100, 100), root_window()));
  std::unique_ptr<ui::Layer> old_layer(w->RecreateLayer());

  const std::vector<ui::Layer*>& child_layers =
      root_window()->layer()->children();
  ASSERT_EQ(2u, child_layers.size());
  EXPECT_EQ(w->layer(), child_layers[0]);
  EXPECT_EQ(old_layer.get(), child_layers[1]);
}

// Ensure that acquiring a layer then recreating a layer does not crash
// and that RecreateLayer returns null.
TEST_F(WindowTest, AcquireThenRecreateLayer) {
  std::unique_ptr<Window> w(CreateTestWindow(
      SK_ColorWHITE, 1, gfx::Rect(0, 0, 100, 100), root_window()));
  std::unique_ptr<ui::Layer> acquired_layer(w->AcquireLayer());
  std::unique_ptr<ui::Layer> doubly_acquired_layer(w->RecreateLayer());
  EXPECT_EQ(NULL, doubly_acquired_layer.get());

  // Destroy window before layer gets destroyed.
  w.reset();
}

class TestVisibilityClient : public client::VisibilityClient {
 public:
  explicit TestVisibilityClient(Window* root_window)
      : ignore_visibility_changes_(false) {
    client::SetVisibilityClient(root_window, this);
  }
  ~TestVisibilityClient() override {}

  void set_ignore_visibility_changes(bool ignore_visibility_changes) {
    ignore_visibility_changes_ = ignore_visibility_changes;
  }

  // Overridden from client::VisibilityClient:
  void UpdateLayerVisibility(aura::Window* window, bool visible) override {
    if (!ignore_visibility_changes_)
      window->layer()->SetVisible(visible);
  }

 private:
  bool ignore_visibility_changes_;
  DISALLOW_COPY_AND_ASSIGN(TestVisibilityClient);
};

TEST_F(WindowTest, VisibilityClientIsVisible) {
  TestVisibilityClient client(root_window());

  std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->layer()->visible());

  window->Hide();
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->layer()->visible());
  window->Show();

  client.set_ignore_visibility_changes(true);
  window->Hide();
  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(window->layer()->visible());
}

// Tests the mouse events seen by WindowDelegates in a Window hierarchy when
// changing the properties of a leaf Window.
TEST_F(WindowTest, MouseEventsOnLeafWindowChange) {
  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseTo(50, 50);

  EventCountDelegate d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(0, 0, 100, 100), root_window()));
  RunAllPendingInMessageLoop();
  // The format of result is "Enter/Move/Leave".
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());

  // Add new window |w11| on top of |w1| which contains the cursor.
  EventCountDelegate d11;
  std::unique_ptr<Window> w11(CreateTestWindowWithDelegate(
      &d11, 1, gfx::Rect(0, 0, 100, 100), w1.get()));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Resize |w11| so that it does not contain the cursor.
  w11->SetBounds(gfx::Rect(0, 0, 10, 10));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 1", d11.GetMouseMotionCountsAndReset());

  // Resize |w11| so that it does contain the cursor.
  w11->SetBounds(gfx::Rect(0, 0, 60, 60));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Detach |w11| from |w1|.
  w1->RemoveChild(w11.get());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 1", d11.GetMouseMotionCountsAndReset());

  // Re-attach |w11| to |w1|.
  w1->AddChild(w11.get());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Hide |w11|.
  w11->Hide();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 1", d11.GetMouseMotionCountsAndReset());

  // Show |w11|.
  w11->Show();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Translate |w11| so that it does not contain the mouse cursor.
  gfx::Transform transform;
  transform.Translate(100, 100);
  w11->SetTransform(transform);
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 1", d11.GetMouseMotionCountsAndReset());

  // Clear the transform on |w11| so that it does contain the cursor.
  w11->SetTransform(gfx::Transform());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d11.GetMouseMotionCountsAndReset());

  // Close |w11|. Note that since |w11| is being destroyed, its delegate should
  // not see any further events.
  w11.reset();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", d11.GetMouseMotionCountsAndReset());

  // Move the mouse outside the bounds of the root window. Since the mouse
  // cursor is no longer within their bounds, the delegates of the child
  // windows should not see any mouse events.
  generator.MoveMouseTo(-10, -10);
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", d11.GetMouseMotionCountsAndReset());

  // Add |w11|.
  w11.reset(CreateTestWindowWithDelegate(
      &d11, 1, gfx::Rect(0, 0, 100, 100), w1.get()));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", d11.GetMouseMotionCountsAndReset());

  // Close |w11|.
  w11.reset();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", d11.GetMouseMotionCountsAndReset());
}

// Tests the mouse events seen by WindowDelegates in a Window hierarchy when
// deleting a non-leaf Window.
TEST_F(WindowTest, MouseEventsOnNonLeafWindowDelete) {
  ui::test::EventGenerator generator(root_window());
  generator.MoveMouseTo(50, 50);

  EventCountDelegate d1;
  std::unique_ptr<Window> w1(CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(0, 0, 100, 100), root_window()));
  RunAllPendingInMessageLoop();
  // The format of result is "Enter/Move/Leave".
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());

  // Add new window |w2| on top of |w1| which contains the cursor.
  EventCountDelegate d2;
  std::unique_ptr<Window> w2(CreateTestWindowWithDelegate(
      &d2, 1, gfx::Rect(0, 0, 100, 100), w1.get()));
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 1", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d2.GetMouseMotionCountsAndReset());

  // Add new window on top of |w2| which contains the cursor.
  EventCountDelegate d3;
  CreateTestWindowWithDelegate(
      &d3, 1, gfx::Rect(0, 0, 100, 100), w2.get());
  RunAllPendingInMessageLoop();
  EXPECT_EQ("0 0 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 1", d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("1 1 0", d3.GetMouseMotionCountsAndReset());

  // Delete |w2|, which will also delete its owned child window. Since |w2| and
  // its child are in the process of being destroyed, their delegates should
  // not see any further events.
  w2.reset();
  RunAllPendingInMessageLoop();
  EXPECT_EQ("1 1 0", d1.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", d2.GetMouseMotionCountsAndReset());
  EXPECT_EQ("0 0 0", d3.GetMouseMotionCountsAndReset());
}

class RootWindowAttachmentObserver : public WindowObserver {
 public:
  RootWindowAttachmentObserver() : added_count_(0), removed_count_(0) {}
  ~RootWindowAttachmentObserver() override {}

  int added_count() const { return added_count_; }
  int removed_count() const { return removed_count_; }

  void Clear() {
    added_count_ = 0;
    removed_count_ = 0;
  }

  // Overridden from WindowObserver:
  void OnWindowAddedToRootWindow(Window* window) override { ++added_count_; }
  void OnWindowRemovingFromRootWindow(Window* window,
                                      Window* new_root) override {
    ++removed_count_;
  }

 private:
  int added_count_;
  int removed_count_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowAttachmentObserver);
};

TEST_F(WindowTest, RootWindowAttachment) {
  RootWindowAttachmentObserver observer;

  // Test a direct add/remove from the RootWindow.
  std::unique_ptr<Window> w1(new Window(NULL));
  w1->Init(ui::LAYER_NOT_DRAWN);
  w1->AddObserver(&observer);

  ParentWindow(w1.get());
  EXPECT_EQ(1, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  w1.reset();
  EXPECT_EQ(1, observer.added_count());
  EXPECT_EQ(1, observer.removed_count());

  observer.Clear();

  // Test an indirect add/remove from the RootWindow.
  w1.reset(new Window(NULL));
  w1->Init(ui::LAYER_NOT_DRAWN);
  Window* w11 = new Window(NULL);
  w11->Init(ui::LAYER_NOT_DRAWN);
  w11->AddObserver(&observer);
  w1->AddChild(w11);
  EXPECT_EQ(0, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  ParentWindow(w1.get());
  EXPECT_EQ(1, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  w1.reset();  // Deletes w11.
  w11 = NULL;
  EXPECT_EQ(1, observer.added_count());
  EXPECT_EQ(1, observer.removed_count());

  observer.Clear();

  // Test an indirect add/remove with nested observers.
  w1.reset(new Window(NULL));
  w1->Init(ui::LAYER_NOT_DRAWN);
  w11 = new Window(NULL);
  w11->Init(ui::LAYER_NOT_DRAWN);
  w11->AddObserver(&observer);
  w1->AddChild(w11);
  Window* w111 = new Window(NULL);
  w111->Init(ui::LAYER_NOT_DRAWN);
  w111->AddObserver(&observer);
  w11->AddChild(w111);

  EXPECT_EQ(0, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  ParentWindow(w1.get());
  EXPECT_EQ(2, observer.added_count());
  EXPECT_EQ(0, observer.removed_count());

  w1.reset();  // Deletes w11 and w111.
  w11 = NULL;
  w111 = NULL;
  EXPECT_EQ(2, observer.added_count());
  EXPECT_EQ(2, observer.removed_count());
}

class BoundsChangedWindowObserver : public WindowObserver {
 public:
  BoundsChangedWindowObserver() : root_set_(false) {}

  void OnWindowBoundsChanged(Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    root_set_ = window->GetRootWindow() != NULL;
  }

  bool root_set() const { return root_set_; }

 private:
  bool root_set_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangedWindowObserver);
};

TEST_F(WindowTest, RootWindowSetWhenReparenting) {
  Window parent1(NULL);
  parent1.Init(ui::LAYER_NOT_DRAWN);
  Window parent2(NULL);
  parent2.Init(ui::LAYER_NOT_DRAWN);
  ParentWindow(&parent1);
  ParentWindow(&parent2);
  parent1.SetBounds(gfx::Rect(10, 10, 300, 300));
  parent2.SetBounds(gfx::Rect(20, 20, 300, 300));

  BoundsChangedWindowObserver observer;
  Window child(NULL);
  child.Init(ui::LAYER_NOT_DRAWN);
  child.SetBounds(gfx::Rect(5, 5, 100, 100));
  parent1.AddChild(&child);

  // We need animations to start in order to observe the bounds changes.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ui::ScopedLayerAnimationSettings settings1(child.layer()->GetAnimator());
  settings1.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
  gfx::Rect new_bounds(gfx::Rect(35, 35, 50, 50));
  child.SetBounds(new_bounds);

  child.AddObserver(&observer);

  // Reparenting the |child| will cause it to get moved. During this move
  // the window should still have root window set.
  parent2.AddChild(&child);
  EXPECT_TRUE(observer.root_set());

  // Animations should stop and the bounds should be as set before the |child|
  // got reparented.
  EXPECT_EQ(new_bounds.ToString(), child.GetTargetBounds().ToString());
  EXPECT_EQ(new_bounds.ToString(), child.bounds().ToString());
  EXPECT_EQ("55,55 50x50", child.GetBoundsInRootWindow().ToString());
}

TEST_F(WindowTest, OwnedByParentFalse) {
  // By default, a window is owned by its parent. If this is set to false, the
  // window will not be destroyed when its parent is.

  std::unique_ptr<Window> w1(new Window(NULL));
  w1->Init(ui::LAYER_NOT_DRAWN);
  std::unique_ptr<Window> w2(new Window(NULL));
  w2->set_owned_by_parent(false);
  w2->Init(ui::LAYER_NOT_DRAWN);
  w1->AddChild(w2.get());

  w1.reset();

  // We should be able to deref w2 still, but its parent should now be NULL.
  EXPECT_EQ(NULL, w2->parent());
}

// Used By DeleteWindowFromOnWindowDestroyed. Destroys a Window from
// OnWindowDestroyed().
class OwningWindowDelegate : public TestWindowDelegate {
 public:
  OwningWindowDelegate() {}

  void SetOwnedWindow(Window* window) {
    owned_window_.reset(window);
  }

  void OnWindowDestroyed(Window* window) override { owned_window_.reset(NULL); }

 private:
  std::unique_ptr<Window> owned_window_;

  DISALLOW_COPY_AND_ASSIGN(OwningWindowDelegate);
};

// Creates a window with two child windows. When the first child window is
// destroyed (WindowDelegate::OnWindowDestroyed) it deletes the second child.
// This synthesizes BrowserView and the status bubble. Both are children of the
// same parent and destroying BrowserView triggers it destroying the status
// bubble.
TEST_F(WindowTest, DeleteWindowFromOnWindowDestroyed) {
  std::unique_ptr<Window> parent(new Window(NULL));
  parent->Init(ui::LAYER_NOT_DRAWN);
  OwningWindowDelegate delegate;
  Window* c1 = new Window(&delegate);
  c1->Init(ui::LAYER_NOT_DRAWN);
  parent->AddChild(c1);
  Window* c2 = new Window(NULL);
  c2->Init(ui::LAYER_NOT_DRAWN);
  parent->AddChild(c2);
  delegate.SetOwnedWindow(c2);
  parent.reset();
}

// WindowObserver implementation that deletes a window in
// OnWindowVisibilityChanged().
class DeleteOnVisibilityChangedObserver : public WindowObserver {
 public:
  // |to_observe| is the Window this is added as an observer to. When
  // OnWindowVisibilityChanged() is called |to_delete| is deleted.
  explicit DeleteOnVisibilityChangedObserver(Window* to_observe,
                                             Window* to_delete)
      : to_observe_(to_observe), to_delete_(to_delete) {
    to_observe_->AddObserver(this);
  }
  ~DeleteOnVisibilityChangedObserver() override {
    // OnWindowVisibilityChanged() should have been called.
    DCHECK(!to_delete_);
  }

  // WindowObserver:
  void OnWindowVisibilityChanged(Window* window, bool visible) override {
    to_observe_->RemoveObserver(this);
    delete to_delete_;
    to_delete_ = nullptr;
  }

 private:
  Window* to_observe_;
  Window* to_delete_;

  DISALLOW_COPY_AND_ASSIGN(DeleteOnVisibilityChangedObserver);
};

TEST_F(WindowTest, DeleteParentWindowFromOnWindowVisibiltyChanged) {
  WindowTracker tracker;
  Window* root = CreateTestWindowWithId(0, nullptr);
  tracker.Add(root);
  Window* child1 = CreateTestWindowWithId(0, root);
  tracker.Add(child1);
  tracker.Add(CreateTestWindowWithId(0, root));

  // This deletes |root| (the parent) when OnWindowVisibilityChanged() is
  // received by |child1|.
  DeleteOnVisibilityChangedObserver deletion_observer(child1, root);
  // The Hide() calls trigger deleting |root|, which should delete the whole
  // tree.
  root->Hide();
  EXPECT_TRUE(tracker.windows().empty());
}

// Used by DelegateNotifiedAsBoundsChange to verify OnBoundsChanged() is
// invoked.
class BoundsChangeDelegate : public TestWindowDelegate {
 public:
  BoundsChangeDelegate() : bounds_changed_(false) {}

  void clear_bounds_changed() { bounds_changed_ = false; }
  bool bounds_changed() const {
    return bounds_changed_;
  }

  // Window
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {
    bounds_changed_ = true;
  }

 private:
  // Was OnBoundsChanged() invoked?
  bool bounds_changed_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangeDelegate);
};

// Verifies the delegate is notified when the actual bounds of the layer
// change.
TEST_F(WindowTest, DelegateNotifiedAsBoundsChange) {
  BoundsChangeDelegate delegate;

  // We cannot short-circuit animations in this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));
  window->layer()->GetAnimator()->set_disable_timer_for_test(true);

  delegate.clear_bounds_changed();

  // Animate to a different position.
  {
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    window->SetBounds(gfx::Rect(100, 100, 100, 100));
  }

  // Bounds shouldn't immediately have changed.
  EXPECT_EQ("0,0 100x100", window->bounds().ToString());
  EXPECT_FALSE(delegate.bounds_changed());

  // Animate to the end, which should notify of the change.
  base::TimeTicks start_time =
      window->layer()->GetAnimator()->last_step_time();
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(delegate.bounds_changed());
  EXPECT_NE("0,0 100x100", window->bounds().ToString());
}

// Verifies the delegate is notified when the actual bounds of the layer
// change even when the window is not the layer's delegate
TEST_F(WindowTest, DelegateNotifiedAsBoundsChangeInHiddenLayer) {
  BoundsChangeDelegate delegate;

  // We cannot short-circuit animations in this test.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<Window> window(CreateTestWindowWithDelegate(
      &delegate, 1, gfx::Rect(0, 0, 100, 100), root_window()));
  window->layer()->GetAnimator()->set_disable_timer_for_test(true);

  delegate.clear_bounds_changed();

  // Suppress paint on the layer since it is hidden (should reset the layer's
  // delegate to NULL)
  window->layer()->SuppressPaint();
  EXPECT_EQ(NULL, window->layer()->delegate());

  // Animate to a different position.
  {
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    window->SetBounds(gfx::Rect(100, 100, 110, 100));
  }

  // Layer delegate is NULL but we should still get bounds changed notification.
  EXPECT_EQ("100,100 110x100", window->GetTargetBounds().ToString());
  EXPECT_TRUE(delegate.bounds_changed());

  delegate.clear_bounds_changed();

  // Animate to the end: will *not* notify of the change since we are hidden.
  base::TimeTicks start_time =
      window->layer()->GetAnimator()->last_step_time();
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  animator->Step(start_time + base::TimeDelta::FromMilliseconds(1000));

  // No bounds changed notification at the end of animation since layer
  // delegate is NULL.
  EXPECT_FALSE(delegate.bounds_changed());
  EXPECT_NE("0,0 100x100", window->layer()->bounds().ToString());
}

// Used by AddChildNotifications to track notification counts.
class AddChildNotificationsObserver : public WindowObserver {
 public:
  AddChildNotificationsObserver() : added_count_(0), removed_count_(0) {}

  std::string CountStringAndReset() {
    std::string result = base::NumberToString(added_count_) + " " +
                         base::NumberToString(removed_count_);
    added_count_ = removed_count_ = 0;
    return result;
  }

  // WindowObserver overrides:
  void OnWindowAddedToRootWindow(Window* window) override { added_count_++; }
  void OnWindowRemovingFromRootWindow(Window* window,
                                      Window* new_root) override {
    removed_count_++;
  }

 private:
  int added_count_;
  int removed_count_;

  DISALLOW_COPY_AND_ASSIGN(AddChildNotificationsObserver);
};

// Assertions around when root window notifications are sent.
TEST_F(WindowTest, AddChildNotifications) {
  AddChildNotificationsObserver observer;
  std::unique_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
  std::unique_ptr<Window> w2(CreateTestWindowWithId(1, root_window()));
  w2->AddObserver(&observer);
  w2->Focus();
  EXPECT_TRUE(w2->HasFocus());

  // Move |w2| to be a child of |w1|.
  w1->AddChild(w2.get());
  // Sine we moved in the same root, observer shouldn't be notified.
  EXPECT_EQ("0 0", observer.CountStringAndReset());
  // |w2| should still have focus after moving.
  EXPECT_TRUE(w2->HasFocus());
}

// Tests that a delegate that destroys itself when the window is destroyed does
// not break.
TEST_F(WindowTest, DelegateDestroysSelfOnWindowDestroy) {
  std::unique_ptr<Window> w1(
      CreateTestWindowWithDelegate(new DestroyWindowDelegate(), 0,
                                   gfx::Rect(10, 20, 30, 40), root_window()));
}

class HierarchyObserver : public WindowObserver {
 public:
  explicit HierarchyObserver(Window* target) : target_(target) {
    target_->AddObserver(this);
  }
  ~HierarchyObserver() override { target_->RemoveObserver(this); }

  void ValidateState(
      int index,
      const WindowObserver::HierarchyChangeParams& params) const {
    ParamsMatch(params_[index], params);
  }

  void Reset() {
    params_.clear();
  }

 private:
  // Overridden from WindowObserver:
  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override {
    params_.push_back(params);
  }
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override {
    params_.push_back(params);
  }

  void ParamsMatch(const WindowObserver::HierarchyChangeParams& p1,
                   const WindowObserver::HierarchyChangeParams& p2) const {
    EXPECT_EQ(p1.phase, p2.phase);
    EXPECT_EQ(p1.target, p2.target);
    EXPECT_EQ(p1.new_parent, p2.new_parent);
    EXPECT_EQ(p1.old_parent, p2.old_parent);
    EXPECT_EQ(p1.receiver, p2.receiver);
  }

  Window* target_;
  std::vector<WindowObserver::HierarchyChangeParams> params_;

  DISALLOW_COPY_AND_ASSIGN(HierarchyObserver);
};

// Tests hierarchy change notifications.
TEST_F(WindowTest, OnWindowHierarchyChange) {
  {
    // Simple add & remove.
    HierarchyObserver oroot(root_window());

    std::unique_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
    HierarchyObserver o1(w1.get());

    // Add.
    root_window()->AddChild(w1.get());

    WindowObserver::HierarchyChangeParams params;
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.target = w1.get();
    params.old_parent = NULL;
    params.new_parent = root_window();
    params.receiver = w1.get();
    o1.ValidateState(0, params);

    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
    params.receiver = w1.get();
    o1.ValidateState(1, params);

    params.receiver = root_window();
    oroot.ValidateState(0, params);

    // Remove.
    o1.Reset();
    oroot.Reset();

    root_window()->RemoveChild(w1.get());

    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.old_parent = root_window();
    params.new_parent = NULL;
    params.receiver = w1.get();

    o1.ValidateState(0, params);

    params.receiver = root_window();
    oroot.ValidateState(0, params);

    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
    params.receiver = w1.get();
    o1.ValidateState(1, params);
  }

  {
    // Add & remove of hierarchy. Tests notification order per documentation in
    // WindowObserver.
    HierarchyObserver o(root_window());
    std::unique_ptr<Window> w1(CreateTestWindowWithId(1, NULL));
    Window* w11 = CreateTestWindowWithId(11, w1.get());
    w1->AddObserver(&o);
    w11->AddObserver(&o);

    // Add.
    root_window()->AddChild(w1.get());

    // Dispatched to target first.
    int index = 0;
    WindowObserver::HierarchyChangeParams params;
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.target = w1.get();
    params.old_parent = NULL;
    params.new_parent = root_window();
    params.receiver = w1.get();
    o.ValidateState(index++, params);

    // Dispatched to target's children.
    params.receiver = w11;
    o.ValidateState(index++, params);

    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;

    // Now process the "changed" phase.
    params.receiver = w1.get();
    o.ValidateState(index++, params);
    params.receiver = w11;
    o.ValidateState(index++, params);
    params.receiver = root_window();
    o.ValidateState(index++, params);

    // Remove.
    root_window()->RemoveChild(w1.get());
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.old_parent = root_window();
    params.new_parent = NULL;
    params.receiver = w1.get();
    o.ValidateState(index++, params);
    params.receiver = w11;
    o.ValidateState(index++, params);
    params.receiver = root_window();
    o.ValidateState(index++, params);
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
    params.receiver = w1.get();
    o.ValidateState(index++, params);
    params.receiver = w11;
    o.ValidateState(index++, params);

    w1.reset();
  }

  {
    // Reparent. Tests notification order per documentation in WindowObserver.
    std::unique_ptr<Window> w1(CreateTestWindowWithId(1, root_window()));
    Window* w11 = CreateTestWindowWithId(11, w1.get());
    Window* w111 = CreateTestWindowWithId(111, w11);
    std::unique_ptr<Window> w2(CreateTestWindowWithId(2, root_window()));

    HierarchyObserver o(root_window());
    w1->AddObserver(&o);
    w11->AddObserver(&o);
    w111->AddObserver(&o);
    w2->AddObserver(&o);

    w2->AddChild(w11);

    // Dispatched to target first.
    int index = 0;
    WindowObserver::HierarchyChangeParams params;
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
    params.target = w11;
    params.old_parent = w1.get();
    params.new_parent = w2.get();
    params.receiver = w11;
    o.ValidateState(index++, params);

    // Then to target's children.
    params.receiver = w111;
    o.ValidateState(index++, params);

    // Then to target's old parent chain.
    params.receiver = w1.get();
    o.ValidateState(index++, params);
    params.receiver = root_window();
    o.ValidateState(index++, params);

    // "Changed" phase.
    params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
    params.receiver = w11;
    o.ValidateState(index++, params);
    params.receiver = w111;
    o.ValidateState(index++, params);
    params.receiver = w2.get();
    o.ValidateState(index++, params);
    params.receiver = root_window();
    o.ValidateState(index++, params);

    w1.reset();
    w2.reset();
  }
}

class TestLayerAnimationObserver : public ui::LayerAnimationObserver {
 public:
  TestLayerAnimationObserver()
      : animation_completed_(false),
        animation_aborted_(false) {}
  ~TestLayerAnimationObserver() override {}

  bool animation_completed() const { return animation_completed_; }
  bool animation_aborted() const { return animation_aborted_; }

  void Reset() {
    animation_completed_ = false;
    animation_aborted_ = false;
  }

 private:
  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    animation_completed_ = true;
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    animation_aborted_ = true;
  }

  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  bool animation_completed_;
  bool animation_aborted_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimationObserver);
};

TEST_F(WindowTest, WindowDestroyCompletesAnimations) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  scoped_refptr<ui::LayerAnimator> animator =
      ui::LayerAnimator::CreateImplicitAnimator();
  TestLayerAnimationObserver observer;
  animator->AddObserver(&observer);
  // Make sure destroying a Window completes the animation.
  {
    std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
    window->layer()->SetAnimator(animator.get());

    gfx::Transform transform;
    transform.Scale(0.5f, 0.5f);
    window->SetTransform(transform);

    EXPECT_TRUE(animator->is_animating());
    EXPECT_FALSE(observer.animation_completed());
  }
  EXPECT_TRUE(animator.get());
  EXPECT_FALSE(animator->is_animating());
  EXPECT_TRUE(observer.animation_completed());
  EXPECT_FALSE(observer.animation_aborted());
  animator->RemoveObserver(&observer);
  observer.Reset();

  animator = ui::LayerAnimator::CreateImplicitAnimator();
  animator->AddObserver(&observer);
  ui::Layer layer;
  layer.SetAnimator(animator.get());
  {
    std::unique_ptr<Window> window(CreateTestWindowWithId(1, root_window()));
    window->layer()->Add(&layer);

    gfx::Transform transform;
    transform.Scale(0.5f, 0.5f);
    layer.SetTransform(transform);

    EXPECT_TRUE(animator->is_animating());
    EXPECT_FALSE(observer.animation_completed());
  }

  EXPECT_TRUE(animator.get());
  EXPECT_FALSE(animator->is_animating());
  EXPECT_TRUE(observer.animation_completed());
  EXPECT_FALSE(observer.animation_aborted());
  animator->RemoveObserver(&observer);
}

TEST_F(WindowTest, RootWindowUsesCompositorFrameSinkId) {
  EXPECT_EQ(host()->compositor()->frame_sink_id(),
            root_window()->GetFrameSinkId());
  EXPECT_TRUE(root_window()->GetFrameSinkId().is_valid());
}

TEST_F(WindowTest, LocalSurfaceIdChanges) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetBounds(gfx::Rect(300, 300));

  root_window()->AddChild(&window);

  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink(
      window.CreateLayerTreeFrameSink());
  viz::LocalSurfaceId local_surface_id1 =
      window.GetLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_NE(nullptr, frame_sink.get());
  EXPECT_TRUE(local_surface_id1.is_valid());

  // Resize to 0x0 to make sure the correct window size is stored before
  // creating the frame sink.
  window.SetBounds(gfx::Rect(0, 0));
  viz::LocalSurfaceId local_surface_id2 =
      window.GetLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_TRUE(local_surface_id2.is_valid());
  EXPECT_NE(local_surface_id1, local_surface_id2);

  window.SetBounds(gfx::Rect(300, 300));
  viz::LocalSurfaceId local_surface_id3 =
      window.GetLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_TRUE(local_surface_id3.is_valid());
  EXPECT_NE(local_surface_id1, local_surface_id3);
  EXPECT_NE(local_surface_id2, local_surface_id3);

  window.OnDeviceScaleFactorChanged(1.0f, 3.0f);
  viz::LocalSurfaceId local_surface_id4 =
      window.GetLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_TRUE(local_surface_id4.is_valid());
  EXPECT_NE(local_surface_id1, local_surface_id4);
  EXPECT_NE(local_surface_id2, local_surface_id4);
  EXPECT_NE(local_surface_id3, local_surface_id4);

  window.RecreateLayer();
  viz::LocalSurfaceId local_surface_id5 =
      window.GetLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_TRUE(local_surface_id5.is_valid());
  EXPECT_NE(local_surface_id1, local_surface_id5);
  EXPECT_NE(local_surface_id2, local_surface_id5);
  EXPECT_NE(local_surface_id3, local_surface_id5);
  EXPECT_NE(local_surface_id4, local_surface_id5);

  window.AllocateLocalSurfaceId();
  viz::LocalSurfaceId local_surface_id6 =
      window.GetLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_TRUE(local_surface_id6.is_valid());
  EXPECT_NE(local_surface_id1, local_surface_id6);
  EXPECT_NE(local_surface_id2, local_surface_id6);
  EXPECT_NE(local_surface_id3, local_surface_id6);
  EXPECT_NE(local_surface_id4, local_surface_id6);
  EXPECT_NE(local_surface_id5, local_surface_id6);
}

TEST_F(WindowTest, CreateLayerTreeFrameSink) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetBounds(gfx::Rect(300, 300));

  root_window()->AddChild(&window);

  // The window shouldn't have FrameSinkId before CreateLayerTreeFrameSink() is
  // called.
  EXPECT_FALSE(window.GetFrameSinkId().is_valid());

  std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink =
      window.CreateLayerTreeFrameSink();

  // Calling CreateLayerTreeFrameSink() should return a LayerTreeFrameSink and
  // the window should now have a FrameSinkId.
  EXPECT_NE(layer_tree_frame_sink.get(), nullptr);
  EXPECT_TRUE(window.GetFrameSinkId().is_valid());

  viz::FrameSinkId frame_sink_id = window.GetFrameSinkId();

  // Reset and recreate the LayerTreeFrameSink. This would typically happen
  // after a GPU crash.
  layer_tree_frame_sink.reset();
  layer_tree_frame_sink = window.CreateLayerTreeFrameSink();

  // A new LayerTreeFrameSink should be created for the same FrameSinkId.
  EXPECT_NE(layer_tree_frame_sink.get(), nullptr);
  EXPECT_EQ(frame_sink_id, window.GetFrameSinkId());
}

// This delegate moves its parent window to the specified one when the gesture
// ends.
class HandleGestureEndDelegate : public TestWindowDelegate {
 public:
  explicit HandleGestureEndDelegate(
      base::OnceCallback<void(Window*)> on_gesture_end)
      : on_gesture_end_(std::move(on_gesture_end)) {}
  ~HandleGestureEndDelegate() override = default;

 private:
  // WindowDelegate:
  void OnGestureEvent(ui::GestureEvent* event) override {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_END:
      case ui::ET_GESTURE_END:
      case ui::ET_GESTURE_PINCH_END: {
        if (on_gesture_end_)
          std::move(on_gesture_end_).Run(static_cast<Window*>(event->target()));
        break;
      }
      default:
        break;
    }
  }

  base::OnceCallback<void(Window*)> on_gesture_end_;
  DISALLOW_COPY_AND_ASSIGN(HandleGestureEndDelegate);
};

TEST_F(WindowTest, CleanupGestureStateChangesWindowHierarchy) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&window);
  window.SetBounds(gfx::Rect(0, 0, 100, 100));
  window.Show();
  Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&window2);
  root_window()->StackChildAtBottom(&window2);
  window2.Show();
  HandleGestureEndDelegate delegate(base::BindLambdaForTesting(
      [&](Window* target_window) { window2.AddChild(target_window); }));
  std::unique_ptr<Window> child = std::make_unique<Window>(&delegate);
  child->Init(ui::LAYER_NOT_DRAWN);
  window.AddChild(child.get());
  child->SetBounds(gfx::Rect(0, 0, 100, 100));
  child->Show();
  EXPECT_EQ(1u, window.children().size());
  EXPECT_EQ(0u, window2.children().size());

  ui::test::EventGenerator event_generator(root_window(), child.get());
  event_generator.PressTouch();
  window.CleanupGestureState();
  EXPECT_EQ(0u, window.children().size());
  EXPECT_EQ(1u, window2.children().size());
  child.reset();
}

TEST_F(WindowTest, CleanupGestureStateDeleteOtherWindows) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&window);
  window.SetBounds(gfx::Rect(0, 0, 200, 200));
  window.Show();
  std::unique_ptr<Window> child1 = std::make_unique<Window>(nullptr);
  child1->Init(ui::LAYER_NOT_DRAWN);
  child1->SetBounds(gfx::Rect(100, 100, 100, 100));
  window.AddChild(child1.get());
  child1->Show();
  HandleGestureEndDelegate delegate(base::BindLambdaForTesting(
      [&](Window* target_window) { child1.reset(); }));
  std::unique_ptr<Window> child2 = std::make_unique<Window>(&delegate);
  child2->Init(ui::LAYER_NOT_DRAWN);
  window.AddChild(child2.get());
  child2->SetBounds(gfx::Rect(0, 0, 100, 100));
  child2->Show();
  window.StackChildAtBottom(child2.get());
  EXPECT_EQ(2u, window.children().size());
  EXPECT_EQ(child2.get(), window.children().front());

  ui::test::EventGenerator event_generator(root_window(), child2.get());
  event_generator.PressTouch();
  window.CleanupGestureState();
  EXPECT_EQ(1u, window.children().size());
  EXPECT_FALSE(child1);
  child2.reset();
}

}  // namespace
}  // namespace test
}  // namespace aura
