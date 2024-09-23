// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/test/ui_controls.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"

namespace views::test {

// Tests for NativeWidgetMac that rely on global window manager state, and can
// not be parallelized.
class NativeWidgetMacInteractiveUITest
    : public WidgetTest,
      public ::testing::WithParamInterface<bool> {
 public:
  class Observer;

  NativeWidgetMacInteractiveUITest() = default;

  NativeWidgetMacInteractiveUITest(const NativeWidgetMacInteractiveUITest&) =
      delete;
  NativeWidgetMacInteractiveUITest& operator=(
      const NativeWidgetMacInteractiveUITest&) = delete;

  // WidgetTest:
  void SetUp() override {
    SetUpForInteractiveTests();
    WidgetTest::SetUp();
  }

  Widget* MakeWidget() {
    return GetParam() ? CreateTopLevelFramelessPlatformWidget()
                      : CreateTopLevelPlatformWidget();
  }

 protected:
  std::unique_ptr<Observer> observer_;
  int activation_count_ = 0;
  int deactivation_count_ = 0;
};

class NativeWidgetMacInteractiveUITest::Observer : public TestWidgetObserver {
 public:
  Observer(NativeWidgetMacInteractiveUITest* parent, Widget* widget)
      : TestWidgetObserver(widget), parent_(parent) {}

  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active)
      parent_->activation_count_++;
    else
      parent_->deactivation_count_++;
  }

 private:
  raw_ptr<NativeWidgetMacInteractiveUITest> parent_;
};

// Test that showing a window causes it to attain global keyWindow status.
TEST_P(NativeWidgetMacInteractiveUITest, ShowAttainsKeyStatus) {
  Widget* widget = MakeWidget();
  observer_ = std::make_unique<Observer>(this, widget);

  EXPECT_FALSE(widget->IsActive());
  EXPECT_EQ(0, activation_count_);
  {
    widget->Show();
    WaitForWidgetActive(widget, true);
  }
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(widget->GetNativeWindow().GetNativeNSWindow().keyWindow);
  EXPECT_EQ(1, activation_count_);
  EXPECT_EQ(0, deactivation_count_);

  // Now check that losing and gaining key status due events outside of Widget
  // works correctly.
  Widget* widget2 = MakeWidget();  // Note: not observed.
  EXPECT_EQ(0, deactivation_count_);
  {
    widget2->Show();
    WaitForWidgetActive(widget2, true);
  }
  EXPECT_EQ(1, deactivation_count_);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_EQ(1, activation_count_);

  {
    [widget->GetNativeWindow().GetNativeNSWindow() makeKeyAndOrderFront:nil];
    WaitForWidgetActive(widget, true);
  }
  EXPECT_TRUE(widget->IsActive());
  EXPECT_EQ(1, deactivation_count_);
  EXPECT_EQ(2, activation_count_);

  widget2->CloseNow();
  widget->CloseNow();

  EXPECT_EQ(1, deactivation_count_);
  EXPECT_EQ(2, activation_count_);
}

// Test that ShowInactive does not take keyWindow status.
TEST_P(NativeWidgetMacInteractiveUITest, ShowInactiveIgnoresKeyStatus) {
  WidgetTest::WaitForSystemAppActivation();
  Widget* widget = MakeWidget();
  NSWindow* widget_window = widget->GetNativeWindow().GetNativeNSWindow();

  WindowedNSNotificationObserver* waiter =
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidBecomeKeyNotification
                       object:widget_window];

  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(widget_window.visible);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget_window.keyWindow);
  widget->ShowInactive();

  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(widget_window.visible);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget_window.keyWindow);

  // If the window were to become active, this would activate it.
  RunPendingMessages();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE(widget_window.keyWindow);
  EXPECT_EQ(0, waiter.notificationCount);

  // Activating the inactive widget should make it key, asynchronously.
  widget->Activate();
  [waiter wait];
  EXPECT_EQ(1, waiter.notificationCount);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE(widget_window.keyWindow);

  widget->CloseNow();
}

namespace {

// Show |widget| and wait for it to become the key window.
void ShowKeyWindow(Widget* widget) {
  NSWindow* widget_window = widget->GetNativeWindow().GetNativeNSWindow();
  WindowedNSNotificationObserver* waiter =
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidBecomeKeyNotification
                       object:widget_window];
  widget->Show();
  EXPECT_TRUE([waiter wait]);
  EXPECT_TRUE(widget_window.keyWindow);
}

NSData* ViewAsTIFF(NSView* view) {
  NSBitmapImageRep* bitmap =
      [view bitmapImageRepForCachingDisplayInRect:view.bounds];
  [view cacheDisplayInRect:view.bounds toBitmapImageRep:bitmap];
  return [bitmap TIFFRepresentation];
}

