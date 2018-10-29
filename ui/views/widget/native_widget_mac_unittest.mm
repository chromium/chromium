// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#import "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"
#import "ui/base/cocoa/window_size_constants.h"
#import "ui/base/test/scoped_fake_full_keyboard_access.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/events/test/event_generator.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/cocoa/bridged_native_widget_host_impl.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/native_cursor.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"
#import "ui/views_bridge_mac/bridged_content_view.h"
#import "ui/views_bridge_mac/bridged_native_widget_impl.h"
#import "ui/views_bridge_mac/native_widget_mac_nswindow.h"

// Donates an implementation of -[NSAnimation stopAnimation] which calls the
// original implementation, then quits a nested run loop.
@interface TestStopAnimationWaiter : NSObject
@end

@interface ConstrainedWindowAnimationBase (TestingAPI)
- (void)setWindowStateForEnd;
@end

@interface NSWindow (PrivateAPI)
- (BOOL)_isTitleHidden;
@end

// Test NSWindow that provides hooks via method overrides to verify behavior.
@interface NativeWidgetMacTestWindow : NativeWidgetMacNSWindow {
 @private
  int invalidateShadowCount_;
  int orderWindowCount_;
  bool* deallocFlag_;
}
@property(readonly, nonatomic) int invalidateShadowCount;
@property(readonly, nonatomic) int orderWindowCount;
@property(assign, nonatomic) bool* deallocFlag;
@end

// Used to mock BridgedContentView so that calls to drawRect: can be
// intercepted.
@interface MockBridgedView : NSView {
 @private
  // Number of times -[NSView drawRect:] has been called.
  NSUInteger drawRectCount_;

  // The dirtyRect parameter passed to last invocation of drawRect:.
  NSRect lastDirtyRect_;
}

@property(assign, nonatomic) NSUInteger drawRectCount;
@property(assign, nonatomic) NSRect lastDirtyRect;
@end

@interface FocusableTestNSView : NSView
@end

namespace views {
namespace test {

// BridgedNativeWidgetImpl friend to access private members.
class BridgedNativeWidgetTestApi {
 public:
  explicit BridgedNativeWidgetTestApi(NSWindow* window) {
    bridge_ =
        BridgedNativeWidgetHostImpl::GetFromNativeWindow(window)->bridge_impl();
  }

  // Simulate a frame swap from the compositor.
  void SimulateFrameSwap(const gfx::Size& size) {
    const float kScaleFactor = 1.0f;
    gfx::CALayerParams ca_layer_params;
    ca_layer_params.is_empty = false;
    ca_layer_params.pixel_size = size;
    ca_layer_params.scale_factor = kScaleFactor;
    bridge_->SetCALayerParams(ca_layer_params);
  }

  NSAnimation* show_animation() {
    return base::mac::ObjCCastStrict<NSAnimation>(
        bridge_->show_animation_.get());
  }

 private:
  BridgedNativeWidgetImpl* bridge_;

  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetTestApi);
};

// Custom native_widget to create a NativeWidgetMacTestWindow.
class TestWindowNativeWidgetMac : public NativeWidgetMac {
 public:
  explicit TestWindowNativeWidgetMac(Widget* delegate)
      : NativeWidgetMac(delegate) {}

 protected:
  // NativeWidgetMac:
  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      views_bridge_mac::mojom::CreateWindowParams* params) override {
    params->style_mask = NSBorderlessWindowMask;
    if (widget_params.type == Widget::InitParams::TYPE_WINDOW) {
      params->style_mask = NSTexturedBackgroundWindowMask | NSTitledWindowMask |
                           NSClosableWindowMask | NSMiniaturizableWindowMask |
                           NSResizableWindowMask;
    }
  }
  NativeWidgetMacNSWindow* CreateNSWindow(
      const views_bridge_mac::mojom::CreateWindowParams* params) override {
    return [[[NativeWidgetMacTestWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:params->style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO] autorelease];
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowNativeWidgetMac);
};

// Tests for parts of NativeWidgetMac not covered by BridgedNativeWidgetImpl,
// which need access to Cocoa APIs.
class NativeWidgetMacTest : public WidgetTest {
 public:
  NativeWidgetMacTest() {}

  // Make an NSWindow with a close button and a title bar to use as a parent.
  // This NSWindow is backed by a widget that is not exposed to the caller.
  // To destroy the Widget, the native NSWindow must be closed.
  NativeWidgetMacTestWindow* MakeClosableTitledNativeParent() {
    NativeWidgetMacTestWindow* native_parent = nil;
    Widget::InitParams parent_init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    parent_init_params.bounds = gfx::Rect(100, 100, 200, 200);
    CreateWidgetWithTestWindow(parent_init_params, &native_parent);
    return native_parent;
  }

  // Same as the above, but creates a borderless NSWindow.
  NativeWidgetMacTestWindow* MakeBorderlessNativeParent() {
    NativeWidgetMacTestWindow* native_parent = nil;
    Widget::InitParams parent_init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    parent_init_params.remove_standard_frame = true;
    parent_init_params.bounds = gfx::Rect(100, 100, 200, 200);
    CreateWidgetWithTestWindow(parent_init_params, &native_parent);
    return native_parent;
  }

  // Create a Widget backed by the NativeWidgetMacTestWindow NSWindow subclass.
  Widget* CreateWidgetWithTestWindow(Widget::InitParams params,
                                     NativeWidgetMacTestWindow** window) {
    Widget* widget = new Widget;
    params.native_widget = new TestWindowNativeWidgetMac(widget);
    widget->Init(params);
    widget->Show();
    *window = base::mac::ObjCCastStrict<NativeWidgetMacTestWindow>(
        widget->GetNativeWindow().GetNativeNSWindow());
    EXPECT_TRUE(*window);
    return widget;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacTest);
};

class WidgetChangeObserver : public TestWidgetObserver {
 public:
  WidgetChangeObserver(Widget* widget) : TestWidgetObserver(widget) {}

  void WaitForVisibleCounts(int gained, int lost) {
    if (gained_visible_count_ >= gained && lost_visible_count_ >= lost)
      return;

    target_gained_visible_count_ = gained;
    target_lost_visible_count_ = lost;

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
    run_loop_ = nullptr;
  }

  int gained_visible_count() const { return gained_visible_count_; }
  int lost_visible_count() const { return lost_visible_count_; }

 private:
  // WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget,
                                 bool visible) override {
    ++(visible ? gained_visible_count_ : lost_visible_count_);
    if (run_loop_ && gained_visible_count_ >= target_gained_visible_count_ &&
        lost_visible_count_ >= target_lost_visible_count_)
      run_loop_->Quit();
  }

  int gained_visible_count_ = 0;
  int lost_visible_count_ = 0;
  int target_gained_visible_count_ = 0;
  int target_lost_visible_count_ = 0;
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WidgetChangeObserver);
};

class NativeHostHolder {
 public:
  NativeHostHolder()
      : view_([[NSView alloc] init]), host_(new NativeViewHost()) {
    host_->set_owned_by_client();
  }

  void AttachNativeView() {
    DCHECK(!host_->native_view());
    host_->Attach(view_.get());
  }

  void Detach() { host_->Detach(); }

  NSView* view() const { return view_.get(); }
  NativeViewHost* host() const { return host_.get(); }

 private:
  base::scoped_nsobject<NSView> view_;
  std::unique_ptr<NativeViewHost> host_;

  DISALLOW_COPY_AND_ASSIGN(NativeHostHolder);
};

// This class gives public access to the protected ctor of
// BubbleDialogDelegateView.
class SimpleBubbleView : public BubbleDialogDelegateView {
 public:
  SimpleBubbleView() {}
  ~SimpleBubbleView() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleBubbleView);
};

class CustomTooltipView : public View {
 public:
  CustomTooltipView(const base::string16& tooltip, View* tooltip_handler)
      : tooltip_(tooltip), tooltip_handler_(tooltip_handler) {}

  // View:
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override {
    *tooltip = tooltip_;
    return true;
  }

  View* GetTooltipHandlerForPoint(const gfx::Point& point) override {
    return tooltip_handler_ ? tooltip_handler_ : this;
  }

 private:
  base::string16 tooltip_;
  View* tooltip_handler_;  // Weak

  DISALLOW_COPY_AND_ASSIGN(CustomTooltipView);
};

// Test visibility states triggered externally.
TEST_F(NativeWidgetMacTest, HideAndShowExternally) {
  Widget* widget = CreateTopLevelPlatformWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  WidgetChangeObserver observer(widget);

  // Should initially be hidden.
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(0, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());

  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());

  widget->Hide();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());

  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());

  // Test when hiding individual windows.
  [ns_window orderOut:nil];
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());

  [ns_window orderFront:nil];
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());

  // Test when hiding the entire application. This doesn't send an orderOut:
  // to the NSWindow.
  [NSApp hide:nil];
  // When the activation policy is NSApplicationActivationPolicyRegular, the
  // calls via NSApp are asynchronous, and the run loop needs to be flushed.
  // With NSApplicationActivationPolicyProhibited, the following
  // WaitForVisibleCounts calls are superfluous, but don't hurt.
  observer.WaitForVisibleCounts(3, 3);
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  [NSApp unhideWithoutActivation];
  observer.WaitForVisibleCounts(4, 3);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(4, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  // Hide again to test unhiding with an activation.
  [NSApp hide:nil];
  observer.WaitForVisibleCounts(4, 4);
  EXPECT_EQ(4, observer.lost_visible_count());
  [NSApp unhide:nil];
  observer.WaitForVisibleCounts(5, 4);
  EXPECT_EQ(5, observer.gained_visible_count());

  // Hide again to test makeKeyAndOrderFront:.
  [ns_window orderOut:nil];
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(5, observer.gained_visible_count());
  EXPECT_EQ(5, observer.lost_visible_count());

  [ns_window makeKeyAndOrderFront:nil];
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(6, observer.gained_visible_count());
  EXPECT_EQ(5, observer.lost_visible_count());

  // No change when closing.
  widget->CloseNow();
  EXPECT_EQ(5, observer.lost_visible_count());
  EXPECT_EQ(6, observer.gained_visible_count());
}

// A view that counts calls to OnPaint().
class PaintCountView : public View {
 public:
  PaintCountView() { SetBounds(0, 0, 100, 100); }

  // View:
  void OnPaint(gfx::Canvas* canvas) override {
    EXPECT_TRUE(GetWidget()->IsVisible());
    ++paint_count_;
    if (run_loop_ && paint_count_ == target_paint_count_)
      run_loop_->Quit();
  }

  void WaitForPaintCount(int target) {
    if (paint_count_ == target)
      return;

    target_paint_count_ = target;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  int paint_count() { return paint_count_; }

 private:
  int paint_count_ = 0;
  int target_paint_count_ = 0;
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PaintCountView);
};

