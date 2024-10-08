// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/view_android.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/event_forwarder.h"
#include "ui/android/view_android.h"
#include "ui/android/view_android_observer.h"
#include "ui/android/window_android.h"
#include "ui/events/android/event_handler_android.h"
#include "ui/events/android/motion_event_android_java.h"
#include "ui/events/test/scoped_event_test_tick_clock.h"

namespace ui {

using base::android::JavaParamRef;

class TestViewAndroid : public ViewAndroid {
 public:
  TestViewAndroid(ViewAndroid::LayoutType layout_type)
      : ViewAndroid(layout_type) {}

  float GetDipScale() override { return 1.f; }
};

class TestEventHandler : public EventHandlerAndroid {
 public:
  TestEventHandler() {}

  bool OnTouchEvent(const MotionEventAndroid& event) override {
    touch_called_ = true;
    return handle_event_;
  }
  void OnSizeChanged() override { onsize_called_ = true; }

  void SetHandleEvent(bool handle_event) { handle_event_ = handle_event; }
  bool TouchEventHandled() { return touch_called_ && handle_event_; }
  bool TouchEventCalled() { return touch_called_; }
  bool OnSizeCalled() { return onsize_called_; }
  void Reset() {
    touch_called_ = false;
    onsize_called_ = false;
  }

 private:
  bool handle_event_{true};  // Marks as event was consumed. True by default.
  bool touch_called_{false};
  bool onsize_called_{false};
};

class ViewAndroidBoundsTest : public testing::Test {
 public:
  ViewAndroidBoundsTest()
      : root_(ViewAndroid::LayoutType::MATCH_PARENT),
        view1_(ViewAndroid::LayoutType::NORMAL),
        view2_(ViewAndroid::LayoutType::NORMAL),
        view3_(ViewAndroid::LayoutType::NORMAL),
        viewm_(ViewAndroid::LayoutType::MATCH_PARENT) {
    root_.GetEventForwarder();
    view1_.set_event_handler(&handler1_);
    view2_.set_event_handler(&handler2_);
    view3_.set_event_handler(&handler3_);
    viewm_.set_event_handler(&handlerm_);
  }

  void Reset() {
    handler1_.Reset();
    handler2_.Reset();
    handler3_.Reset();
    handlerm_.Reset();
    test_clock_.SetNowTicks(base::TimeTicks());
  }

  void GenerateTouchEventAt(float x, float y) {
    ui::MotionEventAndroid::Pointer pointer0(0, x, y, 0, 0, 0, 0, 0);
    ui::MotionEventAndroid::Pointer pointer1(0, 0, 0, 0, 0, 0, 0, 0);
    ui::MotionEventAndroidJava event(nullptr, JavaParamRef<jobject>(nullptr),
                                     1.f, 0, 0, 0, base::TimeTicks(), 0, 1, 0,
                                     0, 0, 0, 0, 0, 0, 0, 0, false, &pointer0,
                                     &pointer1);
    root_.OnTouchEvent(event);
  }

  void ExpectHit(const TestEventHandler& hitHandler) {
    TestEventHandler* handlers[4] = {&handler1_, &handler2_, &handler3_,
                                     &handlerm_};
    for (auto* handler : handlers) {
      if (&hitHandler == handler)
        EXPECT_TRUE(handler->TouchEventHandled());
      else
        EXPECT_FALSE(handler->TouchEventHandled());
    }
    Reset();
  }

