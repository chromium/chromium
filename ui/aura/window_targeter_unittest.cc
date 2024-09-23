// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_targeter.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/test_event_handler.h"

namespace aura {

// Always returns the same window.
class StaticWindowTargeter : public WindowTargeter {
 public:
  explicit StaticWindowTargeter(aura::Window* window) : window_(window) {}

  StaticWindowTargeter(const StaticWindowTargeter&) = delete;
  StaticWindowTargeter& operator=(const StaticWindowTargeter&) = delete;

  ~StaticWindowTargeter() override {}

 private:
  // aura::WindowTargeter:
  Window* FindTargetForLocatedEvent(Window* window,
                                    ui::LocatedEvent* event) override {
    return window_;
  }

  raw_ptr<Window> window_;
};

gfx::RectF GetEffectiveVisibleBoundsInRootWindow(Window* window) {
  Window* root = window->GetRootWindow();
  CHECK(window->layer());
  CHECK(root->layer());
  gfx::Transform transform;
  if (!window->layer()->GetTargetTransformRelativeTo(root->layer(), &transform))
    return gfx::RectF();
  return transform.MapRect(gfx::RectF(gfx::SizeF(window->bounds().size())));
}

using WindowTargeterTest = test::AuraTestBase;

TEST_F(WindowTargeterTest, Basic) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> window(
      CreateNormalWindow(1, root_window(), &delegate));
  Window* one = CreateNormalWindow(2, window.get(), &delegate);
  Window* two = CreateNormalWindow(3, window.get(), &delegate);

  window->SetBounds(gfx::Rect(0, 0, 1000, 1000));
  one->SetBounds(gfx::Rect(0, 0, 500, 100));
  two->SetBounds(gfx::Rect(501, 0, 500, 1000));

  root_window()->Show();

  ui::test::TestEventHandler handler;
  one->AddPreTargetHandler(&handler);

  ui::MouseEvent press(ui::EventType::kMousePressed, gfx::Point(20, 20),
                       gfx::Point(20, 20), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_EQ(1, handler.num_mouse_events());

  handler.Reset();
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_EQ(1, handler.num_mouse_events());

  one->RemovePreTargetHandler(&handler);
}

TEST_F(WindowTargeterTest, FindTargetInRootWindow) {
  WindowTargeter targeter;

  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window());
  EXPECT_EQ(display.bounds(), root_window()->GetBoundsInScreen());

  // Mouse and touch presses inside the display yield null targets.
  gfx::Point inside = display.bounds().CenterPoint();
  ui::MouseEvent mouse1(ui::EventType::kMousePressed, inside, inside,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  ui::TouchEvent touch1(ui::EventType::kTouchPressed, inside,
                        ui::EventTimeForNow(), ui::PointerDetails());
  touch1.set_root_location(inside);
  EXPECT_EQ(nullptr, targeter.FindTargetInRootWindow(root_window(), mouse1));
  EXPECT_EQ(nullptr, targeter.FindTargetInRootWindow(root_window(), touch1));

  // Touch presses outside the display yields the root window as a target.
  gfx::Point outside(display.bounds().right() + 10, inside.y());
  ui::MouseEvent mouse2(ui::EventType::kMousePressed, outside, outside,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  ui::TouchEvent touch2(ui::EventType::kTouchPressed, outside,
                        ui::EventTimeForNow(), ui::PointerDetails());
  touch2.set_root_location(outside);
  EXPECT_EQ(nullptr, targeter.FindTargetInRootWindow(root_window(), mouse2));
  EXPECT_EQ(root_window(),
            targeter.FindTargetInRootWindow(root_window(), touch2));
}

TEST_F(WindowTargeterTest, ScopedWindowTargeter) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> window(
      CreateNormalWindow(1, root_window(), &delegate));
  Window* child = CreateNormalWindow(2, window.get(), &delegate);

  window->SetBounds(gfx::Rect(30, 30, 100, 100));
  child->SetBounds(gfx::Rect(20, 20, 50, 50));
  root_window()->Show();

  ui::EventTarget* root = root_window();
  ui::EventTargeter* targeter = root->GetEventTargeter();

  gfx::Point event_location(60, 60);
  {
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                         event_location, ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(child, targeter->FindTargetForEvent(root, &mouse));
  }

  // Install a targeter on |window| so that the events never reach the child.
  std::unique_ptr<ScopedWindowTargeter> scoped_targeter(
      new ScopedWindowTargeter(window.get(),
                               std::unique_ptr<WindowTargeter>(
                                   new StaticWindowTargeter(window.get()))));
  {
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                         event_location, ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root, &mouse));
  }
  scoped_targeter.reset();
  {
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                         event_location, ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(child, targeter->FindTargetForEvent(root, &mouse));
  }
}