// Test for correct child window restore when parent window is minimized
// and restored using -makeKeyAndOrderFront:.
// Parent-child window relationships in AppKit are not supported when window
// visibility changes.
// Disabled because it relies on cocoa occlusion APIs
// and state changes that are unavoidably flaky.
TEST_F(NativeWidgetMacTest, DISABLED_OrderFrontAfterMiniaturize) {
  Widget* widget = CreateTopLevelPlatformWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();

  Widget* child_widget = CreateChildPlatformWidget(widget->GetNativeView());
  NSWindow* child_ns_window =
      child_widget->GetNativeWindow().GetNativeNSWindow();

  // Set parent bounds that overlap child.
  widget->SetBounds(gfx::Rect(100, 100, 300, 300));
  child_widget->SetBounds(gfx::Rect(110, 110, 100, 100));

  widget->Show();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget->IsMinimized());

  // Minimize parent.
  [ns_window performMiniaturize:nil];
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(child_widget->IsVisible());

  // Restore parent window as AppController does.
  [ns_window makeKeyAndOrderFront:nil];

  // Wait and check that child is really visible.
  // TODO(kirr): remove the fixed delay.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated(),
      base::TimeDelta::FromSeconds(2));
  base::RunLoop().Run();

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(child_widget->IsVisible());
  // Check that child window is visible.
  EXPECT_TRUE([child_ns_window occlusionState] & NSWindowOcclusionStateVisible);
  EXPECT_TRUE(IsWindowStackedAbove(child_widget, widget));
  widget->Close();
}

// Test that ShowInactive() on already-visible child widgets is ignored, since
// it may cause a space transition. See https://crbug.com/866760.
TEST_F(NativeWidgetMacTest, ShowInactiveOnChildWidget) {
  NativeWidgetMacTestWindow* parent_window;
  NativeWidgetMacTestWindow* child_window;

  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(100, 100, 200, 200);
  Widget* parent = CreateWidgetWithTestWindow(init_params, &parent_window);

  // CreateWidgetWithTestWindow calls Show()
  EXPECT_EQ(1, [parent_window orderWindowCount]);

  init_params.parent = parent->GetNativeView();
  Widget* child = CreateWidgetWithTestWindow(init_params, &child_window);

  // The child is ordered twice, once by Show() and again (by AppKit) when it is
  // registered as a child window.
  EXPECT_EQ(2, [child_window orderWindowCount]);

  // Parent is unchanged.
  EXPECT_EQ(1, [parent_window orderWindowCount]);

  // ShowInactive() on a visible regular window may serve to raise its stacking
  // order without taking focus, so it should invoke -[NSWindow orderWindow:..].
  parent->ShowInactive();
  EXPECT_EQ(2, [parent_window orderWindowCount]);  // Increases.

  // However, ShowInactive() on the child should have no effect. It should
  // already be in a correct stacking order and we must avoid a Space switch.
  child->ShowInactive();
  EXPECT_EQ(2, [child_window orderWindowCount]);   // No change.
  EXPECT_EQ(2, [parent_window orderWindowCount]);  // Parent also unchanged.

  parent->CloseNow();
}

// Test minimized states triggered externally, implied visibility and restored
// bounds whilst minimized.
TEST_F(NativeWidgetMacTest, MiniaturizeExternally) {
  if (base::mac::IsOS10_10())
    return;  // Fails when swarmed. http://crbug.com/660582
  Widget* widget = new Widget;
  Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW);
  widget->Init(init_params);

  PaintCountView* view = new PaintCountView();
  widget->GetContentsView()->AddChildView(view);
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  WidgetChangeObserver observer(widget);

  widget->SetBounds(gfx::Rect(100, 100, 300, 300));

  EXPECT_TRUE(view->IsDrawn());
  EXPECT_EQ(0, view->paint_count());
  widget->Show();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());
  const gfx::Rect restored_bounds = widget->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());

  // Showing should paint.
  view->WaitForPaintCount(1);

  // First try performMiniaturize:, which requires a minimize button. Note that
  // Cocoa just blocks the UI thread during the animation, so no need to do
  // anything fancy to wait for it finish.
  [ns_window performMiniaturize:nil];
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsVisible());  // Minimizing also makes things invisible.
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());

  // No repaint when minimizing. But note that this is partly due to not calling
  // [NSView setNeedsDisplay:YES] on the content view. The superview, which is
  // an NSThemeFrame, would repaint |view| if we had, because the miniaturize
  // button is highlighted for performMiniaturize.
  EXPECT_EQ(1, view->paint_count());

  [ns_window deminiaturize:nil];
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());

  view->WaitForPaintCount(2);  // A single paint when deminiaturizing.
  EXPECT_FALSE([ns_window isMiniaturized]);

  widget->Minimize();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_TRUE([ns_window isMiniaturized]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(2, view->paint_count());  // No paint when miniaturizing.

  widget->Restore();  // If miniaturized, should deminiaturize.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  view->WaitForPaintCount(3);

  widget->Restore();  // If not miniaturized, does nothing.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(3, view->paint_count());

  widget->CloseNow();

  // Create a widget without a minimize button.
  widget = CreateTopLevelFramelessPlatformWidget();
  ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  widget->SetBounds(gfx::Rect(100, 100, 300, 300));
  widget->Show();
  EXPECT_FALSE(widget->IsMinimized());

  // This should fail, since performMiniaturize: requires a minimize button.
  [ns_window performMiniaturize:nil];
  EXPECT_FALSE(widget->IsMinimized());

  // But this should work.
  widget->Minimize();
  EXPECT_TRUE(widget->IsMinimized());

  // Test closing while minimized.
  widget->CloseNow();
}

// Simple view for the SetCursor test that overrides View::GetCursor().
class CursorView : public View {
 public:
  CursorView(int x, NSCursor* cursor) : cursor_(cursor) {
    SetBounds(x, 0, 100, 300);
  }

  // View:
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override {
    return cursor_;
  }

 private:
  NSCursor* cursor_;

  DISALLOW_COPY_AND_ASSIGN(CursorView);
};

// Test for Widget::SetCursor(). There is no Widget::GetCursor(), so this uses
// -[NSCursor currentCursor] to validate expectations. Note that currentCursor
// is just "the top cursor on the application's cursor stack.", which is why it
// is safe to use this in a non-interactive UI test with the EventGenerator.
TEST_F(NativeWidgetMacTest, SetCursor) {
  NSCursor* arrow = [NSCursor arrowCursor];
  NSCursor* hand = GetNativeHandCursor();
  NSCursor* ibeam = GetNativeIBeamCursor();

  Widget* widget = CreateTopLevelPlatformWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));
  widget->GetContentsView()->AddChildView(new CursorView(0, hand));
  widget->GetContentsView()->AddChildView(new CursorView(100, ibeam));
  widget->Show();
  NSWindow* widget_window = widget->GetNativeWindow().GetNativeNSWindow();

  // Events used to simulate tracking rectangle updates. These are not passed to
  // toolkit-views, so it only matters whether they are inside or outside the
  // content area.
  NSEvent* event_in_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(100, 100), NSMouseMoved, 0);
  NSEvent* event_out_of_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(-50, -50), NSMouseMoved, 0);

  EXPECT_NE(arrow, hand);
  EXPECT_NE(arrow, ibeam);

  // Make arrow the current cursor.
  [arrow set];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  // Use an event generator to ask views code to set the cursor. However, note
  // that this does not cause Cocoa to generate tracking rectangle updates.
  ui::test::EventGenerator event_generator(GetContext(), widget_window);

  // Move the mouse over the first view, then simulate a tracking rectangle
  // update. Verify that the cursor changed from arrow to hand type.
  event_generator.MoveMouseTo(gfx::Point(50, 50));
  [widget_window cursorUpdate:event_in_content];
  EXPECT_EQ(hand, [NSCursor currentCursor]);

  // A tracking rectangle update not in the content area should forward to
  // the native NSWindow implementation, which sets the arrow cursor.
  [widget_window cursorUpdate:event_out_of_content];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  // Now move to the second view.
  event_generator.MoveMouseTo(gfx::Point(150, 50));
  [widget_window cursorUpdate:event_in_content];
  EXPECT_EQ(ibeam, [NSCursor currentCursor]);

  // Moving to the third view (but remaining in the content area) should also
  // forward to the native NSWindow implementation.
  event_generator.MoveMouseTo(gfx::Point(250, 50));
  [widget_window cursorUpdate:event_in_content];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  widget->CloseNow();
}

// Tests that an accessibility request from the system makes its way through to
// a views::Label filling the window.
TEST_F(NativeWidgetMacTest, AccessibilityIntegration) {
  Widget* widget = CreateTopLevelPlatformWidget();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  const base::string16 test_string = base::ASCIIToUTF16("Green");
  views::Label* label = new views::Label(test_string);
  label->SetBounds(0, 0, 100, 100);
  widget->GetContentsView()->AddChildView(label);
  widget->Show();

  // Accessibility hit tests come in Cocoa screen coordinates.
  NSRect nsrect = gfx::ScreenRectToNSRect(screen_rect);
  NSPoint midpoint = NSMakePoint(NSMidX(nsrect), NSMidY(nsrect));

  id hit = [widget->GetNativeWindow().GetNativeNSWindow()
      accessibilityHitTest:midpoint];
  id title = [hit accessibilityAttributeValue:NSAccessibilityValueAttribute];
  EXPECT_NSEQ(title, @"Green");

  widget->CloseNow();
}

namespace {

Widget* AttachPopupToNativeParent(NSWindow* native_parent) {
  base::scoped_nsobject<NSView> anchor_view(
      [[NSView alloc] initWithFrame:[[native_parent contentView] bounds]]);
  [[native_parent contentView] addSubview:anchor_view];

  // Note: Don't use WidgetTest::CreateChildPlatformWidget because that makes
  // windows of TYPE_CONTROL which need a parent Widget to obtain the focus
  // manager.
  Widget* child = new Widget;
  Widget::InitParams init_params;
  init_params.parent = anchor_view.get();
  init_params.type = Widget::InitParams::TYPE_POPUP;
  child->Init(init_params);
  return child;
}

}  // namespace

// Tests creating a views::Widget parented off a native NSWindow.
TEST_F(NativeWidgetMacTest, NonWidgetParent) {
  NSWindow* native_parent = MakeBorderlessNativeParent();

  Widget::Widgets children;
  Widget::GetAllChildWidgets([native_parent contentView], &children);
  EXPECT_EQ(1u, children.size());

  Widget* child = AttachPopupToNativeParent(native_parent);
  TestWidgetObserver child_observer(child);

  // GetTopLevelNativeWidget() will go up through |native_parent|'s Widget.
  internal::NativeWidgetPrivate* top_level_widget =
      internal::NativeWidgetPrivate::GetTopLevelNativeWidget(
          child->GetNativeView());
  EXPECT_EQ(Widget::GetWidgetForNativeWindow(native_parent),
            top_level_widget->GetWidget());
  EXPECT_NE(child, top_level_widget->GetWidget());

  // To verify the parent, we need to use NativeWidgetMac APIs.
  BridgedNativeWidgetHostImpl* bridged_native_widget_host =
      BridgedNativeWidgetHostImpl::GetFromNativeWindow(
          child->GetNativeWindow());
  EXPECT_EQ(bridged_native_widget_host->parent()
                ->native_widget_mac()
                ->GetNativeWindow(),
            native_parent);

  const gfx::Rect child_bounds(50, 50, 200, 100);
  child->SetBounds(child_bounds);
  EXPECT_FALSE(child->IsVisible());
  EXPECT_EQ(0u, [[native_parent childWindows] count]);

  child->Show();
  EXPECT_TRUE(child->IsVisible());
  EXPECT_EQ(1u, [[native_parent childWindows] count]);
  EXPECT_EQ(child->GetNativeWindow(), [native_parent childWindows][0]);
  EXPECT_EQ(native_parent,
            [child->GetNativeWindow().GetNativeNSWindow() parentWindow]);

  Widget::GetAllChildWidgets([native_parent contentView], &children);
  ASSERT_EQ(2u, children.size());
  EXPECT_EQ(1u, children.count(child));

  // Only non-toplevel Widgets are positioned relative to the parent, so the
  // bounds set above should be in screen coordinates.
  EXPECT_EQ(child_bounds, child->GetWindowBoundsInScreen());

  // Removing the anchor view from its view hierarchy is permitted. This should
  // not break the relationship between the two windows.
  NSView* anchor_view = [[native_parent contentView] subviews][0];
  EXPECT_TRUE(anchor_view);
  [anchor_view removeFromSuperview];
  EXPECT_EQ(bridged_native_widget_host->parent()
                ->native_widget_mac()
                ->GetNativeWindow(),
            native_parent);

  // Closing the parent should close and destroy the child.
  EXPECT_FALSE(child_observer.widget_closed());
  [native_parent close];
  EXPECT_TRUE(child_observer.widget_closed());

  EXPECT_EQ(0u, [[native_parent childWindows] count]);
  [native_parent close];
}

