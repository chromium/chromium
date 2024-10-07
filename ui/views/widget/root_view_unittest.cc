// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/root_view.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget_deletion_observer.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace views::test {
namespace {

struct RootViewTestStateInit {
  gfx::Rect bounds;
  Widget::InitParams::Type type = Widget::InitParams::TYPE_WINDOW_FRAMELESS;
};

class RootViewTestState {
 public:
  explicit RootViewTestState(ViewsTestBase* delegate,
                             RootViewTestStateInit init = {}) {
    Widget::InitParams init_params = delegate->CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, init.type);
    if (init.bounds != gfx::Rect())
      init_params.bounds = init.bounds;
    widget_.Init(std::move(init_params));
    widget_.Show();
    widget_.SetContentsView(std::make_unique<View>());
  }

  Widget* widget() { return &widget_; }

  internal::RootView* GetRootView() {
    return static_cast<internal::RootView*>(widget_.GetRootView());
  }

  template <typename T>
  T* AddChildView(std::unique_ptr<T> view) {
    return widget_.GetContentsView()->AddChildView(std::move(view));
  }

 private:
  Widget widget_;
};

class DeleteOnKeyEventView : public View {
  METADATA_HEADER(DeleteOnKeyEventView, View)

 public:
  explicit DeleteOnKeyEventView(bool* set_on_key) : set_on_key_(set_on_key) {}

  DeleteOnKeyEventView(const DeleteOnKeyEventView&) = delete;
  DeleteOnKeyEventView& operator=(const DeleteOnKeyEventView&) = delete;

  ~DeleteOnKeyEventView() override = default;

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    *set_on_key_ = true;
    delete this;
    return true;
  }

 private:
  // Set to true in OnKeyPressed().
  raw_ptr<bool> set_on_key_;
};

BEGIN_METADATA(DeleteOnKeyEventView)
END_METADATA

}  // namespace

using RootViewTest = ViewsTestBase;

// Verifies deleting a View in OnKeyPressed() doesn't crash and that the
// target is marked as destroyed in the returned EventDispatchDetails.
TEST_F(RootViewTest, DeleteViewDuringKeyEventDispatch) {
  RootViewTestState state(this);
  internal::RootView* root_view = state.GetRootView();

  bool got_key_event = false;
  View* child = state.AddChildView(
      std::make_unique<DeleteOnKeyEventView>(&got_key_event));

  // Give focus to |child| so that it will be the target of the key event.
  child->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  child->RequestFocus();

  ViewTargeter* view_targeter = new ViewTargeter(root_view);
  root_view->SetEventTargeter(base::WrapUnique(view_targeter));

  ui::KeyEvent key_event(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE,
                         ui::EF_NONE);
  ui::EventDispatchDetails details = root_view->OnEventFromSource(&key_event);
  EXPECT_TRUE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(got_key_event);
}

// Tracks whether a context menu is shown.
class TestContextMenuController : public ContextMenuController {
 public:
  TestContextMenuController() = default;

  TestContextMenuController(const TestContextMenuController&) = delete;
  TestContextMenuController& operator=(const TestContextMenuController&) =
      delete;

  ~TestContextMenuController() override = default;

  int show_context_menu_calls() const { return show_context_menu_calls_; }
  View* menu_source_view() const { return menu_source_view_; }
  ui::MenuSourceType menu_source_type() const { return menu_source_type_; }

  void Reset() {
    show_context_menu_calls_ = 0;
    menu_source_view_ = nullptr;
    menu_source_type_ = ui::MENU_SOURCE_NONE;
  }

  // ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
    show_context_menu_calls_++;
    menu_source_view_ = source;
    menu_source_type_ = source_type;
  }

 private:
  int show_context_menu_calls_ = 0;
  raw_ptr<View> menu_source_view_ = nullptr;
  ui::MenuSourceType menu_source_type_ = ui::MENU_SOURCE_NONE;
};