class TestBubbleView : public BubbleDialogDelegateView {
 public:
  explicit TestBubbleView(Widget* parent) {
    SetAnchorView(parent->GetContentsView());
  }

  TestBubbleView(const TestBubbleView&) = delete;
  TestBubbleView& operator=(const TestBubbleView&) = delete;
};

}  // namespace

// Test that parent windows keep their traffic lights enabled when showing
// dialogs.
TEST_F(NativeWidgetMacInteractiveUITest, ParentWindowTrafficLights) {
  Widget* parent_widget = CreateTopLevelPlatformWidget();
  parent_widget->SetBounds(gfx::Rect(100, 100, 100, 100));
  ShowKeyWindow(parent_widget);

  NSWindow* parent = parent_widget->GetNativeWindow().GetNativeNSWindow();
  EXPECT_TRUE(parent.mainWindow);

  NSButton* button = [parent standardWindowButton:NSWindowCloseButton];
  EXPECT_TRUE(button);
  NSData* active_button_image = ViewAsTIFF(button);
  EXPECT_TRUE(active_button_image);
  EXPECT_TRUE(parent_widget->ShouldPaintAsActive());

  // If a child widget is key, the parent should paint as active.
  Widget* child_widget = new Widget;
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = parent_widget->GetNativeView();
  child_widget->Init(std::move(params));
  child_widget->SetContentsView(new View);
  child_widget->Show();
  NSWindow* child = child_widget->GetNativeWindow().GetNativeNSWindow();

  // Ensure the button instance is still valid.
  EXPECT_EQ(button, [parent standardWindowButton:NSWindowCloseButton]);
  EXPECT_TRUE(parent_widget->ShouldPaintAsActive());

  // Parent window should still be main, and have its traffic lights active.
  EXPECT_TRUE(parent.mainWindow);
  EXPECT_FALSE(parent.keyWindow);

  // Enabled status doesn't actually change, but check anyway.
  EXPECT_TRUE(button.enabled);
  NSData* button_image_with_child = ViewAsTIFF(button);
  EXPECT_TRUE([active_button_image isEqualToData:button_image_with_child]);

  // Verify that activating some other random window does change the button.
  // When the bubble loses activation, it will dismiss itself and update
  // Widget::ShouldPaintAsActive().
  Widget* other_widget = CreateTopLevelPlatformWidget();
  other_widget->SetBounds(gfx::Rect(200, 200, 100, 100));
  ShowKeyWindow(other_widget);
  EXPECT_FALSE(parent.mainWindow);
  EXPECT_FALSE(parent.keyWindow);
  EXPECT_FALSE(parent_widget->ShouldPaintAsActive());
  EXPECT_TRUE(button.enabled);
  NSData* inactive_button_image = ViewAsTIFF(button);
  EXPECT_FALSE([active_button_image isEqualToData:inactive_button_image]);

  // Focus the child again and assert the parent once again paints as active.
  [child makeKeyWindow];
  EXPECT_TRUE(parent_widget->ShouldPaintAsActive());
  EXPECT_TRUE(child.keyWindow);
  EXPECT_FALSE(parent.keyWindow);

  child_widget->CloseNow();
  other_widget->CloseNow();
  parent_widget->CloseNow();
}

// Test activation of a window that has restoration data that was restored to
// the dock. See crbug.com/1205683 .
TEST_F(NativeWidgetMacInteractiveUITest,
       DeminiaturizeWindowWithRestorationData) {
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.native_widget =
      CreatePlatformNativeWidgetImpl(widget, kStubCapture, nullptr);
  // Start the window off in the dock.
  params.show_state = ui::mojom::WindowShowState::kMinimized;
  // "{}" in base64encode, to create some dummy restoration data.
  const std::string kDummyWindowRestorationData = "e30=";
  params.workspace = kDummyWindowRestorationData;
  widget->Init(std::move(params));

  // Wait for the window to minimize. Ultimately we're going to check the
  // NSWindow minimization state, so it would make sense to wait on the
  // notification as we do below. However,
  // widget->GetNativeWindow().GetNativeNSWindow() returns nil before the call
  // to widget->Init(), and we'd need to set up the notification observer at
  // that point. So instead, wait on the Widget state change.
  {
    views::test::PropertyWaiter minimize_waiter(
        base::BindRepeating(&Widget::IsMinimized, base::Unretained(widget)),
        true);
    EXPECT_TRUE(minimize_waiter.Wait());
  }

  NSWindow* window = widget->GetNativeWindow().GetNativeNSWindow();
  EXPECT_TRUE(window.miniaturized);

  // As part of the window restoration process,
  // SessionRestoreImpl::ShowBrowser() -> BrowserView::Show() ->
  // views::Widget::Show() -> views::NativeWidgetMac::Show() which calls
  // SetVisibilityState(), the code path we want to test. Even though the method
  // name is Show(), it "shows" the saved_show_state_ which in this case is
  // WindowVisibilityState::kHideWindow.
  widget->Show();
  EXPECT_TRUE(window.miniaturized);

  // Activate the window from the dock (i.e.
  // SetVisibilityState(WindowVisibilityState::kShowAndActivateWindow)).
  WindowedNSNotificationObserver* deminiaturizationObserver =
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidDeminiaturizeNotification
                       object:window];
  widget->Activate();
  [deminiaturizationObserver wait];
  EXPECT_FALSE(window.miniaturized);

  widget->CloseNow();
}