  TestViewAndroid root_;
  TestViewAndroid view1_;
  TestViewAndroid view2_;
  TestViewAndroid view3_;
  TestViewAndroid viewm_;  // match-parent view
  TestEventHandler handler1_;
  TestEventHandler handler2_;
  TestEventHandler handler3_;
  TestEventHandler handlerm_;
  ui::test::ScopedEventTestTickClock test_clock_;
};

TEST_F(ViewAndroidBoundsTest, MatchesViewInFront) {
  view1_.SetLayoutForTesting(50, 50, 400, 600);
  view2_.SetLayoutForTesting(50, 50, 400, 600);
  root_.AddChild(&view2_);
  root_.AddChild(&view1_);

  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(handler1_);

  // View 2 moves up to the top, and events should hit it from now.
  root_.MoveToFront(&view2_);
  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(handler2_);

  // View 2 moves back to the bottom, and events should hit View 1 again.
  root_.MoveToBack(&view2_);
  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(handler1_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewArea) {
  view1_.SetLayoutForTesting(50, 50, 200, 200);
  view2_.SetLayoutForTesting(20, 20, 400, 600);

  root_.AddChild(&view2_);
  root_.AddChild(&view1_);

  // Falls within |view1_|'s bounds
  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(handler1_);

  // Falls within |view2_|'s bounds
  GenerateTouchEventAt(300.f, 400.f);
  ExpectHit(handler2_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewAfterMove) {
  view1_.SetLayoutForTesting(50, 50, 200, 200);
  view2_.SetLayoutForTesting(20, 20, 400, 600);
  root_.AddChild(&view2_);
  root_.AddChild(&view1_);

  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(handler1_);

  view1_.SetLayoutForTesting(150, 150, 200, 200);
  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(handler2_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewSizeOfkMatchParent) {
  view1_.SetLayoutForTesting(20, 20, 400, 600);
  view2_.SetLayoutForTesting(50, 50, 200, 200);

  root_.AddChild(&view1_);
  root_.AddChild(&view2_);
  view1_.AddChild(&viewm_);

  GenerateTouchEventAt(100.f, 100.f);
  ExpectHit(handler2_);

  GenerateTouchEventAt(300.f, 400.f);
  ExpectHit(handler1_);

  handler1_.SetHandleEvent(false);
  GenerateTouchEventAt(300.f, 400.f);
  EXPECT_TRUE(handler1_.TouchEventCalled());
  ExpectHit(handlerm_);
}

TEST_F(ViewAndroidBoundsTest, MatchesViewsWithOffset) {
  view1_.SetLayoutForTesting(10, 20, 150, 100);
  view2_.SetLayoutForTesting(20, 30, 40, 30);
  view3_.SetLayoutForTesting(70, 30, 40, 30);

  root_.AddChild(&view1_);
  view1_.AddChild(&view2_);
  view1_.AddChild(&view3_);

  GenerateTouchEventAt(70, 30);
  ExpectHit(handler1_);

  handler1_.SetHandleEvent(false);
  GenerateTouchEventAt(40, 60);
  EXPECT_TRUE(handler1_.TouchEventCalled());
  ExpectHit(handler2_);

  GenerateTouchEventAt(100, 70);
  EXPECT_TRUE(handler1_.TouchEventCalled());
  ExpectHit(handler3_);
}

TEST_F(ViewAndroidBoundsTest, OnSizeChanged) {
  root_.AddChild(&view1_);
  view1_.AddChild(&viewm_);
  view1_.AddChild(&view3_);

  // Size event propagates to non-match-parent children only.
  view1_.OnSizeChanged(100, 100);
  EXPECT_TRUE(handler1_.OnSizeCalled());
  EXPECT_TRUE(handlerm_.OnSizeCalled());
  EXPECT_FALSE(handler3_.OnSizeCalled());

  Reset();

  // Match-parent view should not receivee size events in the first place.
  EXPECT_DCHECK_DEATH(viewm_.OnSizeChanged(100, 200));
  EXPECT_FALSE(handlerm_.OnSizeCalled());
  EXPECT_FALSE(handler3_.OnSizeCalled());

  viewm_.RemoveFromParent();
  viewm_.OnSizeChangedInternal(gfx::Size(0, 0));  // Reset the size.

  Reset();

  view1_.OnSizeChanged(100, 100);

  // Size event is generated for a newly added, match-parent child view.
  EXPECT_FALSE(handlerm_.OnSizeCalled());
  view1_.AddChild(&viewm_);
  EXPECT_TRUE(handlerm_.OnSizeCalled());
  EXPECT_FALSE(handler3_.OnSizeCalled());

  viewm_.RemoveFromParent();

  Reset();

  view1_.OnSizeChanged(100, 100);

  // Size event won't propagate if the children already have the same size.
  view1_.AddChild(&viewm_);
  EXPECT_FALSE(handlerm_.OnSizeCalled());
  EXPECT_FALSE(handler3_.OnSizeCalled());
}

TEST(ViewAndroidTest, ChecksMultipleEventForwarders) {
  ViewAndroid parent;
  ViewAndroid child;
  parent.GetEventForwarder();
  child.GetEventForwarder();
  EXPECT_DCHECK_DEATH(parent.AddChild(&child));

  ViewAndroid parent2;
  ViewAndroid child2;
  parent2.GetEventForwarder();
  parent2.AddChild(&child2);
  EXPECT_DCHECK_DEATH(child2.GetEventForwarder());

  ViewAndroid window;
  ViewAndroid wcv1, wcv2;
  ViewAndroid rwhv1a, rwhv1b, rwhv2;
  wcv1.GetEventForwarder();
  wcv2.GetEventForwarder();

  window.AddChild(&wcv1);
  wcv1.AddChild(&rwhv1a);
  wcv1.AddChild(&rwhv1b);

  wcv2.AddChild(&rwhv2);

  // window should be able to add wcv2 since there's only one event forwarder
  // in the path window - wcv2* - rwvh2
  window.AddChild(&wcv2);

  // Additional event forwarder will cause failure.
  EXPECT_DCHECK_DEATH(rwhv2.GetEventForwarder());
}

class Observer : public ViewAndroidObserver {
 public:
  Observer() : attached_(false), destroyed_(false) {}

  void OnAttachedToWindow() override { attached_ = true; }

  void OnDetachedFromWindow() override { attached_ = false; }

  void OnViewAndroidDestroyed() override { destroyed_ = true; }

  bool attached_;
  bool destroyed_;
};

TEST(ViewAndroidTest, Observer) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();

  Observer top_observer;
  Observer bottom_observer;
  Observer top_observer2;

  {
    ViewAndroid top;
    ViewAndroid bottom;

    top.AddObserver(&top_observer);
    bottom.AddObserver(&bottom_observer);

    top.AddChild(&bottom);

    EXPECT_FALSE(top_observer.attached_);
    EXPECT_FALSE(bottom_observer.attached_);

    // Views in a tree all get notified of 'attached' event.
    window->get()->AddChild(&top);
    EXPECT_TRUE(top_observer.attached_);
    EXPECT_TRUE(bottom_observer.attached_);

    // Observer, upon addition, does not get notified of the current
    // attached state.
    top.AddObserver(&top_observer2);
    EXPECT_FALSE(top_observer2.attached_);

    bottom.RemoveFromParent();
    EXPECT_FALSE(bottom_observer.attached_);
    top.RemoveFromParent();
    EXPECT_FALSE(top_observer.attached_);

    window->get()->AddChild(&top);
    EXPECT_TRUE(top_observer.attached_);

    // View, upon addition to a tree in the attached state, should be notified.
    top.AddChild(&bottom);
    EXPECT_TRUE(bottom_observer.attached_);

    // Views in a tree all get notified of 'detached' event.
    top.RemoveFromParent();
    EXPECT_FALSE(top_observer.attached_);
    EXPECT_FALSE(bottom_observer.attached_);

    // Remove the second top observer to test the destruction notification.
    top.RemoveObserver(&top_observer2);
  }

  EXPECT_TRUE(top_observer.destroyed_);
  EXPECT_FALSE(top_observer2.destroyed_);
  EXPECT_TRUE(bottom_observer.destroyed_);
}

TEST(ViewAndroidTest, WindowAndroidDestructionDetachesAllViewAndroid) {
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  ViewAndroid top;
  ViewAndroid bottom;

  Observer top_observer;
  Observer bottom_observer;

  top.AddObserver(&top_observer);
  bottom.AddObserver(&bottom_observer);

  window->get()->AddChild(&top);
  top.AddChild(&bottom);

  EXPECT_TRUE(top_observer.attached_);
  EXPECT_TRUE(bottom_observer.attached_);

  window.reset();

  EXPECT_FALSE(top_observer.attached_);
  EXPECT_FALSE(bottom_observer.attached_);

  top.RemoveObserver(&top_observer);
  bottom.RemoveObserver(&bottom_observer);
}

}  // namespace ui