// Test that ScopedWindowTargeter does not crash if the window for which it
// replaces the targeter gets destroyed before it does.
TEST_F(WindowTargeterTest, ScopedWindowTargeterWindowDestroyed) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> window(
      CreateNormalWindow(1, root_window(), &delegate));
  std::unique_ptr<ScopedWindowTargeter> scoped_targeter(
      new ScopedWindowTargeter(window.get(),
                               std::unique_ptr<aura::WindowTargeter>(
                                   new StaticWindowTargeter(window.get()))));

  window.reset();
  scoped_targeter.reset();

  // We did not crash!
}

TEST_F(WindowTargeterTest, TargetTransformedWindow) {
  root_window()->Show();

  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> window(
      CreateNormalWindow(2, root_window(), &delegate));

  const gfx::Rect window_bounds(100, 20, 400, 80);
  window->SetBounds(window_bounds);

  ui::EventTarget* root_target = root_window();
  ui::EventTargeter* targeter = root_target->GetEventTargeter();
  gfx::Point event_location(490, 50);
  {
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                         event_location, ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &mouse));
  }

  // Scale |window| by 50%. This should move it away from underneath
  // |event_location|, so an event in that location will not be targeted to it.
  gfx::Transform transform;
  transform.Scale(0.5, 0.5);
  window->SetTransform(transform);
  EXPECT_EQ(gfx::RectF(100, 20, 200, 40).ToString(),
            GetEffectiveVisibleBoundsInRootWindow(window.get()).ToString());
  {
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                         event_location, ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(root_window(), targeter->FindTargetForEvent(root_target, &mouse));
  }

  transform = gfx::Transform();
  transform.Translate(200, 10);
  transform.Scale(0.5, 0.5);
  window->SetTransform(transform);
  EXPECT_EQ(gfx::RectF(300, 30, 200, 40).ToString(),
            GetEffectiveVisibleBoundsInRootWindow(window.get()).ToString());
  {
    ui::MouseEvent mouse(ui::EventType::kMouseMoved, event_location,
                         event_location, ui::EventTimeForNow(), ui::EF_NONE,
                         ui::EF_NONE);
    EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &mouse));
  }
}

class IdCheckingEventTargeter : public WindowTargeter {
 public:
  explicit IdCheckingEventTargeter(int id) : id_(id) {}
  ~IdCheckingEventTargeter() override {}

 protected:
  // WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(Window* window,
                                       const ui::LocatedEvent& event) override {
    return (window->GetId() == id_ &&
            WindowTargeter::SubtreeShouldBeExploredForEvent(window, event));
  }

 private:
  int id_;
};