// Test that bubble widgets are dismissed on right mouse down.
TEST_F(NativeWidgetMacInteractiveUITest, BubbleDismiss) {
  Widget* parent_widget = CreateTopLevelPlatformWidget();
  parent_widget->SetBounds(gfx::Rect(100, 100, 100, 100));
  ShowKeyWindow(parent_widget);

  Widget* bubble_widget =
      BubbleDialogDelegateView::CreateBubble(new TestBubbleView(parent_widget));
  ShowKeyWindow(bubble_widget);

  // First, test with LeftMouseDown in the parent window.
  NSEvent* mouse_down = cocoa_test_event_utils::LeftMouseDownAtPointInWindow(
      NSMakePoint(50, 50),
      parent_widget->GetNativeWindow().GetNativeNSWindow());
  [NSApp sendEvent:mouse_down];
  EXPECT_TRUE(bubble_widget->IsClosed());

  bubble_widget =
      BubbleDialogDelegateView::CreateBubble(new TestBubbleView(parent_widget));
  ShowKeyWindow(bubble_widget);

  // Test with RightMouseDown in the parent window.
  mouse_down = cocoa_test_event_utils::RightMouseDownAtPointInWindow(
      NSMakePoint(50, 50),
      parent_widget->GetNativeWindow().GetNativeNSWindow());
  [NSApp sendEvent:mouse_down];
  EXPECT_TRUE(bubble_widget->IsClosed());

  bubble_widget =
      BubbleDialogDelegateView::CreateBubble(new TestBubbleView(parent_widget));
  ShowKeyWindow(bubble_widget);

  // Test with RightMouseDown in the bubble (bubble should stay open).
  mouse_down = cocoa_test_event_utils::RightMouseDownAtPointInWindow(
      NSMakePoint(50, 50),
      bubble_widget->GetNativeWindow().GetNativeNSWindow());
  [NSApp sendEvent:mouse_down];
  EXPECT_FALSE(bubble_widget->IsClosed());
  bubble_widget->CloseNow();

  // Test with RightMouseDown when set_close_on_deactivate(false).
  TestBubbleView* bubble_view = new TestBubbleView(parent_widget);
  bubble_view->set_close_on_deactivate(false);
  bubble_widget = BubbleDialogDelegateView::CreateBubble(bubble_view);
  ShowKeyWindow(bubble_widget);

  mouse_down = cocoa_test_event_utils::RightMouseDownAtPointInWindow(
      NSMakePoint(50, 50),
      parent_widget->GetNativeWindow().GetNativeNSWindow());
  [NSApp sendEvent:mouse_down];
  EXPECT_FALSE(bubble_widget->IsClosed());

  parent_widget->CloseNow();
}

// Ensure BridgedContentView's inputContext can handle its window being torn
// away mid-way through event processing. Toolkit-views guarantees to move focus
// away from any Widget when the window is torn down. This test ensures that
// global references AppKit may have held on to are also updated.
TEST_F(NativeWidgetMacInteractiveUITest, GlobalNSTextInputContextUpdates) {
  Widget* widget = CreateTopLevelNativeWidget();
  Textfield* textfield = new Textfield;
  textfield->SetBounds(0, 0, 100, 100);
  widget->GetContentsView()->AddChildView(textfield);
  textfield->RequestFocus();
  {
    widget->Show();
    WaitForWidgetActive(widget, true);
  }
  EXPECT_TRUE(widget->GetNativeView().GetNativeNSView().inputContext);
  EXPECT_EQ(widget->GetNativeView().GetNativeNSView().inputContext,
            NSTextInputContext.currentInputContext);

  widget->GetContentsView()->RemoveChildView(textfield);

  // NSTextInputContext usually only updates at the end of an AppKit event loop
  // iteration. We just tore out the inputContext, so ensure the raw, weak
  // global pointer that AppKit likes to keep around has been updated manually.
  EXPECT_EQ(nil, NSTextInputContext.currentInputContext);
  EXPECT_FALSE(widget->GetNativeView().GetNativeNSView().inputContext);

  // RemoveChildView() doesn't delete the view.
  delete textfield;

  widget->Close();
  base::RunLoop().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(NativeWidgetMacInteractiveUITestInstance,
                         NativeWidgetMacInteractiveUITest,
                         ::testing::Bool());

}  // namespace views::test