// Tests that CloseAllSecondaryWidgets behaves in various configurations.
TEST_F(NativeWidgetMacTest, CloseAllSecondaryWidgetsValidState) {
  NSWindow* last_window = nil;
  {
    // First verify the behavior of CloseAllSecondaryWidgets in the normal case,
    // and how [NSApp windows] changes in response to Widget closure.
    base::mac::ScopedNSAutoreleasePool pool;
    Widget* widget = CreateTopLevelPlatformWidget();
    widget->Show();
    TestWidgetObserver observer(widget);
    last_window = widget->GetNativeWindow().GetNativeNSWindow();
    EXPECT_TRUE([[NSApp windows] containsObject:last_window]);
    Widget::CloseAllSecondaryWidgets();
    EXPECT_TRUE(observer.widget_closed());
  }

  {
    // Calls to [NSApp windows] do autorelease, so ensure the pool empties.
    base::mac::ScopedNSAutoreleasePool pool;

    // [NSApp windows] updates inside dealloc, so the window should be gone.
    EXPECT_FALSE([[NSApp windows] containsObject:last_window]);
  }

  {
    // Repeat, but now retain a reference and close the window before
    // CloseAllSecondaryWidgets().
    base::mac::ScopedNSAutoreleasePool pool;
    Widget* widget = CreateTopLevelPlatformWidget();
    widget->Show();
    TestWidgetObserver observer(widget);
    last_window = [widget->GetNativeWindow().GetNativeNSWindow() retain];
    EXPECT_TRUE([[NSApp windows] containsObject:last_window]);
    widget->CloseNow();
    EXPECT_TRUE(observer.widget_closed());
  }
  {
    base::mac::ScopedNSAutoreleasePool pool;
    // Reference retained, so the window should still be present.
    EXPECT_TRUE([[NSApp windows] containsObject:last_window]);
  }

  {
    base::mac::ScopedNSAutoreleasePool pool;
    Widget::CloseAllSecondaryWidgets();
    [last_window release];
  }
  {
    base::mac::ScopedNSAutoreleasePool pool;
    EXPECT_FALSE([[NSApp windows] containsObject:last_window]);
  }

  // Repeat, with two Widgets. We can't control the order of window closure.
  // If the parent is closed first, it should tear down the child while
  // iterating over the windows. -[NSWindow close] will be sent to the child
  // twice, but that should be fine.
  Widget* parent = CreateTopLevelPlatformWidget();
  Widget* child = CreateChildPlatformWidget(parent->GetNativeView());
  parent->Show();
  child->Show();
  TestWidgetObserver parent_observer(parent);
  TestWidgetObserver child_observer(child);

  EXPECT_TRUE([[NSApp windows]
      containsObject:parent->GetNativeWindow().GetNativeNSWindow()]);
  EXPECT_TRUE([[NSApp windows]
      containsObject:child->GetNativeWindow().GetNativeNSWindow()]);
  Widget::CloseAllSecondaryWidgets();
  EXPECT_TRUE(parent_observer.widget_closed());
  EXPECT_TRUE(child_observer.widget_closed());
}

// Tests closing the last remaining NSWindow reference via -windowWillClose:.
// This is a regression test for http://crbug.com/616701.
TEST_F(NativeWidgetMacTest, NonWidgetParentLastReference) {
  bool child_dealloced = false;
  bool native_parent_dealloced = false;
  NativeWidgetMacTestWindow* native_parent = nil;
  {
    base::mac::ScopedNSAutoreleasePool pool;
    native_parent = MakeBorderlessNativeParent();
    [native_parent setDeallocFlag:&native_parent_dealloced];

    NativeWidgetMacTestWindow* window;
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_POPUP);
    init_params.parent = [native_parent contentView];
    init_params.bounds = gfx::Rect(0, 0, 100, 200);
    CreateWidgetWithTestWindow(init_params, &window);
    [window setDeallocFlag:&child_dealloced];
  }
  {
    // On 10.11, closing a weak reference on the parent window works, but older
    // versions of AppKit get upset if things are released inside -[NSWindow
    // close]. This test tries to establish a situation where the last reference
    // to the child window is released inside WidgetOwnerNSWindowAdapter::
    // OnWindowWillClose().
    base::mac::ScopedNSAutoreleasePool pool;
    [native_parent close];
    EXPECT_TRUE(child_dealloced);
  }
  EXPECT_TRUE(native_parent_dealloced);
}

// Tests visibility for child of native NSWindow, reshowing after -[NSApp hide].
// Occasionally flaky (maybe due to [NSApp hide]). See https://crbug.com/777247.
TEST_F(NativeWidgetMacTest, DISABLED_VisibleAfterNativeParentShow) {
  NSWindow* native_parent = MakeBorderlessNativeParent();
  Widget* child = AttachPopupToNativeParent(native_parent);
  child->Show();
  EXPECT_TRUE(child->IsVisible());

  WidgetChangeObserver child_observer(child);
  [NSApp hide:nil];
  child_observer.WaitForVisibleCounts(0, 1);
  EXPECT_FALSE(child->IsVisible());

  [native_parent makeKeyAndOrderFront:nil];
  child_observer.WaitForVisibleCounts(1, 1);
  EXPECT_TRUE(child->IsVisible());

  [native_parent close];
}

// Tests visibility for a child of a native NSWindow, reshowing after a
// deminiaturize on the parent window (after attempting to show the child while
// the parent was miniaturized).
TEST_F(NativeWidgetMacTest, VisibleAfterNativeParentDeminiaturize) {
  // Disabled on 10.10 due to flakes. See http://crbug.com/777247.
  if (base::mac::IsOS10_10())
    return;

  NSWindow* native_parent = MakeBorderlessNativeParent();
  [native_parent makeKeyAndOrderFront:nil];
  [native_parent miniaturize:nil];
  Widget* child = AttachPopupToNativeParent(native_parent);

  child->Show();
  EXPECT_FALSE([native_parent isVisible]);
  EXPECT_FALSE(child->IsVisible());  // Parent is hidden so child is also.

  [native_parent deminiaturize:nil];
  EXPECT_TRUE([native_parent isVisible]);
  // Don't WaitForVisibleCounts() here: deminiaturize is synchronous, so any
  // spurious _occlusion_ state change would have already occurred. Further
  // occlusion changes are not guaranteed to be triggered by the deminiaturize.
  EXPECT_TRUE(child->IsVisible());
  [native_parent close];
}

// Use Native APIs to query the tooltip text that would be shown once the
// tooltip delay had elapsed.
base::string16 TooltipTextForWidget(Widget* widget) {
  // For Mac, the actual location doesn't matter, since there is only one native
  // view and it fills the window. This just assumes the window is at least big
  // big enough for a constant coordinate to be within it.
  NSPoint point = NSMakePoint(30, 30);
  NSView* view = [widget->GetNativeView().GetNativeNSView() hitTest:point];
  NSString* text =
      [view view:view stringForToolTip:0 point:point userData:nullptr];
  return base::SysNSStringToUTF16(text);
}

// Tests tooltips. The test doesn't wait for tooltips to appear. That is, the
// test assumes Cocoa calls stringForToolTip: at appropriate times and that,
// when a tooltip is already visible, changing it causes an update. These were
// tested manually by inserting a base::RunLoop.Run().
TEST_F(NativeWidgetMacTest, Tooltips) {
  Widget* widget = CreateTopLevelPlatformWidget();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  const base::string16 tooltip_back = base::ASCIIToUTF16("Back");
  const base::string16 tooltip_front = base::ASCIIToUTF16("Front");
  const base::string16 long_tooltip(2000, 'W');

  // Create a nested layout to test corner cases.
  LabelButton* back = new LabelButton(nullptr, base::string16());
  back->SetBounds(10, 10, 80, 80);
  widget->GetContentsView()->AddChildView(back);
  widget->Show();

  ui::test::EventGenerator event_generator(GetContext(),
                                           widget->GetNativeWindow());

  // Initially, there should be no tooltip.
  event_generator.MoveMouseTo(gfx::Point(50, 50));
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());

  // Create a new button for the "front", and set the tooltip, but don't add it
  // to the view hierarchy yet.
  LabelButton* front = new LabelButton(nullptr, base::string16());
  front->SetBounds(20, 20, 40, 40);
  front->SetTooltipText(tooltip_front);

  // Changing the tooltip text shouldn't require an additional mousemove to take
  // effect.
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());
  back->SetTooltipText(tooltip_back);
  EXPECT_EQ(tooltip_back, TooltipTextForWidget(widget));

  // Adding a new view under the mouse should also take immediate effect.
  back->AddChildView(front);
  EXPECT_EQ(tooltip_front, TooltipTextForWidget(widget));

  // A long tooltip will be wrapped by Cocoa, but the full string should appear.
  // Note that render widget hosts clip at 1024 to prevent DOS, but in toolkit-
  // views the UI is more trusted.
  front->SetTooltipText(long_tooltip);
  EXPECT_EQ(long_tooltip, TooltipTextForWidget(widget));

  // Move the mouse to a different view - tooltip should change.
  event_generator.MoveMouseTo(gfx::Point(15, 15));
  EXPECT_EQ(tooltip_back, TooltipTextForWidget(widget));

  // Move the mouse off of any view, tooltip should clear.
  event_generator.MoveMouseTo(gfx::Point(5, 5));
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());

  widget->CloseNow();
}

// Tests case when mouse events are handled in one Widget,
// but tooltip belongs to another.
// It happens in menus when a submenu is shown and the parent gets the
// MouseExit event.
TEST_F(NativeWidgetMacTest, TwoWidgetTooltips) {
  // Init two widgets, one above another.
  Widget* widget_below = CreateTopLevelPlatformWidget();
  widget_below->SetBounds(gfx::Rect(50, 50, 200, 200));

  Widget* widget_above =
      CreateChildPlatformWidget(widget_below->GetNativeView());
  widget_above->SetBounds(gfx::Rect(100, 0, 100, 200));

  const base::string16 tooltip_above = base::ASCIIToUTF16("Front");
  CustomTooltipView* view_above = new CustomTooltipView(tooltip_above, nullptr);
  view_above->SetBoundsRect(widget_above->GetContentsView()->bounds());
  widget_above->GetContentsView()->AddChildView(view_above);

  CustomTooltipView* view_below =
      new CustomTooltipView(base::ASCIIToUTF16("Back"), view_above);
  view_below->SetBoundsRect(widget_below->GetContentsView()->bounds());
  widget_below->GetContentsView()->AddChildView(view_below);

  widget_below->Show();
  widget_above->Show();

  // Move mouse above second widget and check that it returns tooltip
  // for second. Despite that event was handled in the first one.
  ui::test::EventGenerator event_generator(GetContext(),
                                           widget_below->GetNativeWindow());
  event_generator.MoveMouseTo(gfx::Point(120, 60));
  EXPECT_EQ(tooltip_above, TooltipTextForWidget(widget_below));

  widget_above->CloseNow();
  widget_below->CloseNow();
}