// Tests that context menus are shown for certain key events (Shift+F10
// and VKEY_APPS) by the pre-target handler installed on RootView.
TEST_F(RootViewTest, ContextMenuFromKeyEvent) {
  // This behavior is intentionally unsupported on macOS.
#if !BUILDFLAG(IS_MAC)
  RootViewTestState state(this);
  internal::RootView* root_view = state.GetRootView();

  TestContextMenuController controller;
  View* focused_view = root_view->GetContentsView();
  focused_view->set_context_menu_controller(&controller);
  focused_view->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  focused_view->RequestFocus();

  // No context menu should be shown for a keypress of 'A'.
  ui::KeyEvent nomenu_key_event = ui::KeyEvent::FromCharacter(
      'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE);
  ui::EventDispatchDetails details =
      root_view->OnEventFromSource(&nomenu_key_event);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
  EXPECT_EQ(nullptr, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_NONE, controller.menu_source_type());
  controller.Reset();

  // A context menu should be shown for a keypress of Shift+F10.
  ui::KeyEvent menu_key_event(ui::EventType::kKeyPressed, ui::VKEY_F10,
                              ui::EF_SHIFT_DOWN);
  details = root_view->OnEventFromSource(&menu_key_event);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  EXPECT_EQ(focused_view, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_KEYBOARD, controller.menu_source_type());
  controller.Reset();

  // A context menu should be shown for a keypress of VKEY_APPS.
  ui::KeyEvent menu_key_event2(ui::EventType::kKeyPressed, ui::VKEY_APPS,
                               ui::EF_NONE);
  details = root_view->OnEventFromSource(&menu_key_event2);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  EXPECT_EQ(focused_view, controller.menu_source_view());
  EXPECT_EQ(ui::MENU_SOURCE_KEYBOARD, controller.menu_source_type());
  controller.Reset();
#endif
}

// View which handles all gesture events.
class GestureHandlingView : public View {
  METADATA_HEADER(GestureHandlingView, View)

 public:
  GestureHandlingView() = default;

  GestureHandlingView(const GestureHandlingView&) = delete;
  GestureHandlingView& operator=(const GestureHandlingView&) = delete;

  ~GestureHandlingView() override = default;

  void OnGestureEvent(ui::GestureEvent* event) override { event->SetHandled(); }
};

BEGIN_METADATA(GestureHandlingView)
END_METADATA

// View which handles all mouse events.
class MouseHandlingView : public View {
  METADATA_HEADER(MouseHandlingView, View)

 public:
  MouseHandlingView() = default;
  MouseHandlingView(const MouseHandlingView&) = delete;
  MouseHandlingView& operator=(const MouseHandlingView&) = delete;
  ~MouseHandlingView() override = default;

  // View:
  void OnMouseEvent(ui::MouseEvent* event) override { event->SetHandled(); }
};

BEGIN_METADATA(MouseHandlingView)
END_METADATA

TEST_F(RootViewTest, EventHandlersResetWhenDeleted) {
  RootViewTestState state(this, {.bounds = {100, 100}});
  internal::RootView* root_view = state.GetRootView();

  // Set up a child view to handle events.
  View* event_handler = state.AddChildView(std::make_unique<View>());
  root_view->SetMouseAndGestureHandler(event_handler);
  ASSERT_EQ(event_handler, root_view->gesture_handler_for_testing());
  ASSERT_EQ(event_handler, root_view->mouse_pressed_handler_for_testing());

  // Delete the child and expect that there is no longer a mouse handler.
  root_view->GetContentsView()->RemoveChildViewT(event_handler);
  EXPECT_EQ(nullptr, root_view->gesture_handler_for_testing());
  EXPECT_EQ(nullptr, root_view->mouse_pressed_handler_for_testing());
}

TEST_F(RootViewTest, EventHandlersNotResetWhenReparented) {
  RootViewTestState state(this, {.bounds = {100, 100}});
  internal::RootView* root_view = state.GetRootView();

  // Set up a child view to handle events
  View* event_handler = state.AddChildView(std::make_unique<View>());
  root_view->SetMouseAndGestureHandler(event_handler);
  ASSERT_EQ(event_handler, root_view->gesture_handler_for_testing());

  // Reparent the child within the hierarchy and expect that it's still the
  // mouse handler.
  View* other_parent = state.AddChildView(std::make_unique<View>());
  other_parent->AddChildView(event_handler);
  EXPECT_EQ(event_handler, root_view->gesture_handler_for_testing());
}

// Verifies that the gesture handler stored in the root view is reset after
// mouse is released. Note that during mouse event handling,
// `RootView::SetMouseAndGestureHandler()` may be called to set the gesture
// handler. Therefore we should reset the gesture handler when mouse is
// released. We may remove this test in the future if the implementation of the
// product code changes.
TEST_F(RootViewTest, GestureHandlerResetAfterMouseReleased) {
  RootViewTestState state(this, {.bounds = {100, 100}});
  internal::RootView* root_view = state.GetRootView();

  // Create a child view to handle gestures.
  View* gesture_handler =
      state.AddChildView(std::make_unique<GestureHandlingView>());
  gesture_handler->SetBoundsRect(gfx::Rect(gfx::Size{50, 50}));

  // Create a child view to handle mouse events.
  View* mouse_handler =
      state.AddChildView(std::make_unique<MouseHandlingView>());
  mouse_handler->SetBoundsRect(
      gfx::Rect(gesture_handler->bounds().bottom_right(), gfx::Size{50, 50}));

  // Emulate to start gesture scroll on `child_view`.
  const gfx::Point gesture_handler_center_point =
      gesture_handler->GetBoundsInScreen().CenterPoint();
  ui::GestureEvent scroll_begin(
      gesture_handler_center_point.x(), gesture_handler_center_point.y(),
      ui::EF_NONE, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin));
  root_view->OnEventFromSource(&scroll_begin);
  ui::GestureEvent scroll_update(
      gesture_handler_center_point.x(), gesture_handler_center_point.y(),
      ui::EF_NONE, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate,
                              /*delta_x=*/20,
                              /*delta_y=*/10));
  root_view->OnEventFromSource(&scroll_update);

  // Emulate the mouse click on `mouse_handler` before gesture scroll ends.
  const gfx::Point mouse_handler_center_point =
      mouse_handler->GetBoundsInScreen().CenterPoint();
  ui::MouseEvent pressed_event(
      ui::EventType::kMousePressed, mouse_handler_center_point,
      mouse_handler_center_point, ui::EventTimeForNow(), ui::EF_NONE,
      /*changed_button_flags=*/0);
  ui::MouseEvent released_event(
      ui::EventType::kMouseReleased, mouse_handler_center_point,
      mouse_handler_center_point, ui::EventTimeForNow(), ui::EF_NONE,
      /*changed_button_flags=*/0);
  root_view->OnMousePressed(pressed_event);
  root_view->OnMouseReleased(released_event);

  // Check that the gesture handler is reset.
  EXPECT_EQ(nullptr, root_view->gesture_handler_for_testing());
}