TEST_F(WindowTargeterTest, Bounds) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> parent(
      CreateNormalWindow(1, root_window(), &delegate));
  std::unique_ptr<Window> child(CreateNormalWindow(1, parent.get(), &delegate));
  std::unique_ptr<Window> grandchild(
      CreateNormalWindow(1, child.get(), &delegate));

  parent->SetBounds(gfx::Rect(0, 0, 30, 30));
  child->SetBounds(gfx::Rect(5, 5, 20, 20));
  grandchild->SetBounds(gfx::Rect(5, 5, 5, 5));

  ASSERT_EQ(1u, root_window()->children().size());
  ASSERT_EQ(1u, root_window()->children()[0]->children().size());
  ASSERT_EQ(1u, root_window()->children()[0]->children()[0]->children().size());

  Window* parent_r = root_window()->children()[0];
  Window* child_r = parent_r->children()[0];
  Window* grandchild_r = child_r->children()[0];

  ui::EventTarget* root_target = root_window();
  ui::EventTargeter* targeter = root_target->GetEventTargeter();

  // Dispatch a mouse event that falls on the parent, but not on the child. When
  // the default event-targeter used, the event will still reach |grandchild|,
  // because the default targeter does not look at the bounds.
  ui::MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(1, 1),
                       gfx::Point(1, 1), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  EXPECT_EQ(parent_r, targeter->FindTargetForEvent(root_target, &mouse));

  // Install a targeter on the |child| that looks at the window id as well
  // as the bounds and makes sure the event reaches the target only if the id of
  // the window is equal to 2 (incorrect). This causes the event to get handled
  // by |parent|.
  ui::MouseEvent mouse2(ui::EventType::kMouseMoved, gfx::Point(8, 8),
                        gfx::Point(8, 8), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
  std::unique_ptr<aura::WindowTargeter> original_targeter =
      child_r->SetEventTargeter(std::make_unique<IdCheckingEventTargeter>(2));
  EXPECT_EQ(parent_r, targeter->FindTargetForEvent(root_target, &mouse2));

  // Now install a targeter on the |child| that looks at the window id as well
  // as the bounds and makes sure the event reaches the target only if the id of
  // the window is equal to 1 (correct).
  ui::MouseEvent mouse3(ui::EventType::kMouseMoved, gfx::Point(8, 8),
                        gfx::Point(8, 8), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
  child_r->SetEventTargeter(std::make_unique<IdCheckingEventTargeter>(1));
  EXPECT_EQ(child_r, targeter->FindTargetForEvent(root_target, &mouse3));

  // restore original WindowTargeter for |child|.
  child_r->SetEventTargeter(std::move(original_targeter));

  // Target |grandchild| location.
  ui::MouseEvent second(ui::EventType::kMouseMoved, gfx::Point(12, 12),
                        gfx::Point(12, 12), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
  EXPECT_EQ(grandchild_r, targeter->FindTargetForEvent(root_target, &second));

  // Target |child| location.
  ui::MouseEvent third(ui::EventType::kMouseMoved, gfx::Point(8, 8),
                       gfx::Point(8, 8), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  EXPECT_EQ(child_r, targeter->FindTargetForEvent(root_target, &third));
}

TEST_F(WindowTargeterTest, NonFullyContainedBounds) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> parent(
      CreateNormalWindow(1, root_window(), &delegate));
  std::unique_ptr<Window> child(CreateNormalWindow(1, parent.get(), &delegate));

  parent->SetBounds(gfx::Rect(0, 0, 30, 30));
  child->SetBounds(gfx::Rect(15, 15, 5, 100));

  auto* targeter =
      static_cast<ui::EventTarget*>(root_window())->GetEventTargeter();

  ui::MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(17, 75),
                       gfx::Point(17, 75), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  EXPECT_EQ(child.get(), targeter->FindTargetForEvent(root_window(), &mouse));
}

TEST_F(WindowTargeterTest, NonFullyContainedBoundsWithMasksToBounds) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> parent(
      CreateNormalWindow(1, root_window(), &delegate));
  std::unique_ptr<Window> child(CreateNormalWindow(1, parent.get(), &delegate));

  parent->SetBounds(gfx::Rect(0, 0, 30, 30));
  parent->layer()->SetMasksToBounds(true);
  child->SetBounds(gfx::Rect(15, 15, 5, 100));

  auto* targeter =
      static_cast<ui::EventTarget*>(root_window())->GetEventTargeter();

  ui::MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(17, 75),
                       gfx::Point(17, 75), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  EXPECT_EQ(root_window(), targeter->FindTargetForEvent(root_window(), &mouse));
}

class IgnoreWindowTargeter : public WindowTargeter {
 public:
  IgnoreWindowTargeter() {}
  ~IgnoreWindowTargeter() override {}

 private:
  // WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(Window* window,
                                       const ui::LocatedEvent& event) override {
    return false;
  }
};

// Verifies that an EventTargeter installed on an EventTarget can dictate
// whether the target itself can process an event.
TEST_F(WindowTargeterTest, TargeterChecksOwningEventTarget) {
  test::TestWindowDelegate delegate;
  std::unique_ptr<Window> child(
      CreateNormalWindow(1, root_window(), &delegate));

  ui::EventTarget* root_target = root_window();
  ui::EventTargeter* targeter = root_target->GetEventTargeter();

  ui::MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(10, 10),
                       gfx::Point(10, 10), ui::EventTimeForNow(), ui::EF_NONE,
                       ui::EF_NONE);
  EXPECT_EQ(child.get(), targeter->FindTargetForEvent(root_target, &mouse));

  // Install an event targeter on |child| which always prevents the target from
  // receiving event.
  child->SetEventTargeter(std::make_unique<IgnoreWindowTargeter>());

  ui::MouseEvent mouse2(ui::EventType::kMouseMoved, gfx::Point(10, 10),
                        gfx::Point(10, 10), ui::EventTimeForNow(), ui::EF_NONE,
                        ui::EF_NONE);
  EXPECT_EQ(root_window(), targeter->FindTargetForEvent(root_target, &mouse2));
}

}  // namespace aura