namespace {

// Delegate to make Widgets of a provided ui::ModalType.
class ModalDialogDelegate : public DialogDelegateView {
 public:
  explicit ModalDialogDelegate(ui::ModalType modal_type)
      : modal_type_(modal_type) {}

  void set_can_close(bool value) { can_close_ = value; }
  void set_buttons(int buttons) { buttons_ = buttons; }

  // DialogDelegateView:
  int GetDialogButtons() const override { return buttons_; }
  ui::ModalType GetModalType() const override { return modal_type_; }
  bool Close() override { return can_close_; }

 private:
  const ui::ModalType modal_type_;
  bool can_close_ = true;
  int buttons_ = ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;

  DISALLOW_COPY_AND_ASSIGN(ModalDialogDelegate);
};

// While in scope, waits for a call to a swizzled objective C method, then quits
// a nested run loop.
class ScopedSwizzleWaiter {
 public:
  explicit ScopedSwizzleWaiter(Class target)
      : swizzler_(target,
                  [TestStopAnimationWaiter class],
                  @selector(setWindowStateForEnd)) {
    DCHECK(!instance_);
    instance_ = this;
  }

  ~ScopedSwizzleWaiter() { instance_ = nullptr; }

  static IMP GetMethodAndMarkCalled() {
    return instance_->GetMethodInternal();
  }

  void WaitForMethod() {
    if (method_called_)
      return;

    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  bool method_called() const { return method_called_; }

 private:
  IMP GetMethodInternal() {
    DCHECK(!method_called_);
    method_called_ = true;
    if (run_loop_)
      run_loop_->Quit();
    return swizzler_.GetOriginalImplementation();
  }

  static ScopedSwizzleWaiter* instance_;

  base::mac::ScopedObjCClassSwizzler swizzler_;
  base::RunLoop* run_loop_ = nullptr;
  bool method_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedSwizzleWaiter);
};

ScopedSwizzleWaiter* ScopedSwizzleWaiter::instance_ = nullptr;

// Shows a modal widget and waits for the show animation to complete. Waiting is
// not compulsory (calling Close() while animating the show will cancel the show
// animation). However, testing with overlapping swizzlers is tricky.
Widget* ShowChildModalWidgetAndWait(NSWindow* native_parent) {
  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      new ModalDialogDelegate(ui::MODAL_TYPE_CHILD), nullptr,
      [native_parent contentView]);

  modal_dialog_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  EXPECT_FALSE(modal_dialog_widget->IsVisible());
  ScopedSwizzleWaiter show_waiter([ConstrainedWindowAnimationShow class]);

  BridgedNativeWidgetTestApi test_api(
      modal_dialog_widget->GetNativeWindow().GetNativeNSWindow());
  EXPECT_FALSE(test_api.show_animation());

  modal_dialog_widget->Show();
  // Visible immediately (although it animates from transparent).
  EXPECT_TRUE(modal_dialog_widget->IsVisible());
  base::scoped_nsobject<NSAnimation> retained_animation(
      test_api.show_animation(), base::scoped_policy::RETAIN);
  EXPECT_TRUE(retained_animation);
  EXPECT_TRUE([retained_animation isAnimating]);

  // Run the animation.
  show_waiter.WaitForMethod();
  EXPECT_TRUE(modal_dialog_widget->IsVisible());
  EXPECT_TRUE(show_waiter.method_called());
  EXPECT_FALSE([retained_animation isAnimating]);
  EXPECT_FALSE(test_api.show_animation());
  return modal_dialog_widget;
}

// Shows a window-modal Widget (as a sheet). No need to wait since the native
// sheet animation is blocking.
Widget* ShowWindowModalWidget(NSWindow* native_parent) {
  Widget* sheet_widget = views::DialogDelegate::CreateDialogWidget(
      new ModalDialogDelegate(ui::MODAL_TYPE_WINDOW), nullptr,
      [native_parent contentView]);
  sheet_widget->Show();
  return sheet_widget;
}

}  // namespace

// Tests object lifetime for the show/hide animations used for child-modal
// windows.
TEST_F(NativeWidgetMacTest, NativeWindowChildModalShowHide) {
  NSWindow* native_parent = MakeBorderlessNativeParent();
  {
    Widget* modal_dialog_widget = ShowChildModalWidgetAndWait(native_parent);
    TestWidgetObserver widget_observer(modal_dialog_widget);

    ScopedSwizzleWaiter hide_waiter([ConstrainedWindowAnimationHide class]);
    EXPECT_TRUE(modal_dialog_widget->IsVisible());
    EXPECT_FALSE(widget_observer.widget_closed());

    // Widget::Close() is always asynchronous, so we can check that the widget
    // is initially visible, but then it's destroyed.
    modal_dialog_widget->Close();
    EXPECT_TRUE(modal_dialog_widget->IsVisible());
    EXPECT_FALSE(hide_waiter.method_called());
    EXPECT_FALSE(widget_observer.widget_closed());

    // Wait for a hide to finish.
    hide_waiter.WaitForMethod();
    EXPECT_TRUE(hide_waiter.method_called());

    // The animation finishing should also mean it has closed the window.
    EXPECT_TRUE(widget_observer.widget_closed());
  }

  {
    // Make a new dialog to test another lifetime flow.
    Widget* modal_dialog_widget = ShowChildModalWidgetAndWait(native_parent);
    TestWidgetObserver widget_observer(modal_dialog_widget);

    // Start an asynchronous close as above.
    ScopedSwizzleWaiter hide_waiter([ConstrainedWindowAnimationHide class]);
    modal_dialog_widget->Close();
    EXPECT_FALSE(widget_observer.widget_closed());
    EXPECT_FALSE(hide_waiter.method_called());

    // Now close the _parent_ window to force a synchronous close of the child.
    [native_parent close];

    // Widget is destroyed immediately. No longer paints, but the animation is
    // still running.
    EXPECT_TRUE(widget_observer.widget_closed());
    EXPECT_FALSE(hide_waiter.method_called());

    // Wait for the hide again. It will call close on its retained copy of the
    // child NSWindow, but that's fine since all the C++ objects are detached.
    hide_waiter.WaitForMethod();
    EXPECT_TRUE(hide_waiter.method_called());
  }
}

// Tests that calls to Hide() a Widget cancel any in-progress show animation,
// and that clients can control the triggering of the animation.
TEST_F(NativeWidgetMacTest, ShowAnimationControl) {
  NSWindow* native_parent = MakeBorderlessNativeParent();
  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      new ModalDialogDelegate(ui::MODAL_TYPE_CHILD), nullptr,
      [native_parent contentView]);

  modal_dialog_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  EXPECT_FALSE(modal_dialog_widget->IsVisible());

  BridgedNativeWidgetTestApi test_api(
      modal_dialog_widget->GetNativeWindow().GetNativeNSWindow());
  EXPECT_FALSE(test_api.show_animation());
  modal_dialog_widget->Show();

  EXPECT_TRUE(modal_dialog_widget->IsVisible());
  base::scoped_nsobject<NSAnimation> retained_animation(
      test_api.show_animation(), base::scoped_policy::RETAIN);
  EXPECT_TRUE(retained_animation);
  EXPECT_TRUE([retained_animation isAnimating]);

  // Hide without waiting for the animation to complete. Animation should cancel
  // and clear references from BridgedNativeWidgetImpl.
  modal_dialog_widget->Hide();
  EXPECT_FALSE([retained_animation isAnimating]);
  EXPECT_FALSE(test_api.show_animation());
  retained_animation.reset();

  // Disable animations and show again.
  modal_dialog_widget->SetVisibilityAnimationTransition(Widget::ANIMATE_NONE);
  modal_dialog_widget->Show();
  EXPECT_FALSE(test_api.show_animation());  // No animation this time.
  modal_dialog_widget->Hide();

  // Test after re-enabling.
  modal_dialog_widget->SetVisibilityAnimationTransition(Widget::ANIMATE_BOTH);
  modal_dialog_widget->Show();
  EXPECT_TRUE(test_api.show_animation());
  retained_animation.reset(test_api.show_animation(),
                           base::scoped_policy::RETAIN);

  // Test whether disabling native animations also disables custom modal ones.
  modal_dialog_widget->SetVisibilityChangedAnimationsEnabled(false);
  modal_dialog_widget->Show();
  EXPECT_FALSE(test_api.show_animation());  // No animation this time.
  modal_dialog_widget->Hide();
  // Renable.
  modal_dialog_widget->SetVisibilityChangedAnimationsEnabled(true);
  modal_dialog_widget->Show();
  EXPECT_TRUE(test_api.show_animation());
  retained_animation.reset(test_api.show_animation(),
                           base::scoped_policy::RETAIN);

  // Closing should also cancel the animation.
  EXPECT_TRUE([retained_animation isAnimating]);
  [native_parent close];
  EXPECT_FALSE([retained_animation isAnimating]);
}