// Tests that context menus are shown for long press by the post-target handler
// installed on the RootView only if the event is targetted at a view which can
// show a context menu.
TEST_F(RootViewTest, ContextMenuFromLongPress) {
  RootViewTestState state(
      this, {.bounds = {100, 100}, .type = Widget::InitParams::TYPE_POPUP});
  internal::RootView* root_view = state.GetRootView();
  View* parent_view = root_view->GetContentsView();

  // Create a view capable of showing the context menu with two children one of
  // which handles all gesture events (e.g. a button).
  TestContextMenuController controller;
  parent_view->set_context_menu_controller(&controller);

  View* gesture_handling_child_view = new GestureHandlingView;
  gesture_handling_child_view->SetBoundsRect(gfx::Rect(10, 10));
  parent_view->AddChildView(gesture_handling_child_view);

  View* other_child_view = new View;
  other_child_view->SetBoundsRect(gfx::Rect(20, 0, 10, 10));
  parent_view->AddChildView(other_child_view);

  // |parent_view| should not show a context menu as a result of a long press on
  // |gesture_handling_child_view|.
  ui::GestureEvent long_press1(
      5, 5, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  ui::EventDispatchDetails details = root_view->OnEventFromSource(&long_press1);

  ui::GestureEvent end1(5, 5, 0, base::TimeTicks(),
                        ui::GestureEventDetails(ui::EventType::kGestureEnd));
  details = root_view->OnEventFromSource(&end1);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
  controller.Reset();

  // |parent_view| should show a context menu as a result of a long press on
  // |other_child_view|.
  ui::GestureEvent long_press2(
      25, 5, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  details = root_view->OnEventFromSource(&long_press2);

  ui::GestureEvent end2(25, 5, 0, base::TimeTicks(),
                        ui::GestureEventDetails(ui::EventType::kGestureEnd));
  details = root_view->OnEventFromSource(&end2);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
  controller.Reset();

  // |parent_view| should show a context menu as a result of a long press on
  // itself.
  ui::GestureEvent long_press3(
      50, 50, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  details = root_view->OnEventFromSource(&long_press3);

  ui::GestureEvent end3(25, 5, 0, base::TimeTicks(),
                        ui::GestureEventDetails(ui::EventType::kGestureEnd));
  details = root_view->OnEventFromSource(&end3);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(1, controller.show_context_menu_calls());
}

// Tests that context menus are not shown for disabled views on a long press.
TEST_F(RootViewTest, ContextMenuFromLongPressOnDisabledView) {
  RootViewTestState state(
      this, {.bounds = {100, 100}, .type = Widget::InitParams::TYPE_POPUP});
  internal::RootView* root_view = state.GetRootView();
  View* parent_view = root_view->GetContentsView();

  // Create a view capable of showing the context menu with two children one of
  // which handles all gesture events (e.g. a button). Also mark this view
  // as disabled.
  TestContextMenuController controller;
  parent_view->set_context_menu_controller(&controller);
  parent_view->SetEnabled(false);

  View* gesture_handling_child_view = new GestureHandlingView;
  gesture_handling_child_view->SetBoundsRect(gfx::Rect(10, 10));
  parent_view->AddChildView(gesture_handling_child_view);

  View* other_child_view = new View;
  other_child_view->SetBoundsRect(gfx::Rect(20, 0, 10, 10));
  parent_view->AddChildView(other_child_view);

  // |parent_view| should not show a context menu as a result of a long press on
  // |gesture_handling_child_view|.
  ui::GestureEvent long_press1(
      5, 5, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  ui::EventDispatchDetails details = root_view->OnEventFromSource(&long_press1);

  ui::GestureEvent end1(5, 5, 0, base::TimeTicks(),
                        ui::GestureEventDetails(ui::EventType::kGestureEnd));
  details = root_view->OnEventFromSource(&end1);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
  controller.Reset();

  // |parent_view| should not show a context menu as a result of a long press on
  // |other_child_view|.
  ui::GestureEvent long_press2(
      25, 5, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  details = root_view->OnEventFromSource(&long_press2);

  ui::GestureEvent end2(25, 5, 0, base::TimeTicks(),
                        ui::GestureEventDetails(ui::EventType::kGestureEnd));
  details = root_view->OnEventFromSource(&end2);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
  controller.Reset();

  // |parent_view| should not show a context menu as a result of a long press on
  // itself.
  ui::GestureEvent long_press3(
      50, 50, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  details = root_view->OnEventFromSource(&long_press3);

  ui::GestureEvent end3(25, 5, 0, base::TimeTicks(),
                        ui::GestureEventDetails(ui::EventType::kGestureEnd));
  details = root_view->OnEventFromSource(&end3);

  EXPECT_FALSE(details.target_destroyed);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_EQ(0, controller.show_context_menu_calls());
}

namespace {

// View class which destroys itself when it gets an event of type
// |delete_event_type|.
class DeleteViewOnEvent : public View {
  METADATA_HEADER(DeleteViewOnEvent, View)

 public:
  DeleteViewOnEvent(ui::EventType delete_event_type, bool* was_destroyed)
      : delete_event_type_(delete_event_type), was_destroyed_(was_destroyed) {}

  DeleteViewOnEvent(const DeleteViewOnEvent&) = delete;
  DeleteViewOnEvent& operator=(const DeleteViewOnEvent&) = delete;

  ~DeleteViewOnEvent() override { *was_destroyed_ = true; }

  void OnEvent(ui::Event* event) override {
    if (event->type() == delete_event_type_)
      delete this;
  }

 private:
  // The event type which causes the view to destroy itself.
  ui::EventType delete_event_type_;

  // Tracks whether the view was destroyed.
  raw_ptr<bool> was_destroyed_;
};

BEGIN_METADATA(DeleteViewOnEvent)
END_METADATA

// View class which remove itself when it gets an event of type
// |remove_event_type|.
class RemoveViewOnEvent : public View {
  METADATA_HEADER(RemoveViewOnEvent, View)

 public:
  explicit RemoveViewOnEvent(ui::EventType remove_event_type)
      : remove_event_type_(remove_event_type) {}

  RemoveViewOnEvent(const RemoveViewOnEvent&) = delete;
  RemoveViewOnEvent& operator=(const RemoveViewOnEvent&) = delete;

  void OnEvent(ui::Event* event) override {
    if (event->type() == remove_event_type_)
      parent()->RemoveChildView(this);
  }

 private:
  // The event type which causes the view to remove itself.
  ui::EventType remove_event_type_;
};

BEGIN_METADATA(RemoveViewOnEvent)
END_METADATA

// View class which generates a nested event the first time it gets an event of
// type |nested_event_type|. This is used to simulate nested event loops which
// can cause |RootView::mouse_event_handler_| to get reset.
class NestedEventOnEvent : public View {
  METADATA_HEADER(NestedEventOnEvent, View)

 public:
  NestedEventOnEvent(ui::EventType nested_event_type, View* root_view)
      : nested_event_type_(nested_event_type), root_view_(root_view) {}

  NestedEventOnEvent(const NestedEventOnEvent&) = delete;
  NestedEventOnEvent& operator=(const NestedEventOnEvent&) = delete;

  void OnEvent(ui::Event* event) override {
    if (event->type() == nested_event_type_) {
      ui::MouseEvent exit_event(ui::EventType::kMouseExited, gfx::Point(),
                                gfx::Point(), ui::EventTimeForNow(),
                                ui::EF_NONE, ui::EF_NONE);
      // Avoid infinite recursion if |nested_event_type_| ==
      // EventType::kMouseExited.
      nested_event_type_ = ui::EventType::kUnknown;
      root_view_->OnMouseExited(exit_event);
    }
  }

 private:
  // The event type which causes the view to generate a nested event.
  ui::EventType nested_event_type_;
  // root view of this view; owned by widget.
  raw_ptr<View> root_view_;
};

BEGIN_METADATA(NestedEventOnEvent)
END_METADATA

}  // namespace

// Verifies deleting a View in OnMouseExited() doesn't crash.
TEST_F(RootViewTest, DeleteViewOnMouseExitDispatch) {
  RootViewTestState state(this, {.bounds = {10, 10, 500, 500},
                                 .type = Widget::InitParams::TYPE_POPUP});
  internal::RootView* root_view = state.GetRootView();
  bool view_destroyed = false;

  View* child = state.AddChildView(std::make_unique<DeleteViewOnEvent>(
      ui::EventType::kMouseExited, &view_destroyed));
  child->SetBounds(10, 10, 500, 500);

  // Generate a mouse move event which ensures that |mouse_moved_handler_|
  // is set in the RootView class.
  ui::MouseEvent moved_event(ui::EventType::kMouseMoved, gfx::Point(15, 15),
                             gfx::Point(15, 15), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(moved_event);
  ASSERT_FALSE(view_destroyed);

  // Generate a mouse exit event which in turn will delete the child view which
  // was the target of the mouse move event above. This should not crash when
  // the mouse exit handler returns from the child.
  ui::MouseEvent exit_event(ui::EventType::kMouseExited, gfx::Point(),
                            gfx::Point(), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseExited(exit_event);

  EXPECT_TRUE(view_destroyed);
  EXPECT_TRUE(root_view->GetContentsView()->children().empty());
}

// Verifies deleting a View in OnMouseEntered() doesn't crash.
TEST_F(RootViewTest, DeleteViewOnMouseEnterDispatch) {
  RootViewTestState state(this, {.bounds = {10, 10, 500, 500},
                                 .type = Widget::InitParams::TYPE_POPUP});
  internal::RootView* root_view = state.GetRootView();
  bool view_destroyed = false;

  View* child = state.AddChildView(std::make_unique<DeleteViewOnEvent>(
      ui::EventType::kMouseEntered, &view_destroyed));

  // Make |child| smaller than the containing Widget and RootView.
  child->SetBounds(100, 100, 100, 100);

  // Move the mouse within |widget| but outside of |child|.
  ui::MouseEvent moved_event(ui::EventType::kMouseMoved, gfx::Point(15, 15),
                             gfx::Point(15, 15), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(moved_event);
  ASSERT_FALSE(view_destroyed);

  // Move the mouse within |child|, which should dispatch a mouse enter event to
  // |child| and destroy the view. This should not crash when the mouse enter
  // handler returns from the child.
  ui::MouseEvent moved_event2(ui::EventType::kMouseMoved, gfx::Point(115, 115),
                              gfx::Point(115, 115), ui::EventTimeForNow(), 0,
                              0);
  root_view->OnMouseMoved(moved_event2);

  EXPECT_TRUE(view_destroyed);
  EXPECT_TRUE(root_view->GetContentsView()->children().empty());
}

// Verifies removing a View in OnMouseEntered() doesn't crash.
TEST_F(RootViewTest, RemoveViewOnMouseEnterDispatch) {
  RootViewTestState state(this, {.bounds = {10, 10, 500, 500},
                                 .type = Widget::InitParams::TYPE_POPUP});
  internal::RootView* root_view = state.GetRootView();
  View* content = root_view->GetContentsView();

  // |child| gets removed without being deleted, so make it a local
  // to prevent test memory leak.
  RemoveViewOnEvent child(ui::EventType::kMouseEntered);

  content->AddChildView(&child);

  // Make |child| smaller than the containing Widget and RootView.
  child.SetBounds(100, 100, 100, 100);

  // Move the mouse within |widget| but outside of |child|.
  ui::MouseEvent moved_event(ui::EventType::kMouseMoved, gfx::Point(15, 15),
                             gfx::Point(15, 15), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(moved_event);

  // Move the mouse within |child|, which should dispatch a mouse enter event to
  // |child| and remove the view. This should not crash when the mouse enter
  // handler returns.
  ui::MouseEvent moved_event2(ui::EventType::kMouseMoved, gfx::Point(115, 115),
                              gfx::Point(115, 115), ui::EventTimeForNow(), 0,
                              0);
  root_view->OnMouseMoved(moved_event2);

  EXPECT_TRUE(content->children().empty());
}

// Verifies clearing the root view's |mouse_move_handler_| in OnMouseExited()
// doesn't crash.
TEST_F(RootViewTest, ClearMouseMoveHandlerOnMouseExitDispatch) {
  RootViewTestState state(this, {.bounds = {10, 10, 500, 500},
                                 .type = Widget::InitParams::TYPE_POPUP});
  internal::RootView* root_view = state.GetRootView();

  View* child = state.AddChildView(std::make_unique<NestedEventOnEvent>(
      ui::EventType::kMouseExited, root_view));
  // Make |child| smaller than the containing Widget and RootView.
  child->SetBounds(100, 100, 100, 100);

  // Generate a mouse move event which ensures that |mouse_moved_handler_|
  // is set to the child view in the RootView class.
  ui::MouseEvent moved_event(ui::EventType::kMouseMoved, gfx::Point(110, 110),
                             gfx::Point(110, 110), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(moved_event);

  // Move the mouse outside of |child| which causes a mouse exit event to be
  // dispatched  to |child|, which will in turn generate a nested event that
  // clears |mouse_move_handler_|. This should not crash
  // RootView::OnMouseMoved.
  ui::MouseEvent move_event2(ui::EventType::kMouseMoved, gfx::Point(15, 15),
                             gfx::Point(15, 15), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(move_event2);
}

// Verifies clearing the root view's |mouse_move_handler_| in OnMouseExited()
// doesn't crash, in the case where the root view is targeted, because
// it's the first enabled view encountered walking up the target tree.
TEST_F(RootViewTest,
       ClearMouseMoveHandlerOnMouseExitDispatchWithContentViewDisabled) {
  RootViewTestState state(this, {.bounds = {10, 10, 500, 500},
                                 .type = Widget::InitParams::TYPE_POPUP});
  internal::RootView* root_view = state.GetRootView();

  View* child = state.AddChildView(std::make_unique<NestedEventOnEvent>(
      ui::EventType::kMouseExited, root_view));

  // Make |child| smaller than the containing Widget and RootView.
  child->SetBounds(100, 100, 100, 100);

  // Generate a mouse move event which ensures that the |mouse_moved_handler_|
  // member is set to the child view in the RootView class.
  ui::MouseEvent moved_event(ui::EventType::kMouseMoved, gfx::Point(110, 110),
                             gfx::Point(110, 110), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(moved_event);

  // This will make RootView::OnMouseMoved skip the content view when looking
  // for a handler for the mouse event, and instead use the root view.
  root_view->GetContentsView()->SetEnabled(false);
  // Move the mouse outside of |child| which should dispatch a mouse exit event
  // to |mouse_move_handler_| (currently |child|), which will in turn generate a
  // nested event that clears |mouse_move_handler_|. This should not crash
  // RootView::OnMouseMoved.
  ui::MouseEvent move_event2(ui::EventType::kMouseMoved, gfx::Point(200, 200),
                             gfx::Point(200, 200), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(move_event2);
}

// Verifies clearing the root view's |mouse_move_handler_| in OnMouseEntered()
// doesn't crash.
TEST_F(RootViewTest, ClearMouseMoveHandlerOnMouseEnterDispatch) {
  RootViewTestState state(this, {.bounds = {10, 10, 500, 500},
                                 .type = Widget::InitParams::TYPE_POPUP});
  internal::RootView* root_view = state.GetRootView();

  View* child = state.AddChildView(std::make_unique<NestedEventOnEvent>(
      ui::EventType::kMouseEntered, root_view));

  // Make |child| smaller than the containing Widget and RootView.
  child->SetBounds(100, 100, 100, 100);

  // Move the mouse within |widget| but outside of |child|.
  ui::MouseEvent moved_event(ui::EventType::kMouseMoved, gfx::Point(15, 15),
                             gfx::Point(15, 15), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(moved_event);

  // Move the mouse within |child|, which dispatches a mouse enter event to
  // |child| and resets the root view's |mouse_move_handler_|. This should not
  // crash when the mouse enter handler generates an EventType::kMouseEntered
  // event.
  ui::MouseEvent moved_event2(ui::EventType::kMouseMoved, gfx::Point(115, 115),
                              gfx::Point(115, 115), ui::EventTimeForNow(), 0,
                              0);
  root_view->OnMouseMoved(moved_event2);
}

namespace {

// View class which deletes its owning Widget when it gets a mouse exit event.
class DeleteWidgetOnMouseExit : public View {
  METADATA_HEADER(DeleteWidgetOnMouseExit, View)

 public:
  explicit DeleteWidgetOnMouseExit(base::OnceClosure on_mouse_exit_callback)
      : on_mouse_exit_callback_(std::move(on_mouse_exit_callback)) {}

  DeleteWidgetOnMouseExit(const DeleteWidgetOnMouseExit&) = delete;
  DeleteWidgetOnMouseExit& operator=(const DeleteWidgetOnMouseExit&) = delete;

  ~DeleteWidgetOnMouseExit() override = default;

  void OnMouseExited(const ui::MouseEvent& event) override {
    std::move(on_mouse_exit_callback_).Run();
  }

 private:
  base::OnceClosure on_mouse_exit_callback_;
};

BEGIN_METADATA(DeleteWidgetOnMouseExit)
END_METADATA

}  // namespace

// Test that there is no crash if a View deletes its parent Widget in
// View::OnMouseExited().
TEST_F(RootViewTest, DeleteWidgetOnMouseExitDispatch) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(init_params));
  widget->SetBounds(gfx::Rect(10, 10, 500, 500));
  WidgetDeletionObserver widget_deletion_observer(widget.get());

  auto content = std::make_unique<View>();
  auto* child = content->AddChildView(std::make_unique<DeleteWidgetOnMouseExit>(
      base::BindOnce([](std::unique_ptr<Widget>* widget) { widget->reset(); },
                     base::Unretained(&widget))));
  widget->SetContentsView(std::move(content));

  // Make |child| smaller than the containing Widget and RootView.
  child->SetBounds(100, 100, 100, 100);

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());

  // Move the mouse within |child|.
  ui::MouseEvent moved_event(ui::EventType::kMouseMoved, gfx::Point(115, 115),
                             gfx::Point(115, 115), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(moved_event);
  ASSERT_TRUE(widget_deletion_observer.IsWidgetAlive());

  // Move the mouse outside of |child| which should dispatch a mouse exit event
  // to |child| and destroy the widget. This should not crash when the mouse
  // exit handler returns from the child.
  ui::MouseEvent move_event2(ui::EventType::kMouseMoved, gfx::Point(15, 15),
                             gfx::Point(15, 15), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(move_event2);
  EXPECT_FALSE(widget_deletion_observer.IsWidgetAlive());
}

// Test that there is no crash if a View deletes its parent widget as a result
// of a mouse exited event which was propagated from one of its children.
TEST_F(RootViewTest, DeleteWidgetOnMouseExitDispatchFromChild) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams init_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  widget->Init(std::move(init_params));
  widget->SetBounds(gfx::Rect(10, 10, 500, 500));
  WidgetDeletionObserver widget_deletion_observer(widget.get());

  auto* content = widget->SetContentsView(std::make_unique<View>());
  auto* child = content->AddChildView(std::make_unique<DeleteWidgetOnMouseExit>(
      base::BindOnce([](std::unique_ptr<Widget>* widget) { widget->reset(); },
                     base::Unretained(&widget))));
  auto* subchild = child->AddChildView(std::make_unique<View>());

  // Make |child| and |subchild| smaller than the containing Widget and
  // RootView.
  child->SetBounds(100, 100, 100, 100);
  subchild->SetBounds(0, 0, 100, 100);

  // Make mouse enter and exit events get propagated from |subchild| to |child|.
  child->SetNotifyEnterExitOnChild(true);

  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget->GetRootView());

  // Move the mouse within |subchild| and |child|.
  ui::MouseEvent moved_event(ui::EventType::kMouseMoved, gfx::Point(115, 115),
                             gfx::Point(115, 115), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(moved_event);
  ASSERT_TRUE(widget_deletion_observer.IsWidgetAlive());

  // Move the mouse outside of |subchild| and |child| which should dispatch a
  // mouse exit event to |subchild| and destroy the widget. This should not
  // crash when the mouse exit handler returns from |subchild|.
  ui::MouseEvent move_event2(ui::EventType::kMouseMoved, gfx::Point(15, 15),
                             gfx::Point(15, 15), ui::EventTimeForNow(), 0, 0);
  root_view->OnMouseMoved(move_event2);
  EXPECT_FALSE(widget_deletion_observer.IsWidgetAlive());
}

namespace {
class RootViewTestDialogDelegate : public DialogDelegateView {
 public:
  RootViewTestDialogDelegate() {
    // Ensure that buttons don't influence the layout.
    DialogDelegate::SetButtons(
        static_cast<int>(ui::mojom::DialogButton::kNone));
  }

  RootViewTestDialogDelegate(const RootViewTestDialogDelegate&) = delete;
  RootViewTestDialogDelegate& operator=(const RootViewTestDialogDelegate&) =
      delete;

  ~RootViewTestDialogDelegate() override = default;

  int layout_count() const { return layout_count_; }

  // DialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const SizeBounds& /*available_size*/) const override {
    return preferred_size_;
  }

  void Layout(PassKey) override {
    EXPECT_EQ(size(), preferred_size_);
    ++layout_count_;
  }

 private:
  const gfx::Size preferred_size_ = gfx::Size(111, 111);

  int layout_count_ = 0;
};
}  // namespace

// Ensure only one layout happens during Widget initialization, and ensure it
// happens at the ContentView's preferred size.
TEST_F(RootViewTest, SingleLayoutDuringInit) {
  RootViewTestDialogDelegate* delegate = new RootViewTestDialogDelegate();
  Widget* widget =
      DialogDelegate::CreateDialogWidget(delegate, GetContext(), nullptr);
  EXPECT_EQ(1, delegate->layout_count());
  widget->CloseNow();
}

using RootViewDesktopNativeWidgetTest = ViewsTestWithDesktopNativeWidget;

// Also test Aura desktop Widget codepaths.
TEST_F(RootViewDesktopNativeWidgetTest, SingleLayoutDuringInit) {
  RootViewTestDialogDelegate* delegate = new RootViewTestDialogDelegate();
  Widget* widget =
      DialogDelegate::CreateDialogWidget(delegate, GetContext(), nullptr);
  EXPECT_EQ(1, delegate->layout_count());
  widget->CloseNow();
}

#if !BUILDFLAG(IS_MAC)

// Tests that AnnounceAlert sets up the correct text value on the hidden
// view, and that the resulting hidden view actually stays hidden.
TEST_F(RootViewTest, AnnounceTextAsTest) {
  RootViewTestState state(this, {.bounds = {100, 100, 100, 100}});
  internal::RootView* root_view = state.GetRootView();

  EXPECT_EQ(1U, root_view->children().size());
  const std::u16string kAlertText = u"Alert";
  root_view->AnnounceTextAs(kAlertText,
                            ui::AXPlatformNode::AnnouncementType::kAlert);
  EXPECT_EQ(2U, root_view->children().size());
  views::test::RunScheduledLayout(root_view);
  EXPECT_FALSE(root_view->children()[0]->size().IsEmpty());
  EXPECT_TRUE(root_view->children()[1]->size().IsEmpty());
  View* const hidden_alert_view = root_view->children()[1];
  ui::AXNodeData node_data;
  hidden_alert_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(kAlertText,
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(node_data.role, ax::mojom::Role::kStaticText);
#elif BUILDFLAG(IS_LINUX)
  EXPECT_EQ(node_data.role, ax::mojom::Role::kAlert);
#else
  EXPECT_EQ(node_data.role, ax::mojom::Role::kAlert);
#endif

  const std::u16string kPoliteText = u"Something polite";
  root_view->AnnounceTextAs(kPoliteText,
                            ui::AXPlatformNode::AnnouncementType::kPolite);
  View* const hidden_polite_view = root_view->children()[1];
  hidden_polite_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_EQ(kPoliteText,
            node_data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  hidden_polite_view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  ASSERT_TRUE(node_data.HasStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus));
  const std::string& val = node_data.GetStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus);
  ASSERT_EQ("polite", val);

#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(node_data.role, ax::mojom::Role::kStaticText);
#elif BUILDFLAG(IS_LINUX)
  EXPECT_EQ(node_data.role, ax::mojom::Role::kAlert);
#else
  EXPECT_EQ(node_data.role, ax::mojom::Role::kStatus);
#endif
}

#endif  // !BUILDFLAG(IS_MAC)

TEST_F(RootViewTest, MouseEventDispatchedToClosestEnabledView) {
  RootViewTestState state(this, {.bounds = {100, 100, 100, 100}});
  internal::RootView* root_view = state.GetRootView();

  View* const contents_view = root_view->GetContentsView();
  EventCountView* const v1 =
      contents_view->AddChildView(std::make_unique<EventCountView>());
  EventCountView* const v2 =
      v1->AddChildView(std::make_unique<EventCountView>());
  EventCountView* const v3 =
      v2->AddChildView(std::make_unique<EventCountView>());

  contents_view->SetBoundsRect(gfx::Rect(0, 0, 10, 10));
  v1->SetBoundsRect(gfx::Rect(0, 0, 10, 10));
  v2->SetBoundsRect(gfx::Rect(0, 0, 10, 10));
  v3->SetBoundsRect(gfx::Rect(0, 0, 10, 10));

  v1->set_handle_mode(EventCountView::CONSUME_EVENTS);
  v2->set_handle_mode(EventCountView::CONSUME_EVENTS);
  v3->set_handle_mode(EventCountView::CONSUME_EVENTS);

  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                               gfx::Point(5, 5), ui::EventTimeForNow(), 0, 0);
  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::Point(5, 5),
                                gfx::Point(5, 5), ui::EventTimeForNow(), 0, 0);
  root_view->OnMousePressed(pressed_event);
  root_view->OnMouseReleased(released_event);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(0, v2->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kMousePressed));

  v3->SetEnabled(false);
  root_view->OnMousePressed(pressed_event);
  root_view->OnMouseReleased(released_event);
  EXPECT_EQ(0, v1->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(1, v2->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kMousePressed));

  v3->SetEnabled(true);
  v2->SetEnabled(false);
  root_view->OnMousePressed(pressed_event);
  root_view->OnMouseReleased(released_event);
  EXPECT_EQ(1, v1->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(1, v2->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_EQ(1, v3->GetEventCount(ui::EventType::kMousePressed));
}

// If RootView::OnMousePressed() receives a double-click event that isn't
// handled by any views, it should still report it as handled if the first click
// was handled. However, it should *not* if the first click was unhandled.
// Regression test for https://crbug.com/1055674.
TEST_F(RootViewTest, DoubleClickHandledIffFirstClickHandled) {
  RootViewTestState state(this, {.bounds = {100, 100, 100, 100}});
  internal::RootView* root_view = state.GetRootView();

  View* const contents_view = root_view->GetContentsView();
  EventCountView* const v1 =
      contents_view->AddChildView(std::make_unique<EventCountView>());

  contents_view->SetBoundsRect(gfx::Rect(0, 0, 10, 10));
  v1->SetBoundsRect(gfx::Rect(0, 0, 10, 10));

  ui::MouseEvent pressed_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                               gfx::Point(5, 5), ui::EventTimeForNow(), 0, 0);
  ui::MouseEvent released_event(ui::EventType::kMouseReleased, gfx::Point(5, 5),
                                gfx::Point(5, 5), ui::EventTimeForNow(), 0, 0);

  // First click handled, second click unhandled.
  v1->set_handle_mode(EventCountView::CONSUME_EVENTS);
  pressed_event.SetClickCount(1);
  released_event.SetClickCount(1);
  EXPECT_TRUE(root_view->OnMousePressed(pressed_event));
  root_view->OnMouseReleased(released_event);
  v1->set_handle_mode(EventCountView::PROPAGATE_EVENTS);
  pressed_event.SetClickCount(2);
  released_event.SetClickCount(2);
  EXPECT_TRUE(root_view->OnMousePressed(pressed_event));
  root_view->OnMouseReleased(released_event);

  // Both clicks unhandled.
  v1->set_handle_mode(EventCountView::PROPAGATE_EVENTS);
  pressed_event.SetClickCount(1);
  released_event.SetClickCount(1);
  EXPECT_FALSE(root_view->OnMousePressed(pressed_event));
  root_view->OnMouseReleased(released_event);
  pressed_event.SetClickCount(2);
  released_event.SetClickCount(2);
  EXPECT_FALSE(root_view->OnMousePressed(pressed_event));
  root_view->OnMouseReleased(released_event);
}

TEST_F(RootViewTest, AccessibleProperties) {
  RootViewTestState state(this);
  internal::RootView* root_view = state.GetRootView();

  ui::AXNodeData data;
  root_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kWindow);
}

}  // namespace views::test