// Tests behavior of window-modal dialogs, displayed as sheets.
TEST_F(NativeWidgetMacTest, WindowModalSheet) {
  NSWindow* native_parent = MakeClosableTitledNativeParent();

  Widget* sheet_widget = views::DialogDelegate::CreateDialogWidget(
      new ModalDialogDelegate(ui::MODAL_TYPE_WINDOW), nullptr,
      [native_parent contentView]);

  WidgetChangeObserver widget_observer(sheet_widget);

  // Retain, to run checks after the Widget is torn down.
  base::scoped_nsobject<NSWindow> sheet_window(
      [sheet_widget->GetNativeWindow().GetNativeNSWindow() retain]);

  // Although there is no titlebar displayed, sheets need NSTitledWindowMask in
  // order to properly engage window-modal behavior in AppKit.
  EXPECT_EQ(NSTitledWindowMask, [sheet_window styleMask]);

  sheet_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  EXPECT_FALSE(sheet_widget->IsVisible());
  EXPECT_FALSE(sheet_widget->GetLayer()->IsDrawn());

  NSButton* parent_close_button =
      [native_parent standardWindowButton:NSWindowCloseButton];
  EXPECT_TRUE(parent_close_button);
  EXPECT_TRUE([parent_close_button isEnabled]);

  bool did_observe = false;
  bool* did_observe_ptr = &did_observe;
  id observer = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSWindowWillBeginSheetNotification
                  object:native_parent
                   queue:nil
              usingBlock:^(NSNotification* note) {
                // Ensure that before the sheet runs, the window contents would
                // be drawn.
                EXPECT_TRUE(sheet_widget->IsVisible());
                EXPECT_TRUE(sheet_widget->GetLayer()->IsDrawn());
                *did_observe_ptr = true;
              }];

  Widget::Widgets children;
  Widget::GetAllChildWidgets([native_parent contentView], &children);
  ASSERT_EQ(2u, children.size());

  sheet_widget->Show();  // Should run the above block, then animate the sheet.
  EXPECT_TRUE(did_observe);
  [[NSNotificationCenter defaultCenter] removeObserver:observer];

  // Ensure sheets are included as a child.
  Widget::GetAllChildWidgets([native_parent contentView], &children);
  ASSERT_EQ(2u, children.size());
  EXPECT_TRUE(children.count(sheet_widget));

  // Modal, so the close button in the parent window should get disabled.
  EXPECT_FALSE([parent_close_button isEnabled]);

  // The sheet should be hidden and shown in step with the parent.
  widget_observer.WaitForVisibleCounts(1, 0);
  EXPECT_TRUE(sheet_widget->IsVisible());

  // TODO(tapted): Ideally [native_parent orderOut:nil] would also work here.
  // But it does not. AppKit's childWindow management breaks down after an
  // -orderOut: (see BridgedNativeWidgetImpl::OnVisibilityChanged()). For
  // regular child windows, BridgedNativeWidgetImpl fixes the behavior with its
  // own management. However, it can't do that for sheets without encountering
  // http://crbug.com/605098 and http://crbug.com/667602. -[NSApp hide:] makes
  // the NSWindow hidden in a different way, which does not break like
  // -orderOut: does. Which is good, because a user can always do -[NSApp
  // hide:], e.g., with Cmd+h, and that needs to work correctly.
  [NSApp hide:nil];

  widget_observer.WaitForVisibleCounts(1, 1);
  EXPECT_FALSE(sheet_widget->IsVisible());
  [native_parent makeKeyAndOrderFront:nil];
  widget_observer.WaitForVisibleCounts(2, 1);
  EXPECT_TRUE(sheet_widget->IsVisible());

  // Trigger the close. Don't use CloseNow, since that tears down the UI before
  // the close sheet animation gets a chance to run (so it's banned).
  sheet_widget->Close();
  EXPECT_TRUE(sheet_widget->IsVisible());

  // Pump in order to trigger -[NSWindow endSheet:..], which will block while
  // the animation runs, then delete |sheet_widget|.
  EXPECT_TRUE([sheet_window delegate]);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE([sheet_window delegate]);
  [[NSNotificationCenter defaultCenter] removeObserver:observer];

  EXPECT_TRUE(widget_observer.widget_closed());
  EXPECT_TRUE([parent_close_button isEnabled]);

  [native_parent close];
}

// Tests behavior when closing a window that is a sheet, or that hosts a sheet,
// and reshowing a sheet on a window after the sheet was closed with -[NSWindow
// close].
TEST_F(NativeWidgetMacTest, CloseWithWindowModalSheet) {
  NSWindow* native_parent = MakeClosableTitledNativeParent();

  {
    Widget* sheet_widget = ShowWindowModalWidget(native_parent);
    EXPECT_TRUE(
        [sheet_widget->GetNativeWindow().GetNativeNSWindow() isVisible]);

    WidgetChangeObserver widget_observer(sheet_widget);

    // Test synchronous close (asynchronous close is tested above).
    sheet_widget->CloseNow();
    EXPECT_TRUE(widget_observer.widget_closed());

    // Spin the RunLoop to ensure the task that ends the modal session on
    // |native_parent| is executed. Otherwise |native_parent| will refuse to
    // show another sheet.
    base::RunLoop().RunUntilIdle();
  }

  {
    Widget* sheet_widget = ShowWindowModalWidget(native_parent);

    // Ensure the sheet wasn't blocked by a previous modal session.
    EXPECT_TRUE(
        [sheet_widget->GetNativeWindow().GetNativeNSWindow() isVisible]);

    WidgetChangeObserver widget_observer(sheet_widget);

    // Test native -[NSWindow close] on the sheet. Does not animate.
    [sheet_widget->GetNativeWindow().GetNativeNSWindow() close];
    EXPECT_TRUE(widget_observer.widget_closed());
    base::RunLoop().RunUntilIdle();
  }

  // Similar, but invoke -[NSWindow close] immediately after an asynchronous
  // Close(). This exercises a scenario where two tasks to end the sheet may be
  // posted. Experimentally (on 10.13) both tasks run, but the second will never
  // attempt to invoke -didEndSheet: on the |modalDelegate| arg of -beginSheet:.
  // (If it did, it would be fine.)
  {
    Widget* sheet_widget = ShowWindowModalWidget(native_parent);
    base::scoped_nsobject<NSWindow> sheet_window(
        sheet_widget->GetNativeWindow().GetNativeNSWindow(),
        base::scoped_policy::RETAIN);
    EXPECT_TRUE([sheet_window isVisible]);

    WidgetChangeObserver widget_observer(sheet_widget);
    sheet_widget->Close();  // Asynchronous. Can't be called after -close.
    EXPECT_FALSE(widget_observer.widget_closed());
    [sheet_window close];
    EXPECT_TRUE(widget_observer.widget_closed());
    base::RunLoop().RunUntilIdle();

    // Pretend both tasks ran fully. Note that |sheet_window| serves as its own
    // |modalDelegate|.
    [base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(sheet_window)
        sheetDidEnd:sheet_window
         returnCode:NSModalResponseStop
        contextInfo:nullptr];
  }

  // Test another hypothetical: What if -sheetDidEnd: was invoked somehow
  // without going through [NSApp endSheet:] or -[NSWindow endSheet:].
  {
    base::mac::ScopedNSAutoreleasePool pool;
    Widget* sheet_widget = ShowWindowModalWidget(native_parent);
    NSWindow* sheet_window =
        sheet_widget->GetNativeWindow().GetNativeNSWindow();
    EXPECT_TRUE([sheet_window isVisible]);

    WidgetChangeObserver widget_observer(sheet_widget);
    sheet_widget->Close();

    [base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(sheet_window)
        sheetDidEnd:sheet_window
         returnCode:NSModalResponseStop
        contextInfo:nullptr];

    EXPECT_TRUE(widget_observer.widget_closed());
    // Here, the ViewsNSWindowDelegate should be dealloc'd.
  }
  base::RunLoop().RunUntilIdle();  // Run the task posted in Close().

  // Test -[NSWindow close] on the parent window.
  {
    Widget* sheet_widget = ShowWindowModalWidget(native_parent);
    EXPECT_TRUE(
        [sheet_widget->GetNativeWindow().GetNativeNSWindow() isVisible]);
    WidgetChangeObserver widget_observer(sheet_widget);

    [native_parent close];
    EXPECT_TRUE(widget_observer.widget_closed());
  }
}

// Exercise a scenario where the task posted in the asynchronous Close() could
// eventually complete on a destroyed NSWindowDelegate. Regression test for
// https://crbug.com/851376.
TEST_F(NativeWidgetMacTest, CloseWindowModalSheetWithoutSheetParent) {
  NSWindow* native_parent = MakeClosableTitledNativeParent();
  {
    base::mac::ScopedNSAutoreleasePool pool;
    Widget* sheet_widget = ShowWindowModalWidget(native_parent);
    NSWindow* sheet_window =
        sheet_widget->GetNativeWindow().GetNativeNSWindow();
    EXPECT_TRUE([sheet_window isVisible]);

    sheet_widget->Close();  // Asynchronous. Can't be called after -close.

    // Now there's a task to end the sheet in the message queue. But destroying
    // the NSWindowDelegate without _also_ posting a task that will _retain_ it
    // is hard. It _is_ possible for a -performSelector:afterDelay: already in
    // the queue to happen _after_ a PostTask posted now, but it's a very rare
    // occurrence. So to simulate it, we pretend the sheet isn't actually a
    // sheet by hiding its sheetParent. This avoids a task being posted that
    // would retain the delegate, but also puts |native_parent| into a weird
    // state.
    //
    // In fact, the "real" suspected trigger for this bug requires the PostTask
    // to still be posted, then run to completion, and to dealloc the delegate
    // it retains all before the -performSelector:afterDelay runs. That's the
    // theory anyway.
    //
    // In reality, it didn't seem possible for -sheetDidEnd: to be invoked twice
    // (AppKit would suppress it on subsequent calls to -[NSApp endSheet:] or
    // -[NSWindow endSheet:]), so if the PostTask were to run to completion, the
    // waiting -performSelector would always no- op. So this is actually testing
    // a hypothetical where the sheetParent may be somehow nil during teardown
    // (perhaps due to the parent window being further along in its teardown).
    EXPECT_TRUE([sheet_window sheetParent]);
    [sheet_window setValue:nil forKey:@"sheetParent"];
    EXPECT_FALSE([sheet_window sheetParent]);
    [sheet_window close];

    // To repro the crash, we need a dealloc to occur here on |sheet_widget|'s
    // NSWindowDelegate.
  }
  // Now there is still a task to end the sheet in the message queue, which
  // should not crash.
  base::RunLoop().RunUntilIdle();
  [native_parent close];
}

// Test calls to Widget::ReparentNativeView() that result in a no-op on Mac.
TEST_F(NativeWidgetMacTest, NoopReparentNativeView) {
  NSWindow* parent = MakeBorderlessNativeParent();
  Widget* dialog = views::DialogDelegate::CreateDialogWidget(
      new DialogDelegateView, nullptr, [parent contentView]);
  BridgedNativeWidgetHostImpl* bridge_host =
      BridgedNativeWidgetHostImpl::GetFromNativeWindow(
          dialog->GetNativeWindow());

  EXPECT_EQ(bridge_host->parent()->native_widget_mac()->GetNativeWindow(),
            parent);
  Widget::ReparentNativeView(dialog->GetNativeView(), [parent contentView]);
  EXPECT_EQ(bridge_host->parent()->native_widget_mac()->GetNativeWindow(),
            parent);

  [parent close];

  Widget* parent_widget = CreateNativeDesktopWidget();
  parent = parent_widget->GetNativeWindow().GetNativeNSWindow();
  dialog = views::DialogDelegate::CreateDialogWidget(
      new DialogDelegateView, nullptr, [parent contentView]);
  bridge_host = BridgedNativeWidgetHostImpl::GetFromNativeWindow(
      dialog->GetNativeWindow());

  EXPECT_EQ(bridge_host->parent()->native_widget_mac()->GetNativeWindow(),
            parent);
  Widget::ReparentNativeView(dialog->GetNativeView(), [parent contentView]);
  EXPECT_EQ(bridge_host->parent()->native_widget_mac()->GetNativeWindow(),
            parent);

  parent_widget->CloseNow();
}

// Attaches a child window to |parent| that checks its parent's delegate is
// cleared when the child is destroyed. This assumes the child is destroyed via
// destruction of its parent.
class ParentCloseMonitor : public WidgetObserver {
 public:
  explicit ParentCloseMonitor(Widget* parent) {
    Widget* child = new Widget();
    child->AddObserver(this);
    Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    init_params.parent = parent->GetNativeView();
    init_params.bounds = gfx::Rect(100, 100, 100, 100);
    init_params.native_widget = CreatePlatformNativeWidgetImpl(
        init_params, child, kStubCapture, nullptr);
    child->Init(init_params);
    child->Show();

    // NSWindow parent/child relationship should be established on Show() and
    // the parent should have a delegate. Retain the parent since it can't be
    // retrieved from the child while it is being destroyed.
    parent_nswindow_.reset(
        [[child->GetNativeWindow().GetNativeNSWindow() parentWindow] retain]);
    EXPECT_TRUE(parent_nswindow_);
    EXPECT_TRUE([parent_nswindow_ delegate]);
  }

  ~ParentCloseMonitor() override {
    EXPECT_TRUE(child_closed_);  // Otherwise the observer wasn't removed.
  }

  void OnWidgetDestroying(Widget* child) override {
    // Upon a parent-triggered close, the NSWindow relationship will still exist
    // (it's removed just after OnWidgetDestroying() returns). The parent should
    // still be open (children are always closed first), but not have a delegate
    // (since it is being torn down).
    EXPECT_TRUE([child->GetNativeWindow().GetNativeNSWindow() parentWindow]);
    EXPECT_TRUE([parent_nswindow_ isVisible]);
    EXPECT_FALSE([parent_nswindow_ delegate]);

    EXPECT_FALSE(child_closed_);
  }

  void OnWidgetDestroyed(Widget* child) override {
    EXPECT_FALSE([child->GetNativeWindow().GetNativeNSWindow() parentWindow]);
    EXPECT_TRUE([parent_nswindow_ isVisible]);
    EXPECT_FALSE([parent_nswindow_ delegate]);

    EXPECT_FALSE(child_closed_);
    child->RemoveObserver(this);
    child_closed_ = true;
  }

  bool child_closed() const { return child_closed_; }

 private:
  base::scoped_nsobject<NSWindow> parent_nswindow_;
  bool child_closed_ = false;

  DISALLOW_COPY_AND_ASSIGN(ParentCloseMonitor);
};

// Ensures when a parent window is destroyed, and triggers its child windows to
// be closed, that the child windows (via AppKit) do not attempt to call back
// into the parent, whilst it's in the process of being destroyed.
TEST_F(NativeWidgetMacTest, NoParentDelegateDuringTeardown) {
  // First test "normal" windows and AppKit close.
  {
    Widget* parent = CreateTopLevelPlatformWidget();
    parent->SetBounds(gfx::Rect(100, 100, 300, 200));
    parent->Show();
    ParentCloseMonitor monitor(parent);
    [parent->GetNativeWindow().GetNativeNSWindow() close];
    EXPECT_TRUE(monitor.child_closed());
  }

  // Test the Widget::CloseNow() flow.
  {
    Widget* parent = CreateTopLevelPlatformWidget();
    parent->SetBounds(gfx::Rect(100, 100, 300, 200));
    parent->Show();
    ParentCloseMonitor monitor(parent);
    parent->CloseNow();
    EXPECT_TRUE(monitor.child_closed());
  }

  // Test the WIDGET_OWNS_NATIVE_WIDGET flow.
  {
    std::unique_ptr<Widget> parent(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(100, 100, 300, 200);
    parent->Init(params);
    parent->Show();

    ParentCloseMonitor monitor(parent.get());
    parent.reset();
    EXPECT_TRUE(monitor.child_closed());
  }
}

// Tests Cocoa properties that should be given to particular widget types.
TEST_F(NativeWidgetMacTest, NativeProperties) {
  // Create a regular widget (TYPE_WINDOW).
  Widget* regular_widget = CreateNativeDesktopWidget();
  EXPECT_TRUE([regular_widget->GetNativeWindow().GetNativeNSWindow()
                   canBecomeKeyWindow]);
  EXPECT_TRUE([regular_widget->GetNativeWindow().GetNativeNSWindow()
                   canBecomeMainWindow]);

  // Disabling activation should prevent key and main status.
  regular_widget->widget_delegate()->set_can_activate(false);
  EXPECT_FALSE([regular_widget->GetNativeWindow().GetNativeNSWindow()
                    canBecomeKeyWindow]);
  EXPECT_FALSE([regular_widget->GetNativeWindow().GetNativeNSWindow()
                    canBecomeMainWindow]);

  // Create a dialog widget (also TYPE_WINDOW), but with a DialogDelegate.
  Widget* dialog_widget = views::DialogDelegate::CreateDialogWidget(
      new ModalDialogDelegate(ui::MODAL_TYPE_CHILD), nullptr,
      regular_widget->GetNativeView());
  EXPECT_TRUE([dialog_widget->GetNativeWindow().GetNativeNSWindow()
                   canBecomeKeyWindow]);
  // Dialogs shouldn't take main status away from their parent.
  EXPECT_FALSE([dialog_widget->GetNativeWindow().GetNativeNSWindow()
                    canBecomeMainWindow]);

  // Create a bubble widget with a parent: also shouldn't get main.
  BubbleDialogDelegateView* bubble_view = new SimpleBubbleView();
  bubble_view->set_parent_window(regular_widget->GetNativeView());
  Widget* bubble_widget = BubbleDialogDelegateView::CreateBubble(bubble_view);
  EXPECT_TRUE([bubble_widget->GetNativeWindow().GetNativeNSWindow()
                   canBecomeKeyWindow]);
  EXPECT_FALSE([bubble_widget->GetNativeWindow().GetNativeNSWindow()
                    canBecomeMainWindow]);
  EXPECT_EQ(NSWindowCollectionBehaviorTransient,
            [bubble_widget->GetNativeWindow().GetNativeNSWindow()
                    collectionBehavior] &
                NSWindowCollectionBehaviorTransient);

  // But a bubble without a parent should still be able to become main.
  Widget* toplevel_bubble_widget =
      BubbleDialogDelegateView::CreateBubble(new SimpleBubbleView());
  EXPECT_TRUE([toplevel_bubble_widget->GetNativeWindow().GetNativeNSWindow()
                   canBecomeKeyWindow]);
  EXPECT_TRUE([toplevel_bubble_widget->GetNativeWindow().GetNativeNSWindow()
                   canBecomeMainWindow]);

  toplevel_bubble_widget->CloseNow();
  regular_widget->CloseNow();
}

NSData* WindowContentsAsTIFF(NSWindow* window) {
  NSView* frame_view = [[window contentView] superview];
  EXPECT_TRUE(frame_view);

  // Inset to mask off left and right edges which vary in HighDPI.
  NSRect bounds = NSInsetRect([frame_view bounds], 4, 0);

  // On 10.6, the grippy changes appearance slightly when painted the second
  // time in a textured window. Since this test cares about the window title,
  // cut off the bottom of the window.
  bounds.size.height -= 40;
  bounds.origin.y += 40;

  NSBitmapImageRep* bitmap =
      [frame_view bitmapImageRepForCachingDisplayInRect:bounds];
  EXPECT_TRUE(bitmap);

  [frame_view cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
  NSData* tiff = [bitmap TIFFRepresentation];
  EXPECT_TRUE(tiff);
  return tiff;
}

class CustomTitleWidgetDelegate : public WidgetDelegate {
 public:
  CustomTitleWidgetDelegate(Widget* widget)
      : widget_(widget), should_show_title_(true) {}

  void set_title(const base::string16& title) { title_ = title; }
  void set_should_show_title(bool show) { should_show_title_ = show; }

  // WidgetDelegate:
  base::string16 GetWindowTitle() const override { return title_; }
  bool ShouldShowWindowTitle() const override { return should_show_title_; }
  Widget* GetWidget() override { return widget_; };
  const Widget* GetWidget() const override { return widget_; };

 private:
  Widget* widget_;
  base::string16 title_;
  bool should_show_title_;

  DISALLOW_COPY_AND_ASSIGN(CustomTitleWidgetDelegate);
};

// Test that undocumented title-hiding API we're using does the job.
TEST_F(NativeWidgetMacTest, DISABLED_DoesHideTitle) {
  // Same as CreateTopLevelPlatformWidget but with a custom delegate.
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  Widget* widget = new Widget;
  params.native_widget =
      CreatePlatformNativeWidgetImpl(params, widget, kStubCapture, nullptr);
  CustomTitleWidgetDelegate delegate(widget);
  params.delegate = &delegate;
  params.bounds = gfx::Rect(0, 0, 800, 600);
  widget->Init(params);
  widget->Show();

  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  // Disable color correction so we can read unmodified values from the bitmap.
  [ns_window setColorSpace:[NSColorSpace sRGBColorSpace]];

  EXPECT_EQ(base::string16(), delegate.GetWindowTitle());
  EXPECT_NSEQ(@"", [ns_window title]);
  NSData* empty_title_data = WindowContentsAsTIFF(ns_window);

  delegate.set_title(base::ASCIIToUTF16("This is a title"));
  widget->UpdateWindowTitle();
  NSData* this_title_data = WindowContentsAsTIFF(ns_window);

  // The default window with a title should look different from the
  // window with an empty title.
  EXPECT_NSNE(empty_title_data, this_title_data);

  delegate.set_should_show_title(false);
  delegate.set_title(base::ASCIIToUTF16("This is another title"));
  widget->UpdateWindowTitle();
  NSData* hidden_title_data = WindowContentsAsTIFF(ns_window);

  // With our magic setting, the window with a title should look the
  // same as the window with an empty title.
  EXPECT_TRUE([ns_window _isTitleHidden]);
  EXPECT_NSEQ(empty_title_data, hidden_title_data);

  widget->CloseNow();
}

// Test calls to invalidate the shadow when composited frames arrive.
TEST_F(NativeWidgetMacTest, InvalidateShadow) {
  NativeWidgetMacTestWindow* window;
  const gfx::Rect rect(0, 0, 100, 200);
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.bounds = rect;
  Widget* widget = CreateWidgetWithTestWindow(init_params, &window);

  // Simulate the initial paint.
  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(rect.size());

  // Default is an opaque window, so shadow doesn't need to be invalidated.
  EXPECT_EQ(0, [window invalidateShadowCount]);
  widget->CloseNow();

  init_params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
  widget = CreateWidgetWithTestWindow(init_params, &window);
  BridgedNativeWidgetTestApi test_api(window);

  // First paint on a translucent window needs to invalidate the shadow. Once.
  EXPECT_EQ(0, [window invalidateShadowCount]);
  test_api.SimulateFrameSwap(rect.size());
  EXPECT_EQ(1, [window invalidateShadowCount]);
  test_api.SimulateFrameSwap(rect.size());
  EXPECT_EQ(1, [window invalidateShadowCount]);

  // Resizing the window also needs to trigger a shadow invalidation.
  [window setContentSize:NSMakeSize(123, 456)];
  // A "late" frame swap at the old size should do nothing.
  test_api.SimulateFrameSwap(rect.size());
  EXPECT_EQ(1, [window invalidateShadowCount]);

  test_api.SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(2, [window invalidateShadowCount]);
  test_api.SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(2, [window invalidateShadowCount]);

  // Hiding the window does not require shadow invalidation.
  widget->Hide();
  test_api.SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(2, [window invalidateShadowCount]);

  // Showing a translucent window after hiding it, should trigger shadow
  // invalidation.
  widget->Show();
  test_api.SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(3, [window invalidateShadowCount]);

  widget->CloseNow();
}

// Test that the contentView opacity corresponds to the window type.
TEST_F(NativeWidgetMacTest, ContentOpacity) {
  NativeWidgetMacTestWindow* window;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);

  EXPECT_EQ(init_params.opacity, Widget::InitParams::INFER_OPACITY);
  Widget* widget = CreateWidgetWithTestWindow(init_params, &window);

  // Infer should default to opaque on Mac.
  EXPECT_TRUE([[window contentView] isOpaque]);
  widget->CloseNow();

  init_params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
  widget = CreateWidgetWithTestWindow(init_params, &window);
  EXPECT_FALSE([[window contentView] isOpaque]);
  widget->CloseNow();

  // Test opaque explicitly.
  init_params.opacity = Widget::InitParams::OPAQUE_WINDOW;
  widget = CreateWidgetWithTestWindow(init_params, &window);
  EXPECT_TRUE([[window contentView] isOpaque]);
  widget->CloseNow();
}

// Test the expected result of GetWorkAreaBoundsInScreen().
TEST_F(NativeWidgetMacTest, GetWorkAreaBoundsInScreen) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

  // This is relative to the top-left of the primary screen, so unless the bot's
  // display is smaller than 400x300, the window will be wholly contained there.
  params.bounds = gfx::Rect(100, 100, 300, 200);
  widget.Init(params);
  widget.Show();
  NSRect expected = [[[NSScreen screens] firstObject] visibleFrame];
  NSRect actual = gfx::ScreenRectToNSRect(widget.GetWorkAreaBoundsInScreen());
  EXPECT_FALSE(NSIsEmptyRect(actual));
  EXPECT_NSEQ(expected, actual);

  [widget.GetNativeWindow().GetNativeNSWindow() close];
  actual = gfx::ScreenRectToNSRect(widget.GetWorkAreaBoundsInScreen());
  EXPECT_TRUE(NSIsEmptyRect(actual));
}

// Test that Widget opacity can be changed.
TEST_F(NativeWidgetMacTest, ChangeOpacity) {
  Widget* widget = CreateTopLevelPlatformWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();

  CGFloat old_opacity = [ns_window alphaValue];
  widget->SetOpacity(.7f);
  EXPECT_NE(old_opacity, [ns_window alphaValue]);
  EXPECT_DOUBLE_EQ(.7f, [ns_window alphaValue]);

  widget->CloseNow();
}

// Test that NativeWidgetMac::SchedulePaintInRect correctly passes the dirtyRect
// parameter to BridgedContentView::drawRect, for a titled window (window with a
// toolbar).
TEST_F(NativeWidgetMacTest, SchedulePaintInRect_Titled) {
  Widget* widget = CreateTopLevelPlatformWidget();

  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  // Setup the mock content view for the NSWindow, so that we can intercept
  // drawRect.
  NSWindow* window = widget->GetNativeWindow().GetNativeNSWindow();
  base::scoped_nsobject<MockBridgedView> mock_bridged_view(
      [[MockBridgedView alloc] init]);
  // Reset drawRect count.
  [mock_bridged_view setDrawRectCount:0];
  [window setContentView:mock_bridged_view];

  // Ensure the initial draw of the window is done.
  while ([mock_bridged_view drawRectCount] == 0)
    base::RunLoop().RunUntilIdle();

  // Add a dummy view to the widget. This will cause SchedulePaint to be called
  // on the dummy view.
  View* dummy_view = new View();
  gfx::Rect dummy_bounds(25, 30, 10, 15);
  dummy_view->SetBoundsRect(dummy_bounds);
  // Reset drawRect count.
  [mock_bridged_view setDrawRectCount:0];
  widget->GetContentsView()->AddChildView(dummy_view);

  // SchedulePaint is asyncronous. Wait for drawRect: to be called.
  while ([mock_bridged_view drawRectCount] == 0)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, [mock_bridged_view drawRectCount]);
  int client_area_height = widget->GetClientAreaBoundsInScreen().height();
  // These are expected dummy_view bounds in AppKit coordinate system. The y
  // coordinate of rect origin is calculated as:
  // client_area_height - 30 (dummy_view's y coordinate) - 15 (dummy view's
  // height).
  gfx::Rect expected_appkit_bounds(25, client_area_height - 45, 10, 15);
  EXPECT_NSEQ(expected_appkit_bounds.ToCGRect(),
              [mock_bridged_view lastDirtyRect]);
  widget->CloseNow();
}

// Test that NativeWidgetMac::SchedulePaintInRect correctly passes the dirtyRect
// parameter to BridgedContentView::drawRect, for a borderless window.
TEST_F(NativeWidgetMacTest, SchedulePaintInRect_Borderless) {
  Widget* widget = CreateTopLevelFramelessPlatformWidget();

  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  // Setup the mock content view for the NSWindow, so that we can intercept
  // drawRect.
  NSWindow* window = widget->GetNativeWindow().GetNativeNSWindow();
  base::scoped_nsobject<MockBridgedView> mock_bridged_view(
      [[MockBridgedView alloc] init]);
  // Reset drawRect count.
  [mock_bridged_view setDrawRectCount:0];
  [window setContentView:mock_bridged_view];

  // Ensure the initial draw of the window is done.
  while ([mock_bridged_view drawRectCount] == 0)
    base::RunLoop().RunUntilIdle();

  // Add a dummy view to the widget. This will cause SchedulePaint to be called
  // on the dummy view.
  View* dummy_view = new View();
  gfx::Rect dummy_bounds(25, 30, 10, 15);
  dummy_view->SetBoundsRect(dummy_bounds);
  // Reset drawRect count.
  [mock_bridged_view setDrawRectCount:0];
  widget->GetRootView()->AddChildView(dummy_view);

  // SchedulePaint is asyncronous. Wait for drawRect: to be called.
  while ([mock_bridged_view drawRectCount] == 0)
    base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, [mock_bridged_view drawRectCount]);
  // These are expected dummy_view bounds in AppKit coordinate system. The y
  // coordinate of rect origin is calculated as:
  // 100(client area height) - 30 (dummy_view's y coordinate) - 15 (dummy view's
  // height).
  gfx::Rect expected_appkit_bounds(25, 55, 10, 15);
  EXPECT_NSEQ(expected_appkit_bounds.ToCGRect(),
              [mock_bridged_view lastDirtyRect]);
  widget->CloseNow();
}

// Ensure traversing NSView focus correctly updates the views::FocusManager.
TEST_F(NativeWidgetMacTest, ChangeFocusOnChangeFirstResponder) {
  Widget* widget = CreateTopLevelPlatformWidget();
  widget->GetRootView()->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget->Show();

  base::scoped_nsobject<NSView> child_view([[FocusableTestNSView alloc]
      initWithFrame:[widget->GetNativeView().GetNativeNSView() bounds]]);
  [widget->GetNativeView().GetNativeNSView() addSubview:child_view];
  EXPECT_TRUE([child_view acceptsFirstResponder]);
  EXPECT_TRUE(widget->GetRootView()->IsFocusable());

  FocusManager* manager = widget->GetFocusManager();
  manager->SetFocusedView(widget->GetRootView());
  EXPECT_EQ(manager->GetFocusedView(), widget->GetRootView());

  [widget->GetNativeWindow().GetNativeNSWindow() makeFirstResponder:child_view];
  EXPECT_FALSE(manager->GetFocusedView());

  [widget->GetNativeWindow().GetNativeNSWindow()
      makeFirstResponder:widget->GetNativeView().GetNativeNSView()];
  EXPECT_EQ(manager->GetFocusedView(), widget->GetRootView());

  widget->CloseNow();
}

// Ensure reparented native view has correct bounds.
TEST_F(NativeWidgetMacTest, ReparentNativeViewBounds) {
  Widget* parent = CreateTopLevelFramelessPlatformWidget();
  gfx::Rect parent_rect(100, 100, 300, 200);
  parent->SetBounds(parent_rect);

  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.parent = parent->GetNativeView();
  Widget* widget = new Widget;
  widget->Init(params);
  widget->SetContentsView(new View);

  NSView* child_view = widget->GetNativeView().GetNativeNSView();
  Widget::ReparentNativeView(child_view, parent->GetNativeView());

  // Reparented content view has the size of the Widget that created it.
  gfx::Rect widget_rect(0, 0, 200, 100);
  widget->SetBounds(widget_rect);
  EXPECT_EQ(200, NSWidth([child_view frame]));
  EXPECT_EQ(100, NSHeight([child_view frame]));

  // Reparented widget has bounds relative to the native parent
  NSRect native_parent_rect = NSMakeRect(50, 100, 200, 70);
  base::scoped_nsobject<NSView> native_parent(
      [[NSView alloc] initWithFrame:native_parent_rect]);
  [parent->GetNativeView().GetNativeNSView() addSubview:native_parent];

  gfx::Rect screen_rect = widget->GetWindowBoundsInScreen();
  EXPECT_EQ(100, screen_rect.x());
  EXPECT_EQ(100, screen_rect.y());

  Widget::ReparentNativeView(child_view, native_parent.get());
  widget->SetBounds(widget_rect);
  screen_rect = widget->GetWindowBoundsInScreen();
  EXPECT_EQ(150, screen_rect.x());
  EXPECT_EQ(130, screen_rect.y());

  parent->CloseNow();
}

// Test two kinds of widgets to re-parent.
TEST_F(NativeWidgetMacTest, ReparentNativeViewTypes) {
  std::unique_ptr<Widget> toplevel1(new Widget);
  Widget::InitParams toplevel_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  toplevel_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  toplevel1->Init(toplevel_params);
  toplevel1->Show();

  std::unique_ptr<Widget> toplevel2(new Widget);
  toplevel2->Init(toplevel_params);
  toplevel2->Show();

  Widget* child = new Widget;
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child->Init(child_params);
  child->Show();

  Widget::ReparentNativeView(child->GetNativeView(),
                             toplevel1->GetNativeView());
  EXPECT_EQ([child->GetNativeView().GetNativeNSView() window],
            [toplevel1->GetNativeView().GetNativeNSView() window]);
  EXPECT_EQ(0, [child->GetNativeWindow().GetNativeNSWindow() alphaValue]);

  Widget::ReparentNativeView(child->GetNativeView(),
                             toplevel2->GetNativeView());
  EXPECT_EQ([child->GetNativeView().GetNativeNSView() window],
            [toplevel2->GetNativeView().GetNativeNSView() window]);
  EXPECT_EQ(0, [child->GetNativeWindow().GetNativeNSWindow() alphaValue]);
  EXPECT_NE(0, [toplevel1->GetNativeWindow().GetNativeNSWindow() alphaValue]);

  Widget::ReparentNativeView(toplevel2->GetNativeView(),
                             toplevel1->GetNativeView());
  EXPECT_EQ([toplevel2->GetNativeWindow().GetNativeNSWindow() parentWindow],
            [toplevel1->GetNativeView().GetNativeNSView() window]);
}

// Test class for Full Keyboard Access related tests.
class NativeWidgetMacFullKeyboardAccessTest : public NativeWidgetMacTest {
 public:
  NativeWidgetMacFullKeyboardAccessTest() {}

 protected:
  // testing::Test:
  void SetUp() override {
    NativeWidgetMacTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget();
    bridge_ = BridgedNativeWidgetHostImpl::GetFromNativeWindow(
                  widget_->GetNativeWindow())
                  ->bridge_impl();
    fake_full_keyboard_access_ =
        ui::test::ScopedFakeFullKeyboardAccess::GetInstance();
    DCHECK(fake_full_keyboard_access_);
    widget_->Show();
  }

  void TearDown() override {
    widget_->CloseNow();
    NativeWidgetMacTest::TearDown();
  }

  Widget* widget_ = nullptr;
  BridgedNativeWidgetImpl* bridge_ = nullptr;
  ui::test::ScopedFakeFullKeyboardAccess* fake_full_keyboard_access_ = nullptr;
};

// Test that updateFullKeyboardAccess method on BridgedContentView correctly
// sets the keyboard accessibility mode on the associated focus manager.
TEST_F(NativeWidgetMacFullKeyboardAccessTest, FullKeyboardToggle) {
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());
  fake_full_keyboard_access_->set_full_keyboard_access_state(false);
  [bridge_->ns_view() updateFullKeyboardAccess];
  EXPECT_FALSE(widget_->GetFocusManager()->keyboard_accessible());
  fake_full_keyboard_access_->set_full_keyboard_access_state(true);
  [bridge_->ns_view() updateFullKeyboardAccess];
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());
}

// Test that a Widget's associated FocusManager is initialized with the correct
// keyboard accessibility value.
TEST_F(NativeWidgetMacFullKeyboardAccessTest, Initialization) {
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());

  fake_full_keyboard_access_->set_full_keyboard_access_state(false);
  Widget* widget2 = CreateTopLevelPlatformWidget();
  EXPECT_FALSE(widget2->GetFocusManager()->keyboard_accessible());
  widget2->CloseNow();
}

// Test that the correct keyboard accessibility mode is set when the window
// becomes active.
TEST_F(NativeWidgetMacFullKeyboardAccessTest, Activation) {
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());

  widget_->Hide();
  fake_full_keyboard_access_->set_full_keyboard_access_state(false);
  // [bridge_->ns_view() updateFullKeyboardAccess] is not explicitly called
  // since we may not receive full keyboard access toggle notifications when our
  // application is inactive.

  widget_->Show();
  EXPECT_FALSE(widget_->GetFocusManager()->keyboard_accessible());

  widget_->Hide();
  fake_full_keyboard_access_->set_full_keyboard_access_state(true);

  widget_->Show();
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());
}

class NativeWidgetMacViewsOrderTest : public WidgetTest {
 public:
  NativeWidgetMacViewsOrderTest() {}

 protected:
  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget();

    // It's not a bug if the native view has a different number of subviews,
    // but the rest of this test assumes it. It may or may not be worth
    // removing that assumption at some point.
    ASSERT_EQ(0u,
              [[widget_->GetNativeView().GetNativeNSView() subviews] count]);

    native_host_parent_ = new View();
    widget_->GetContentsView()->AddChildView(native_host_parent_);

    const int kNativeViewCount = 3;
    for (int i = 0; i < kNativeViewCount; ++i) {
      std::unique_ptr<NativeHostHolder> holder(new NativeHostHolder());
      native_host_parent_->AddChildView(holder->host());
      holder->AttachNativeView();
      hosts_.push_back(std::move(holder));
    }
    EXPECT_EQ(kNativeViewCount, native_host_parent_->child_count());
    EXPECT_NSEQ([widget_->GetNativeView().GetNativeNSView() subviews],
                (@[ hosts_[0]->view(), hosts_[1]->view(), hosts_[2]->view() ]));
  }

  void TearDown() override {
    widget_->CloseNow();
    WidgetTest::TearDown();
  }

  NSView* GetContentNativeView() {
    return widget_->GetNativeView().GetNativeNSView();
  }

  Widget* widget_ = nullptr;
  View* native_host_parent_ = nullptr;
  std::vector<std::unique_ptr<NativeHostHolder>> hosts_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacViewsOrderTest);
};

// Test that NativeViewHost::Attach()/Detach() method saves the NativeView
// z-order.
TEST_F(NativeWidgetMacViewsOrderTest, NativeViewAttached) {
  hosts_[1]->Detach();
  EXPECT_NSEQ([GetContentNativeView() subviews],
              (@[ hosts_[0]->view(), hosts_[2]->view() ]));

  hosts_[1]->AttachNativeView();
  EXPECT_NSEQ([GetContentNativeView() subviews],
              (@[ hosts_[0]->view(), hosts_[1]->view(), hosts_[2]->view() ]));
}

// Tests that NativeViews order changes according to views::View hierarchy.
TEST_F(NativeWidgetMacViewsOrderTest, ReorderViews) {
  native_host_parent_->ReorderChildView(hosts_[2]->host(), 1);
  EXPECT_NSEQ([GetContentNativeView() subviews],
              (@[ hosts_[0]->view(), hosts_[2]->view(), hosts_[1]->view() ]));

  native_host_parent_->RemoveChildView(hosts_[2]->host());
  EXPECT_NSEQ([GetContentNativeView() subviews],
              (@[ hosts_[0]->view(), hosts_[1]->view() ]));

  View* new_parent = new View();
  native_host_parent_->RemoveChildView(hosts_[1]->host());
  native_host_parent_->AddChildView(new_parent);
  new_parent->AddChildView(hosts_[1]->host());
  new_parent->AddChildView(hosts_[2]->host());
  EXPECT_NSEQ([GetContentNativeView() subviews],
              (@[ hosts_[0]->view(), hosts_[1]->view(), hosts_[2]->view() ]));

  native_host_parent_->ReorderChildView(new_parent, 0);
  EXPECT_NSEQ([GetContentNativeView() subviews],
              (@[ hosts_[1]->view(), hosts_[2]->view(), hosts_[0]->view() ]));
}

// Test that unassociated native views stay on top after reordering.
TEST_F(NativeWidgetMacViewsOrderTest, UnassociatedViewsIsAbove) {
  base::scoped_nsobject<NSView> child_view([[NSView alloc] init]);
  [GetContentNativeView() addSubview:child_view];
  EXPECT_NSEQ(
      [GetContentNativeView() subviews], (@[
        hosts_[0]->view(), hosts_[1]->view(), hosts_[2]->view(), child_view
      ]));

  native_host_parent_->ReorderChildView(hosts_[2]->host(), 1);
  EXPECT_NSEQ(
      [GetContentNativeView() subviews], (@[
        hosts_[0]->view(), hosts_[2]->view(), hosts_[1]->view(), child_view
      ]));
}

// Test -[NSWindowDelegate windowShouldClose:].
TEST_F(NativeWidgetMacTest, CanClose) {
  ModalDialogDelegate* delegate = new ModalDialogDelegate(ui::MODAL_TYPE_NONE);
  Widget* widget =
      views::DialogDelegate::CreateDialogWidget(delegate, nullptr, nullptr);
  NSWindow* window = widget->GetNativeWindow().GetNativeNSWindow();
  delegate->set_can_close(false);
  EXPECT_FALSE([[window delegate] windowShouldClose:window]);
  delegate->set_can_close(true);
  EXPECT_TRUE([[window delegate] windowShouldClose:window]);
  widget->CloseNow();
}

namespace {

// Returns an array of NSTouchBarItemIdentifier (i.e. NSString), extracted from
// the principal of |view|'s touch bar, which must be a NSGroupTouchBarItem.
// Also verifies that the touch bar's delegate returns non-nil for all items.
NSArray* ExtractTouchBarGroupIdentifiers(NSView* view) {
  NSArray* result = nil;
  if (@available(macOS 10.12.2, *)) {
    NSTouchBar* touch_bar = [view touchBar];
    NSTouchBarItemIdentifier principal = [touch_bar principalItemIdentifier];
    EXPECT_TRUE(principal);
    NSGroupTouchBarItem* group = base::mac::ObjCCastStrict<NSGroupTouchBarItem>(
        [[touch_bar delegate] touchBar:touch_bar
                 makeItemForIdentifier:principal]);
    EXPECT_TRUE(group);
    NSTouchBar* nested_touch_bar = [group groupTouchBar];
    result = [nested_touch_bar itemIdentifiers];

    for (NSTouchBarItemIdentifier item in result) {
      EXPECT_TRUE([[touch_bar delegate] touchBar:nested_touch_bar
                           makeItemForIdentifier:item]);
    }
  }
  return result;
}

}  // namespace

// Test TouchBar integration.
TEST_F(NativeWidgetMacTest, TouchBar) {
  ModalDialogDelegate* delegate = new ModalDialogDelegate(ui::MODAL_TYPE_NONE);
  views::DialogDelegate::CreateDialogWidget(delegate, nullptr, nullptr);
  DialogClientView* client_view = delegate->GetDialogClientView();
  NSView* content =
      [delegate->GetWidget()->GetNativeWindow().GetNativeNSWindow()
              contentView];

  NSString* principal = nil;
  NSObject* old_touch_bar = nil;

  // Constants from bridged_content_view_touch_bar.mm.
  NSString* const kTouchBarOKId = @"com.google.chrome-OK";
  NSString* const kTouchBarCancelId = @"com.google.chrome-CANCEL";

  EXPECT_TRUE(content);
  EXPECT_TRUE(client_view->ok_button());
  EXPECT_TRUE(client_view->cancel_button());

  if (@available(macOS 10.12.2, *)) {
    NSTouchBar* touch_bar = [content touchBar];
    EXPECT_TRUE([touch_bar delegate]);
    EXPECT_TRUE([[touch_bar delegate] touchBar:touch_bar
                         makeItemForIdentifier:kTouchBarOKId]);
    EXPECT_TRUE([[touch_bar delegate] touchBar:touch_bar
                         makeItemForIdentifier:kTouchBarCancelId]);

    principal = [touch_bar principalItemIdentifier];
    EXPECT_NSEQ(@"com.google.chrome-DIALOG-BUTTONS-GROUP", principal);
    EXPECT_NSEQ((@[ kTouchBarCancelId, kTouchBarOKId ]),
                ExtractTouchBarGroupIdentifiers(content));

    // Ensure the touchBar is recreated by comparing pointers.
    old_touch_bar = touch_bar;
    EXPECT_NSEQ(old_touch_bar, [content touchBar]);
  }

  // Remove the cancel button.
  delegate->set_buttons(ui::DIALOG_BUTTON_OK);
  delegate->DialogModelChanged();
  EXPECT_TRUE(client_view->ok_button());
  EXPECT_FALSE(client_view->cancel_button());

  if (@available(macOS 10.12.2, *)) {
    NSTouchBar* touch_bar = [content touchBar];
    EXPECT_NSNE(old_touch_bar, touch_bar);
    EXPECT_NSEQ((@[ kTouchBarOKId ]), ExtractTouchBarGroupIdentifiers(content));
  }

  delegate->GetWidget()->CloseNow();
}

}  // namespace test
}  // namespace views

@implementation TestStopAnimationWaiter
- (void)setWindowStateForEnd {
  views::test::ScopedSwizzleWaiter::GetMethodAndMarkCalled()(self, _cmd);
}
@end

@implementation NativeWidgetMacTestWindow

@synthesize invalidateShadowCount = invalidateShadowCount_;
@synthesize orderWindowCount = orderWindowCount_;
@synthesize deallocFlag = deallocFlag_;

- (void)dealloc {
  if (deallocFlag_) {
    DCHECK(!*deallocFlag_);
    *deallocFlag_ = true;
  }
  [super dealloc];
}

- (void)invalidateShadow {
  ++invalidateShadowCount_;
  [super invalidateShadow];
}

- (void)orderWindow:(NSWindowOrderingMode)orderingMode
         relativeTo:(NSInteger)otherWindowNumber {
  ++orderWindowCount_;
  [super orderWindow:orderingMode relativeTo:otherWindowNumber];
}

@end

@implementation MockBridgedView

@synthesize drawRectCount = drawRectCount_;
@synthesize lastDirtyRect = lastDirtyRect_;

- (void)drawRect:(NSRect)dirtyRect {
  ++drawRectCount_;
  lastDirtyRect_ = dirtyRect;
}

@end

@implementation FocusableTestNSView
- (BOOL)acceptsFirstResponder {
  return YES;
}
@end

