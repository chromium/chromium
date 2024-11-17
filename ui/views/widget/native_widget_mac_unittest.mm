// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/apple/foundation_util.h"
#import "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#import "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"
#import "ui/base/cocoa/tool_tip_base_view.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#import "ui/base/test/scoped_fake_full_keyboard_access.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/events/test/event_generator.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget_interactive_uitest_utils.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace {
// "{}" in base64encode, to create some dummy restoration data.
const std::string kDummyWindowRestorationData = "e30=";
}  // namespace

// Donates an implementation of -[NSAnimation stopAnimation] which calls the
// original implementation, then quits a nested run loop.
@interface TestStopAnimationWaiter : NSObject
@end

@interface ConstrainedWindowAnimationBase (TestingAPI)
- (void)setWindowStateForEnd;
@end

// Test NSWindow that provides hooks via method overrides to verify behavior.
@interface NativeWidgetMacTestWindow : NativeWidgetMacNSWindow
@property(readonly, nonatomic) int invalidateShadowCount;
@property(assign, nonatomic) BOOL fakeOnInactiveSpace;
@property(assign, nonatomic) bool* deallocFlag;
+ (void)waitForDealloc;
@end

// Used to mock BridgedContentView so that calls to drawRect: can be
// intercepted.
@interface MockBridgedView : NSView {
 @private
  // Number of times -[NSView drawRect:] has been called.
  NSUInteger _drawRectCount;

  // The dirtyRect parameter passed to last invocation of drawRect:.
  NSRect _lastDirtyRect;
}

@property(assign, nonatomic) NSUInteger drawRectCount;
@property(assign, nonatomic) NSRect lastDirtyRect;
@end

@interface FocusableTestNSView : NSView
@end

namespace views::test {

// NativeWidgetNSWindowBridge friend to access private members.
class BridgedNativeWidgetTestApi {
 public:
  explicit BridgedNativeWidgetTestApi(NSWindow* window)
      : bridge_(*NativeWidgetMacNSWindowHost::GetFromNativeWindow(window)
                     ->GetInProcessNSWindowBridge()) {}

  explicit BridgedNativeWidgetTestApi(Widget* widget)
      : BridgedNativeWidgetTestApi(
            widget->GetNativeWindow().GetNativeNSWindow()) {}

  BridgedNativeWidgetTestApi(const BridgedNativeWidgetTestApi&) = delete;
  BridgedNativeWidgetTestApi& operator=(const BridgedNativeWidgetTestApi&) =
      delete;

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
    return base::apple::ObjCCastStrict<NSAnimation>(bridge_->show_animation_);
  }

  bool HasWindowRestorationData() {
    return bridge_->HasWindowRestorationData();
  }

 private:
  const raw_ref<remote_cocoa::NativeWidgetNSWindowBridge> bridge_;
};

// Custom native_widget to create a NativeWidgetMacTestWindow.
class TestWindowNativeWidgetMac : public NativeWidgetMac {
 public:
  explicit TestWindowNativeWidgetMac(Widget* delegate)
      : NativeWidgetMac(delegate) {}

  TestWindowNativeWidgetMac(const TestWindowNativeWidgetMac&) = delete;
  TestWindowNativeWidgetMac& operator=(const TestWindowNativeWidgetMac&) =
      delete;

 protected:
  // NativeWidgetMac:
  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override {
    params->style_mask = NSWindowStyleMaskBorderless;
    if (widget_params.type == Widget::InitParams::TYPE_WINDOW) {
      params->style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable |
                           NSWindowStyleMaskResizable;
    }
  }
  NativeWidgetMacNSWindow* CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params) override {
    return [[NativeWidgetMacTestWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:params->style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO];
  }
};

// Tests for parts of NativeWidgetMac not covered by NativeWidgetNSWindowBridge,
// which need access to Cocoa APIs.
class NativeWidgetMacTest : public WidgetTest {
 public:
  NativeWidgetMacTest() = default;

  NativeWidgetMacTest(const NativeWidgetMacTest&) = delete;
  NativeWidgetMacTest& operator=(const NativeWidgetMacTest&) = delete;

  // Make an NSWindow with a close button and a title bar to use as a parent.
  // This NSWindow is backed by a widget that is not exposed to the caller.
  // To destroy the Widget, the native NSWindow must be closed.
  NativeWidgetMacTestWindow* MakeClosableTitledNativeParent() {
    NativeWidgetMacTestWindow* native_parent = nil;
    Widget::InitParams parent_init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    parent_init_params.bounds = gfx::Rect(100, 100, 200, 200);
    CreateWidgetWithTestWindow(std::move(parent_init_params), &native_parent);
    return native_parent;
  }

  // Same as the above, but creates a borderless NSWindow.
  NativeWidgetMacTestWindow* MakeBorderlessNativeParent() {
    NativeWidgetMacTestWindow* native_parent = nil;
    Widget::InitParams parent_init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    parent_init_params.remove_standard_frame = true;
    parent_init_params.bounds = gfx::Rect(100, 100, 200, 200);
    CreateWidgetWithTestWindow(std::move(parent_init_params), &native_parent);
    return native_parent;
  }

  // Create a Widget backed by the NativeWidgetMacTestWindow NSWindow subclass.
  Widget* CreateWidgetWithTestWindow(Widget::InitParams params,
                                     NativeWidgetMacTestWindow** window) {
    Widget* widget = new Widget;
    params.native_widget = new TestWindowNativeWidgetMac(widget);
    widget->Init(std::move(params));
    widget->Show();
    *window = base::apple::ObjCCastStrict<NativeWidgetMacTestWindow>(
        widget->GetNativeWindow().GetNativeNSWindow());
    EXPECT_TRUE(*window);
    return widget;
  }

  FocusManager* GetFocusManager(NativeWidgetMac* native_widget) const {
    return native_widget->focus_manager_;
  }
};

class WidgetChangeObserver : public TestWidgetObserver {
 public:
  explicit WidgetChangeObserver(Widget* widget) : TestWidgetObserver(widget) {}

  WidgetChangeObserver(const WidgetChangeObserver&) = delete;
  WidgetChangeObserver& operator=(const WidgetChangeObserver&) = delete;

  void WaitForVisibleCounts(int gained, int lost) {
    if (gained_visible_count_ >= gained && lost_visible_count_ >= lost)
      return;

    target_gained_visible_count_ = gained;
    target_lost_visible_count_ = lost;

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

// This class gives public access to the protected ctor of
// BubbleDialogDelegateView.
class SimpleBubbleView : public BubbleDialogDelegateView {
  METADATA_HEADER(SimpleBubbleView, BubbleDialogDelegateView)

 public:
  SimpleBubbleView() = default;

  SimpleBubbleView(const SimpleBubbleView&) = delete;
  SimpleBubbleView& operator=(const SimpleBubbleView&) = delete;

  ~SimpleBubbleView() override = default;
};

BEGIN_METADATA(SimpleBubbleView)
END_METADATA

class CustomTooltipView : public View, public ViewObserver {
  METADATA_HEADER(CustomTooltipView, View)

 public:
  CustomTooltipView(const std::u16string& tooltip, View* tooltip_handler)
      : tooltip_(tooltip) {
    if (tooltip_handler) {
      tooltip_handler_observation_.Observe(tooltip_handler);
    }
  }

  CustomTooltipView(const CustomTooltipView&) = delete;
  CustomTooltipView& operator=(const CustomTooltipView&) = delete;

  // View:
  std::u16string GetTooltipText(const gfx::Point& p) const override {
    return tooltip_;
  }

  View* GetTooltipHandlerForPoint(const gfx::Point& point) override {
    return tooltip_handler_observation_.IsObserving()
               ? tooltip_handler_observation_.GetSource()
               : this;
  }

  // ViewObserver::
  void OnViewIsDeleting(View* observed_view) override {
    tooltip_handler_observation_.Reset();
  }

 private:
  std::u16string tooltip_;
  base::ScopedObservation<View, ViewObserver> tooltip_handler_observation_{
      this};
};

BEGIN_METADATA(CustomTooltipView)
END_METADATA

// A Widget subclass that exposes counts to calls made to OnMouseEvent().
class MouseTrackingWidget : public Widget {
 public:
  int GetMouseEventCount(ui::EventType type) { return counts_[type]; }
  void OnMouseEvent(ui::MouseEvent* event) override {
    ++counts_[event->type()];
    Widget::OnMouseEvent(event);
  }

 private:
  std::map<ui::EventType, int> counts_;
};

// Test visibility states triggered externally.
// TODO(crbug.com/40270349): Flaky.
TEST_F(NativeWidgetMacTest, DISABLED_HideAndShowExternally) {
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

// Check methods that should not be implemented by NativeWidgetMac.
TEST_F(NativeWidgetMacTest, NotImplemented) {
  NSWindow* native_parent = MakeBorderlessNativeParent();
  NativeWidgetMacNSWindowHost* window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeWindow(native_parent);

  EXPECT_FALSE(window_host->native_widget_mac()->WillExecuteCommand(
                   5001, WindowOpenDisposition::CURRENT_TAB, true));
  EXPECT_FALSE(window_host->native_widget_mac()->ExecuteCommand(
                   5001, WindowOpenDisposition::CURRENT_TAB, true));

  [native_parent close];
}

// Tests the WindowFrameTitlebarHeight method.
TEST_F(NativeWidgetMacTest, WindowFrameTitlebarHeight) {
  NSWindow* native_parent = MakeBorderlessNativeParent();
  NativeWidgetMacNSWindowHost* window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeWindow(native_parent);

  bool override_titlebar_height = true;
  float titlebar_height = 100.0;
  window_host->native_widget_mac()->GetWindowFrameTitlebarHeight(
      &override_titlebar_height, &titlebar_height);

  EXPECT_EQ(false, override_titlebar_height);
  EXPECT_EQ(0.0, titlebar_height);

  [native_parent close];
}

// A view that counts calls to OnPaint().
class PaintCountView : public View {
  METADATA_HEADER(PaintCountView, View)

 public:
  PaintCountView() { SetBounds(0, 0, 100, 100); }

  PaintCountView(const PaintCountView&) = delete;
  PaintCountView& operator=(const PaintCountView&) = delete;

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
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

BEGIN_METADATA(PaintCountView)
END_METADATA

// Test that a child widget is only added to its parent NSWindow when the
// parent is on the active space. Otherwise, it may cause a space transition.
// See https://crbug.com/866760.
TEST_F(NativeWidgetMacTest, ChildWidgetOnInactiveSpace) {
  NativeWidgetMacTestWindow* parent_window;
  NativeWidgetMacTestWindow* child_window;

  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  init_params.bounds = gfx::Rect(100, 100, 200, 200);
  Widget* parent =
      CreateWidgetWithTestWindow(std::move(init_params), &parent_window);

  parent_window.fakeOnInactiveSpace = YES;

  init_params.parent = parent->GetNativeView();
  CreateWidgetWithTestWindow(std::move(init_params), &child_window);

  EXPECT_EQ(nil, child_window.parentWindow);

  parent_window.fakeOnInactiveSpace = NO;
  parent->Show();
  EXPECT_EQ(parent_window, child_window.parentWindow);

  parent->CloseNow();
}

// Test minimized states triggered externally, implied visibility and restored
// bounds whilst minimized.
TEST_F(NativeWidgetMacTest, MiniaturizeExternally) {
  WidgetAutoclosePtr widget(new Widget);
  Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(init_params));

  auto* view = widget->GetContentsView()->AddChildView(
      std::make_unique<PaintCountView>());
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  WidgetChangeObserver observer(widget.get());

  widget->SetBounds(gfx::Rect(100, 100, 300, 300));

  EXPECT_TRUE(view->IsDrawn());
  EXPECT_EQ(0, view->paint_count());

  {
    views::test::PropertyWaiter visibility_waiter(
        base::BindRepeating(&Widget::IsVisible, base::Unretained(widget.get())),
        true);
    widget->Show();
    EXPECT_TRUE(visibility_waiter.Wait());
  }

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
  {
    views::test::PropertyWaiter minimize_waiter(
        base::BindRepeating(&Widget::IsMinimized,
                            base::Unretained(widget.get())),
        true);
    [ns_window performMiniaturize:nil];
    EXPECT_TRUE(minimize_waiter.Wait());
  }

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

  {
    views::test::PropertyWaiter deminimize_waiter(
        base::BindRepeating(&Widget::IsMinimized,
                            base::Unretained(widget.get())),
        false);
    [ns_window deminiaturize:nil];
    EXPECT_TRUE(deminimize_waiter.Wait());
  }

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());

  view->WaitForPaintCount(2);  // A single paint when deminiaturizing.
  EXPECT_FALSE([ns_window isMiniaturized]);

  {
    views::test::PropertyWaiter minimize_waiter(
        base::BindRepeating(&Widget::IsMinimized,
                            base::Unretained(widget.get())),
        true);
    widget->Minimize();
    EXPECT_TRUE(minimize_waiter.Wait());
  }

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_TRUE([ns_window isMiniaturized]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(2, view->paint_count());  // No paint when miniaturizing.

  {
    views::test::PropertyWaiter deminimize_waiter(
        base::BindRepeating(&Widget::IsMinimized,
                            base::Unretained(widget.get())),
        false);
    widget->Restore();  // If miniaturized, should deminiaturize.
    EXPECT_TRUE(deminimize_waiter.Wait());
  }

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  view->WaitForPaintCount(3);

  {
    views::test::PropertyWaiter deminimize_waiter(
        base::BindRepeating(&Widget::IsMinimized,
                            base::Unretained(widget.get())),
        false);
    widget->Restore();  // If not miniaturized, does nothing.
    EXPECT_TRUE(deminimize_waiter.Wait());
  }

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(3, view->paint_count());
}

// Tests that NativeWidgetMac::Show(ui::mojom::WindowShowState::kMinimized)
// minimizes the widget (previously it ordered its window out).
TEST_F(NativeWidgetMacTest, MinimizeByNativeShow) {
  WidgetAutoclosePtr widget(new Widget);
  Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(init_params));

  auto* view = widget->GetContentsView()->AddChildView(
      std::make_unique<PaintCountView>());
  WidgetChangeObserver observer(widget.get());

  widget->SetBounds(gfx::Rect(100, 100, 300, 300));

  EXPECT_TRUE(view->IsDrawn());
  EXPECT_EQ(0, view->paint_count());

  {
    views::test::PropertyWaiter visibility_waiter(
        base::BindRepeating(&Widget::IsVisible, base::Unretained(widget.get())),
        true);
    widget->Show();
    EXPECT_TRUE(visibility_waiter.Wait());
  }

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());

  {
    views::test::PropertyWaiter minimize_waiter(
        base::BindRepeating(&Widget::IsMinimized,
                            base::Unretained(widget.get())),
        true);

    NativeWidgetMac* native_widget =
        static_cast<views::NativeWidgetMac*>(widget->native_widget());
    gfx::Rect restore_bounds(100, 100, 300, 300);
    native_widget->Show(ui::mojom::WindowShowState::kMinimized, restore_bounds);

    EXPECT_TRUE(minimize_waiter.Wait());
  }

  EXPECT_TRUE(widget->IsMinimized());
}

TEST_F(NativeWidgetMacTest, MiniaturizeFramelessWindow) {
  // Create a widget without a minimize button.
  Widget* widget = CreateTopLevelFramelessPlatformWidget();
  NSWindow* ns_window = widget->GetNativeWindow().GetNativeNSWindow();
  widget->SetBounds(gfx::Rect(100, 100, 300, 300));
  widget->Show();
  EXPECT_FALSE(widget->IsMinimized());

  // This should fail, since performMiniaturize: requires a minimize button.
  [ns_window performMiniaturize:nil];
  EXPECT_FALSE(widget->IsMinimized());

  // But this should work.
  widget->Minimize();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(widget->IsMinimized());

  // Test closing while minimized.
  widget->CloseNow();
}

// Simple view for the SetCursor test that overrides View::GetCursor().
class CursorView : public View {
  METADATA_HEADER(CursorView, View)

 public:
  CursorView(int x, const ui::Cursor& cursor) : cursor_(cursor) {
    SetBounds(x, 0, 100, 300);
  }

  CursorView(const CursorView&) = delete;
  CursorView& operator=(const CursorView&) = delete;

  // View:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override { return cursor_; }

 private:
  ui::Cursor cursor_;
};

BEGIN_METADATA(CursorView)
END_METADATA

// Test for Widget::SetCursor(). There is no Widget::GetCursor(), so this uses
// -[NSCursor currentCursor] to validate expectations. Note that currentCursor
// is just "the top cursor on the application's cursor stack.", which is why it
// is safe to use this in a non-interactive UI test with the EventGenerator.
TEST_F(NativeWidgetMacTest, SetCursor) {
  NSCursor* arrow = [NSCursor arrowCursor];
  NSCursor* hand = [NSCursor pointingHandCursor];
  NSCursor* ibeam = [NSCursor IBeamCursor];

  Widget* widget = CreateTopLevelPlatformWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));
  auto* view_hand = widget->non_client_view()->frame_view()->AddChildView(
      std::make_unique<CursorView>(0, ui::mojom::CursorType::kHand));
  auto* view_ibeam = widget->non_client_view()->frame_view()->AddChildView(
      std::make_unique<CursorView>(100, ui::mojom::CursorType::kIBeam));
  widget->Show();
  NSWindow* widget_window = widget->GetNativeWindow().GetNativeNSWindow();

  // Events used to simulate tracking rectangle updates. These are not passed to
  // toolkit-views, so it only matters whether they are inside or outside the
  // content area.
  const gfx::Rect bounds = widget->GetWindowBoundsInScreen();
  NSEvent* event_in_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(bounds.x(), bounds.y()), NSEventTypeMouseMoved, 0);
  NSEvent* event_out_of_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(-50, -50), NSEventTypeMouseMoved, 0);

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
  event_generator.MoveMouseTo(view_hand->GetBoundsInScreen().CenterPoint());
  [widget_window cursorUpdate:event_in_content];
  EXPECT_EQ(hand, [NSCursor currentCursor]);

  // A tracking rectangle update not in the content area should forward to
  // the native NSWindow implementation, which sets the arrow cursor.
  [widget_window cursorUpdate:event_out_of_content];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  // Now move to the second view.
  event_generator.MoveMouseTo(view_ibeam->GetBoundsInScreen().CenterPoint());
  [widget_window cursorUpdate:event_in_content];
  EXPECT_EQ(ibeam, [NSCursor currentCursor]);

  // Moving to the third view (but remaining in the content area) should also
  // forward to the native NSWindow implementation.
  event_generator.MoveMouseTo(widget->non_client_view()
                                  ->frame_view()
                                  ->GetBoundsInScreen()
                                  .bottom_right());
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

  const std::u16string test_string = u"Green";
  views::Label* label = new views::Label(test_string);
  label->SetBounds(0, 0, 100, 100);
  widget->GetContentsView()->AddChildView(label);
  widget->Show();

  // Accessibility hit tests come in Cocoa screen coordinates.
  NSRect nsrect = gfx::ScreenRectToNSRect(screen_rect);
  NSPoint midpoint = NSMakePoint(NSMidX(nsrect), NSMidY(nsrect));

  id hit = [widget->GetNativeWindow().GetNativeNSWindow()
      accessibilityHitTest:midpoint];
  ASSERT_TRUE([hit conformsToProtocol:@protocol(NSAccessibility)]);
  id<NSAccessibility> ax_hit = hit;
  id title = ax_hit.accessibilityValue;
  EXPECT_NSEQ(title, @"Green");

  widget->CloseNow();
}

namespace {

Widget* AttachPopupToNativeParent(NSWindow* native_parent) {
  NSView* anchor_view =
      [[NSView alloc] initWithFrame:native_parent.contentView.bounds];
  [native_parent.contentView addSubview:anchor_view];

  // Note: Don't use WidgetTest::CreateChildPlatformWidget because that makes
  // windows of TYPE_CONTROL which need a parent Widget to obtain the focus
  // manager.
  Widget* child = new Widget;
  Widget::InitParams init_params(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  init_params.parent = anchor_view;
  init_params.child = true;
  init_params.type = Widget::InitParams::TYPE_POPUP;
  child->Init(std::move(init_params));
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
  EXPECT_FALSE(child->is_top_level());
  TestWidgetObserver child_observer(child);

  // GetTopLevelNativeWidget() will go up through |native_parent|'s Widget.
  internal::NativeWidgetPrivate* top_level_widget =
      internal::NativeWidgetPrivate::GetTopLevelNativeWidget(
          child->GetNativeView());
  EXPECT_EQ(Widget::GetWidgetForNativeWindow(native_parent),
            top_level_widget->GetWidget());
  EXPECT_NE(child, top_level_widget->GetWidget());

  // To verify the parent, we need to use NativeWidgetMac APIs.
  NativeWidgetMacNSWindowHost* bridged_native_widget_host =
      NativeWidgetMacNSWindowHost::GetFromNativeWindow(
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
  NativeWidgetMacTestWindow* __weak last_window_weak = nil;
  bool window_deallocated = false;
  @autoreleasepool {
    // First verify the behavior of CloseAllSecondaryWidgets in the normal case,
    // and how [NSApp windows] changes in response to Widget closure.
    Widget* widget = CreateWidgetWithTestWindow(
        Widget::InitParams(Widget::InitParams::TYPE_WINDOW), &last_window_weak);
    last_window_weak.deallocFlag = &window_deallocated;
    TestWidgetObserver observer(widget);
    EXPECT_TRUE([NSApp.windows containsObject:last_window_weak]);
    Widget::CloseAllSecondaryWidgets();
    EXPECT_TRUE(observer.widget_closed());
  }

  EXPECT_TRUE(window_deallocated);
  window_deallocated = false;

  NativeWidgetMacTestWindow* __strong last_window_strong = nil;
  @autoreleasepool {
    // Repeat, but now retain a reference and close the window before
    // CloseAllSecondaryWidgets().
    Widget* widget = CreateWidgetWithTestWindow(
        Widget::InitParams(Widget::InitParams::TYPE_WINDOW),
        &last_window_strong);
    last_window_strong.deallocFlag = &window_deallocated;
    TestWidgetObserver observer(widget);
    widget->CloseNow();
    EXPECT_TRUE(observer.widget_closed());
  }

  EXPECT_FALSE(window_deallocated);
  @autoreleasepool {
    Widget::CloseAllSecondaryWidgets();
    last_window_strong = nil;
  }
  EXPECT_TRUE(window_deallocated);

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
  NativeWidgetMacTestWindow* __weak native_parent = nil;
  @autoreleasepool {
    native_parent = MakeBorderlessNativeParent();
    native_parent.deallocFlag = &native_parent_dealloced;

    NativeWidgetMacTestWindow* window;
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_POPUP);
    init_params.parent = [native_parent contentView];
    init_params.bounds = gfx::Rect(0, 0, 100, 200);
    CreateWidgetWithTestWindow(std::move(init_params), &window);
    window.deallocFlag = &child_dealloced;
  }
  @autoreleasepool {
    // On 10.11, closing a weak reference on the parent window works, but older
    // versions of AppKit get upset if things are released inside -[NSWindow
    // close]. This test tries to establish a situation where the last reference
    // to the child window is released inside WidgetOwnerNSWindowAdapter::
    // OnWindowWillClose(). TODO(crbug.com/40208881): Is this still a
    // useful test? There is no such thing as "WidgetOwnerNSWindowAdapter" any
    // more.
    [native_parent close];
    native_parent = nil;
  }

  // As of macOS 13 (Ventura), it seems that exiting the autoreleasepool
  // block does not immediately trigger a release of its contents. Wait
  // here for the deallocations to occur before proceeding.
  while (!child_dealloced || !native_parent_dealloced) {
    [NativeWidgetMacTestWindow waitForDealloc];
  }

  EXPECT_TRUE(child_dealloced);
  EXPECT_TRUE(native_parent_dealloced);
}

// Tests visibility for a child of a native NSWindow, reshowing after a
// deminiaturize on the parent window (after attempting to show the child while
// the parent was miniaturized).
TEST_F(NativeWidgetMacTest, VisibleAfterNativeParentDeminiaturize) {
  NSWindow* native_parent = MakeBorderlessNativeParent();
  [native_parent makeKeyAndOrderFront:nil];

  WindowedNSNotificationObserver* miniaturizationObserver =
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidMiniaturizeNotification
                       object:native_parent];
  [native_parent miniaturize:nil];
  [miniaturizationObserver wait];
  Widget* child = AttachPopupToNativeParent(native_parent);

  child->Show();
  EXPECT_FALSE([native_parent isVisible]);
  EXPECT_FALSE(child->IsVisible());  // Parent is hidden so child is also.

  WindowedNSNotificationObserver* deminiaturizationObserver =
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidDeminiaturizeNotification
                       object:native_parent];
  [native_parent deminiaturize:nil];
  [deminiaturizationObserver wait];
  EXPECT_TRUE([native_parent isVisible]);
  // Don't WaitForVisibleCounts() here: deminiaturize is synchronous, so any
  // spurious _occlusion_ state change would have already occurred. Further
  // occlusion changes are not guaranteed to be triggered by the deminiaturize.
  EXPECT_TRUE(child->IsVisible());
  [native_parent close];
}

// Query the tooltip text that would be shown once the tooltip delay had
// elapsed.
std::u16string TooltipTextForWidget(Widget* widget) {
  // The actual location doesn't matter, since there is only one native view and
  // it fills the window. This just assumes the window is at least big enough
  // for a constant coordinate to be within it.
  NSPoint point = NSMakePoint(30, 30);
  NSView* view = [widget->GetNativeView().GetNativeNSView() hitTest:point];

  // Tool tips are vended by `NSViewToolTipOwner`s, which are registered with an
  // NSView by client code to provide a tooltip for a given point. Behind the
  // scenes, the NSView registers the `NSViewToolTipOwner`s with an
  // NSToolTipManager, which is completely private. This means that, in the
  // general case, an NSView cannot be queried for the tooltip that will be used
  // for any given situation.
  //
  // However, the ToolTipBaseView class is known to be its own
  // NSViewToolTipOwner, and that class is used as a base class for the relevant
  // views used. Therefore, if the class matches up, the tool tip can be
  // obtained that way. Do a hard cast, as this test utility function should
  // only be called on NSViews inheriting from that class.
  ToolTipBaseView* tool_tip_base_view =
      base::apple::ObjCCastStrict<ToolTipBaseView>(view);
  return base::SysNSStringToUTF16([tool_tip_base_view view:view
                                          stringForToolTip:0
                                                     point:point
                                                  userData:nullptr]);
}

// Tests tooltips. The test doesn't wait for tooltips to appear. That is, the
// test assumes Cocoa calls stringForToolTip: at appropriate times and that,
// when a tooltip is already visible, changing it causes an update. These were
// tested manually by inserting a base::RunLoop.Run().
TEST_F(NativeWidgetMacTest, Tooltips) {
  Widget* widget = CreateTopLevelPlatformWidget();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  const std::u16string tooltip_back = u"Back";
  const std::u16string tooltip_front = u"Front";
  const std::u16string long_tooltip(2000, 'W');

  // Create a nested layout to test corner cases.
  LabelButton* back = widget->non_client_view()->frame_view()->AddChildView(
      std::make_unique<LabelButton>());
  back->SetBounds(10, 10, 80, 80);
  widget->Show();

  ui::test::EventGenerator event_generator(GetContext(),
                                           widget->GetNativeWindow());

  // Initially, there should be no tooltip.
  const gfx::Rect widget_bounds = widget->GetClientAreaBoundsInScreen();
  event_generator.MoveMouseTo(widget_bounds.CenterPoint());
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());

  // Create a new button for the "front", and set the tooltip, but don't add it
  // to the view hierarchy yet.
  auto front_managed = std::make_unique<LabelButton>();
  front_managed->SetBounds(20, 20, 40, 40);
  front_managed->SetTooltipText(tooltip_front);

  // Changing the tooltip text shouldn't require an additional mousemove to take
  // effect.
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());
  back->SetTooltipText(tooltip_back);
  EXPECT_EQ(tooltip_back, TooltipTextForWidget(widget));

  // Adding a new view under the mouse should also take immediate effect.
  LabelButton* front = back->AddChildView(std::move(front_managed));
  EXPECT_EQ(tooltip_front, TooltipTextForWidget(widget));

  // A long tooltip will be wrapped by Cocoa, but the full string should appear.
  // Note that render widget hosts clip at 1024 to prevent DOS, but in toolkit-
  // views the UI is more trusted.
  front->SetTooltipText(long_tooltip);
  EXPECT_EQ(long_tooltip, TooltipTextForWidget(widget));

  // Move the mouse to a different view - tooltip should change.
  event_generator.MoveMouseTo(back->GetBoundsInScreen().origin());
  EXPECT_EQ(tooltip_back, TooltipTextForWidget(widget));

  // Move the mouse off of any view, tooltip should clear.
  event_generator.MoveMouseTo(widget_bounds.origin());
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

  const std::u16string tooltip_above = u"Front";
  CustomTooltipView* view_above = widget_above->GetContentsView()->AddChildView(
      std::make_unique<CustomTooltipView>(tooltip_above, nullptr));
  view_above->SetBoundsRect(widget_above->GetContentsView()->bounds());

  CustomTooltipView* view_below =
      widget_below->non_client_view()->frame_view()->AddChildView(
          std::make_unique<CustomTooltipView>(u"Back", view_above));
  view_below->SetBoundsRect(widget_below->GetContentsView()->bounds());

  widget_below->Show();
  widget_above->Show();

  // Move mouse above second widget and check that it returns tooltip
  // for second. Despite that event was handled in the first one.
  ui::test::EventGenerator event_generator(GetContext(),
                                           widget_below->GetNativeWindow());
  event_generator.MoveMouseTo(
      widget_above->GetWindowBoundsInScreen().CenterPoint());
  EXPECT_EQ(tooltip_above, TooltipTextForWidget(widget_below));

  widget_above->CloseNow();
  widget_below->CloseNow();
}

// Ensure captured mouse events correctly update dragging state in BaseView.
// Regression test for https://crbug.com/942452.
TEST_F(NativeWidgetMacTest, CapturedMouseUpClearsDrag) {
  MouseTrackingWidget* widget = new MouseTrackingWidget;
  Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW);
  widget->Init(std::move(init_params));

  NSWindow* window = widget->GetNativeWindow().GetNativeNSWindow();
  BridgedContentView* native_view = [window contentView];

  // Note: using native coordinates for consistency.
  [window setFrame:NSMakeRect(50, 50, 100, 100) display:YES animate:NO];
  NSEvent* enter_event = cocoa_test_event_utils::EnterEvent({50, 50}, window);
  NSEvent* exit_event = cocoa_test_event_utils::ExitEvent({200, 200}, window);

  widget->Show();
  EXPECT_EQ(0, widget->GetMouseEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(0, widget->GetMouseEventCount(ui::EventType::kMouseExited));

  [native_view mouseEntered:enter_event];
  EXPECT_EQ(1, widget->GetMouseEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(0, widget->GetMouseEventCount(ui::EventType::kMouseExited));

  [native_view mouseExited:exit_event];
  EXPECT_EQ(1, widget->GetMouseEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(1, widget->GetMouseEventCount(ui::EventType::kMouseExited));

  // Send a click. Note a click may initiate a drag, so the mouse-up is sent as
  // a captured event.
  NSArray<NSEvent*>* click =
      cocoa_test_event_utils::MouseClickInView(native_view, 1);
  [native_view mouseDown:click[0]];
  [native_view processCapturedMouseEvent:click[1]];

  // After a click, Enter/Exit should still work.
  [native_view mouseEntered:enter_event];
  EXPECT_EQ(2, widget->GetMouseEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(1, widget->GetMouseEventCount(ui::EventType::kMouseExited));

  [native_view mouseExited:exit_event];
  EXPECT_EQ(2, widget->GetMouseEventCount(ui::EventType::kMouseEntered));
  EXPECT_EQ(2, widget->GetMouseEventCount(ui::EventType::kMouseExited));

  widget->CloseNow();
}

namespace {

// TODO(ellyjones): Once DialogDelegate::CreateDialogWidget can accept a
// unique_ptr, return unique_ptr here.
DialogDelegateView* MakeModalDialog(ui::mojom::ModalType modal_type) {
  auto dialog = std::make_unique<DialogDelegateView>();
  dialog->SetModalType(modal_type);
  return dialog.release();
}

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

  ScopedSwizzleWaiter(const ScopedSwizzleWaiter&) = delete;
  ScopedSwizzleWaiter& operator=(const ScopedSwizzleWaiter&) = delete;

  ~ScopedSwizzleWaiter() { instance_ = nullptr; }

  static void OriginalSetWindowStateForEnd(id receiver, SEL method) {
    return instance_->CallMethodInternal(receiver, method);
  }

  void WaitForMethod() {
    if (method_called_)
      return;

    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  bool method_called() const { return method_called_; }

 private:
  void CallMethodInternal(id receiver, SEL selector) {
    DCHECK(!method_called_);
    method_called_ = true;
    if (run_loop_)
      run_loop_->Quit();
    swizzler_.InvokeOriginal<void>(receiver, selector);
  }

  static ScopedSwizzleWaiter* instance_;

  base::apple::ScopedObjCClassSwizzler swizzler_;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  bool method_called_ = false;
};

ScopedSwizzleWaiter* ScopedSwizzleWaiter::instance_ = nullptr;

// Shows a modal widget and waits for the show animation to complete. Waiting is
// not compulsory (calling Close() while animating the show will cancel the show
// animation). However, testing with overlapping swizzlers is tricky.
Widget* ShowChildModalWidgetAndWait(NSWindow* native_parent) {
  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      MakeModalDialog(ui::mojom::ModalType::kChild), nullptr,
      [native_parent contentView]);

  modal_dialog_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  EXPECT_FALSE(modal_dialog_widget->IsVisible());
  ScopedSwizzleWaiter show_waiter([ConstrainedWindowAnimationShow class]);

  EXPECT_FALSE(
      BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation());

  modal_dialog_widget->Show();
  // Visible immediately (although it animates from transparent).
  EXPECT_TRUE(modal_dialog_widget->IsVisible());
  NSAnimation* animation =
      BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation();
  EXPECT_TRUE(animation);
  EXPECT_TRUE([animation isAnimating]);

  // Run the animation.
  show_waiter.WaitForMethod();
  EXPECT_TRUE(modal_dialog_widget->IsVisible());
  EXPECT_TRUE(show_waiter.method_called());
  EXPECT_FALSE([animation isAnimating]);
  EXPECT_FALSE(
      BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation());
  return modal_dialog_widget;
}

// Shows a window-modal Widget (as a sheet). No need to wait since the native
// sheet animation is blocking.
Widget* ShowWindowModalWidget(NSWindow* native_parent) {
  Widget* sheet_widget = views::DialogDelegate::CreateDialogWidget(
      MakeModalDialog(ui::mojom::ModalType::kWindow), nullptr,
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

// Tests that the first call into SetVisibilityState() restores the window state
// for windows that start off miniaturized in the dock.
TEST_F(NativeWidgetMacTest, ConfirmMinimizedWindowRestoration) {
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.native_widget =
      CreatePlatformNativeWidgetImpl(widget, kStubCapture, nullptr);
  // Start the window off in the dock.
  params.show_state = ui::mojom::WindowShowState::kMinimized;
  params.workspace = kDummyWindowRestorationData;
  widget->Init(std::move(params));

  EXPECT_TRUE(BridgedNativeWidgetTestApi(widget).HasWindowRestorationData());

  // Show() ultimately invokes SetVisibilityState().
  widget->Show();

  EXPECT_FALSE(BridgedNativeWidgetTestApi(widget).HasWindowRestorationData());

  widget->CloseNow();
}

// Tests that the first call into SetVisibilityState() restores the window state
// for windows that start off visible.
TEST_F(NativeWidgetMacTest, ConfirmVisibleWindowRestoration) {
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.native_widget =
      CreatePlatformNativeWidgetImpl(widget, kStubCapture, nullptr);
  params.show_state = ui::mojom::WindowShowState::kNormal;
  params.workspace = kDummyWindowRestorationData;
  widget->Init(std::move(params));

  EXPECT_TRUE(BridgedNativeWidgetTestApi(widget).HasWindowRestorationData());

  // Show() ultimately invokes SetVisibilityState().
  widget->Show();

  EXPECT_FALSE(BridgedNativeWidgetTestApi(widget).HasWindowRestorationData());

  widget->CloseNow();
}

// Tests that calls to Hide() a Widget cancel any in-progress show animation,
// and that clients can control the triggering of the animation.
TEST_F(NativeWidgetMacTest, ShowAnimationControl) {
  NSWindow* native_parent = MakeBorderlessNativeParent();
  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      MakeModalDialog(ui::mojom::ModalType::kChild), nullptr,
      [native_parent contentView]);

  modal_dialog_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  EXPECT_FALSE(modal_dialog_widget->IsVisible());

  EXPECT_FALSE(
      BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation());
  modal_dialog_widget->Show();

  EXPECT_TRUE(modal_dialog_widget->IsVisible());
  NSAnimation* animation =
      BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation();
  EXPECT_TRUE(animation);
  EXPECT_TRUE([animation isAnimating]);

  // Hide without waiting for the animation to complete. Animation should cancel
  // and clear references from NativeWidgetNSWindowBridge.
  modal_dialog_widget->Hide();
  EXPECT_FALSE([animation isAnimating]);
  EXPECT_FALSE(
      BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation());
  animation = nil;

  // Disable animations and show again.
  modal_dialog_widget->SetVisibilityAnimationTransition(Widget::ANIMATE_NONE);
  modal_dialog_widget->Show();
  EXPECT_FALSE(BridgedNativeWidgetTestApi(modal_dialog_widget)
                   .show_animation());  // No animation this time.
  modal_dialog_widget->Hide();

  // Test after re-enabling.
  modal_dialog_widget->SetVisibilityAnimationTransition(Widget::ANIMATE_BOTH);
  modal_dialog_widget->Show();
  EXPECT_TRUE(BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation());
  animation = BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation();

  // Test whether disabling native animations also disables custom modal ones.
  modal_dialog_widget->SetVisibilityChangedAnimationsEnabled(false);
  modal_dialog_widget->Show();
  EXPECT_FALSE(BridgedNativeWidgetTestApi(modal_dialog_widget)
                   .show_animation());  // No animation this time.
  modal_dialog_widget->Hide();
  // Renable.
  modal_dialog_widget->SetVisibilityChangedAnimationsEnabled(true);
  modal_dialog_widget->Show();
  EXPECT_TRUE(BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation());
  animation = BridgedNativeWidgetTestApi(modal_dialog_widget).show_animation();

  // Closing should also cancel the animation.
  EXPECT_TRUE([animation isAnimating]);
  [native_parent close];
  EXPECT_FALSE([animation isAnimating]);
}

// Tests behavior of window-modal dialogs, displayed as sheets.
#if defined(ARCH_CPU_ARM64) || BUILDFLAG(IS_MAC)
// Bulk-disabled as part of arm64 bot stabilization: https://crbug.com/1154345
// Disabled on Mac 10.15 and 10.11 failing ([parent_close_button isEnabled])
// crbug.com/1473423
#define MAYBE_WindowModalSheet DISABLED_WindowModalSheet
#else
#define MAYBE_WindowModalSheet WindowModalSheet
#endif
TEST_F(NativeWidgetMacTest, MAYBE_WindowModalSheet) {
  NSWindow* native_parent = MakeClosableTitledNativeParent();

  Widget* sheet_widget = views::DialogDelegate::CreateDialogWidget(
      MakeModalDialog(ui::mojom::ModalType::kWindow), nullptr,
      [native_parent contentView]);

  WidgetChangeObserver widget_observer(sheet_widget);

  // Retain, to run checks after the Widget is torn down.
  NSWindow* sheet_window = sheet_widget->GetNativeWindow().GetNativeNSWindow();

  // Although there is no titlebar displayed, sheets need
  // NSWindowStyleMaskTitled in order to properly engage window-modal behavior
  // in AppKit.
  EXPECT_TRUE(NSWindowStyleMaskTitled & [sheet_window styleMask]);

  // But to properly size, sheets also need
  // NSWindowStyleMaskFullSizeContentView.
  EXPECT_TRUE(NSWindowStyleMaskFullSizeContentView & [sheet_window styleMask]);

  sheet_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  EXPECT_FALSE(sheet_widget->IsVisible());
  EXPECT_FALSE(sheet_widget->GetLayer()->IsVisible());

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
                EXPECT_TRUE(sheet_widget->GetLayer()->IsVisible());
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

  ASSERT_EQ(0U, native_parent.childWindows.count);

  // Modal, so the close button in the parent window should get disabled.
  EXPECT_FALSE([parent_close_button isEnabled]);

  // The sheet should be hidden and shown in step with the parent.
  widget_observer.WaitForVisibleCounts(1, 0);
  EXPECT_TRUE(sheet_widget->IsVisible());

  // TODO(tapted): Ideally [native_parent orderOut:nil] would also work here.
  // But it does not. AppKit's childWindow management breaks down after an
  // -orderOut: (see NativeWidgetNSWindowBridge::OnVisibilityChanged()). For
  // regular child windows, NativeWidgetNSWindowBridge fixes the behavior with
  // its own management. However, it can't do that for sheets without
  // encountering http://crbug.com/605098 and http://crbug.com/667602. -[NSApp
  // hide:] makes the NSWindow hidden in a different way, which does not break
  // like -orderOut: does. Which is good, because a user can always do -[NSApp
  // hide:], e.g., with Cmd+h, and that needs to work correctly.
  [NSApp hide:nil];

  widget_observer.WaitForVisibleCounts(1, 1);
  EXPECT_FALSE(sheet_widget->IsVisible());
  [native_parent makeKeyAndOrderFront:nil];
  ASSERT_EQ(0u, native_parent.childWindows.count);
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
  // attempt to invoke the completion handler of the sheet message. (If it did,
  // it would be fine.)
  {
    Widget* sheet_widget = ShowWindowModalWidget(native_parent);
    NSWindow* sheet_window =
        sheet_widget->GetNativeWindow().GetNativeNSWindow();
    EXPECT_TRUE([sheet_window isVisible]);

    WidgetChangeObserver widget_observer(sheet_widget);
    sheet_widget->Close();  // Asynchronous. Can't be called after -close.
    EXPECT_FALSE(widget_observer.widget_closed());
    [sheet_window close];
    EXPECT_TRUE(widget_observer.widget_closed());
    base::RunLoop().RunUntilIdle();

    // Pretend both tasks ran fully.
    [sheet_window.parentWindow endSheet:sheet_window];
  }

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
  @autoreleasepool {
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
  NativeWidgetMacNSWindowHost* window_host =
      NativeWidgetMacNSWindowHost::GetFromNativeWindow(
          dialog->GetNativeWindow());

  EXPECT_EQ(window_host->parent()->native_widget_mac()->GetNativeWindow(),
            parent);
  Widget::ReparentNativeView(dialog->GetNativeView(), [parent contentView]);
  EXPECT_EQ(window_host->parent()->native_widget_mac()->GetNativeWindow(),
            parent);

  [parent close];

  Widget* parent_widget = CreateTopLevelNativeWidget();
  parent = parent_widget->GetNativeWindow().GetNativeNSWindow();
  dialog = views::DialogDelegate::CreateDialogWidget(
      new DialogDelegateView, nullptr, [parent contentView]);
  window_host = NativeWidgetMacNSWindowHost::GetFromNativeWindow(
      dialog->GetNativeWindow());

  EXPECT_EQ(window_host->parent()->native_widget_mac()->GetNativeWindow(),
            parent);
  Widget::ReparentNativeView(dialog->GetNativeView(), [parent contentView]);
  EXPECT_EQ(window_host->parent()->native_widget_mac()->GetNativeWindow(),
            parent);

  parent_widget->CloseNow();
}

// Attaches a child window to |parent| that checks its parent's delegate is
// cleared when the child is destroyed. This assumes the child is destroyed via
// destruction of its parent.
class ParentCloseMonitor : public WidgetObserver {
 public:
  explicit ParentCloseMonitor(Widget* parent) {
    child_widget_ = std::make_unique<Widget>();
    widget_observation_.Observe(child_widget_.get());
    Widget::InitParams init_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    init_params.parent = parent->GetNativeView();
    init_params.bounds = gfx::Rect(100, 100, 100, 100);
    init_params.native_widget = CreatePlatformNativeWidgetImpl(
        child_widget_.get(), kStubCapture, nullptr);
    child_widget_->Init(std::move(init_params));
    child_widget_->Show();

    // NSWindow parent/child relationship should be established on Show() and
    // the parent should have a delegate. Retain the parent since it can't be
    // retrieved from the child while it is being destroyed.
    parent_nswindow_ =
        child_widget_->GetNativeWindow().GetNativeNSWindow().parentWindow;
    EXPECT_TRUE(parent_nswindow_);
    EXPECT_TRUE([parent_nswindow_ delegate]);
  }

  ParentCloseMonitor(const ParentCloseMonitor&) = delete;
  ParentCloseMonitor& operator=(const ParentCloseMonitor&) = delete;

  ~ParentCloseMonitor() override = default;

  void OnWidgetDestroying(Widget* child) override {
    // Upon a parent-triggered close, the NSWindow relationship will still exist
    // (it's removed just after OnWidgetDestroying() returns). The parent should
    // still be open (children are always closed first), but not have a delegate
    // (since it is being torn down).
    EXPECT_TRUE([child->GetNativeWindow().GetNativeNSWindow() parentWindow]);
    EXPECT_TRUE([parent_nswindow_ isVisible]);
    EXPECT_FALSE([parent_nswindow_ delegate]);

    EXPECT_FALSE(DidChildClose());
  }

  void OnWidgetDestroyed(Widget* child) override {
    EXPECT_FALSE([child->GetNativeWindow().GetNativeNSWindow() parentWindow]);
    EXPECT_TRUE([parent_nswindow_ isVisible]);
    EXPECT_FALSE([parent_nswindow_ delegate]);

    EXPECT_FALSE(DidChildClose());
    widget_observation_.Reset();
  }

  bool DidChildClose() const { return !widget_observation_.IsObserving(); }

 private:
  NSWindow* __strong parent_nswindow_;
  std::unique_ptr<Widget> child_widget_;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
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
    EXPECT_TRUE(monitor.DidChildClose());
  }

  // Test the Widget::CloseNow() flow.
  {
    Widget* parent = CreateTopLevelPlatformWidget();
    parent->SetBounds(gfx::Rect(100, 100, 300, 200));
    parent->Show();
    ParentCloseMonitor monitor(parent);
    parent->CloseNow();
    EXPECT_TRUE(monitor.DidChildClose());
  }

  // Test the WIDGET_OWNS_NATIVE_WIDGET flow.
  {
    auto parent = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(100, 100, 300, 200);
    parent->Init(std::move(params));
    parent->Show();

    ParentCloseMonitor monitor(parent.get());
    parent.reset();
    EXPECT_TRUE(monitor.DidChildClose());
  }

  // Test the CLIENT_OWNS_WIDGET flow.
  {
    auto parent = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(100, 100, 300, 200);
    parent->Init(std::move(params));
    parent->Show();

    ParentCloseMonitor monitor(parent.get());
    parent->CloseNow();
    EXPECT_TRUE(monitor.DidChildClose());
  }
}

// Tests Cocoa properties that should be given to particular widget types.
TEST_F(NativeWidgetMacTest, NativeProperties) {
  // Create a regular widget (TYPE_WINDOW).
  Widget* regular_widget = CreateTopLevelNativeWidget();
  EXPECT_TRUE([regular_widget->GetNativeWindow().GetNativeNSWindow()
                   canBecomeKeyWindow]);
  EXPECT_TRUE([regular_widget->GetNativeWindow().GetNativeNSWindow()
                   canBecomeMainWindow]);

  // Disabling activation should prevent key and main status.
  regular_widget->widget_delegate()->SetCanActivate(false);
  EXPECT_FALSE([regular_widget->GetNativeWindow().GetNativeNSWindow()
                    canBecomeKeyWindow]);
  EXPECT_FALSE([regular_widget->GetNativeWindow().GetNativeNSWindow()
                    canBecomeMainWindow]);

  // Create a dialog widget (also TYPE_WINDOW), but with a DialogDelegate.
  Widget* dialog_widget = views::DialogDelegate::CreateDialogWidget(
      MakeModalDialog(ui::mojom::ModalType::kChild), nullptr,
      regular_widget->GetNativeView());
  EXPECT_TRUE([dialog_widget->GetNativeWindow().GetNativeNSWindow()
                   canBecomeKeyWindow]);
  // Dialogs shouldn't take main status away from their parent.
  EXPECT_FALSE([dialog_widget->GetNativeWindow().GetNativeNSWindow()
                    canBecomeMainWindow]);

  // Create a bubble widget (with a parent): also shouldn't get main.
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

  regular_widget->CloseNow();
}

class CustomTitleWidgetDelegate : public WidgetDelegate {
 public:
  explicit CustomTitleWidgetDelegate(Widget* widget) : widget_(widget) {}

  CustomTitleWidgetDelegate(const CustomTitleWidgetDelegate&) = delete;
  CustomTitleWidgetDelegate& operator=(const CustomTitleWidgetDelegate&) =
      delete;

  void set_title(const std::u16string& title) { title_ = title; }
  void set_should_show_title(bool show) { should_show_title_ = show; }

  // WidgetDelegate:
  std::u16string GetWindowTitle() const override { return title_; }
  bool ShouldShowWindowTitle() const override { return should_show_title_; }
  Widget* GetWidget() override { return widget_; }
  const Widget* GetWidget() const override { return widget_; }

 private:
  raw_ptr<Widget> widget_;
  std::u16string title_;
  bool should_show_title_ = true;
};

// Test calls to invalidate the shadow when composited frames arrive.
TEST_F(NativeWidgetMacTest, InvalidateShadow) {
  NativeWidgetMacTestWindow* window;
  const gfx::Rect rect(0, 0, 100, 200);
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.bounds = rect;
  Widget* widget = CreateWidgetWithTestWindow(std::move(init_params), &window);

  // Simulate the initial paint.
  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(rect.size());

  // Default is an opaque window, so shadow doesn't need to be invalidated.
  EXPECT_EQ(0, [window invalidateShadowCount]);
  widget->CloseNow();

  init_params.opacity = Widget::InitParams::WindowOpacity::kTranslucent;
  widget = CreateWidgetWithTestWindow(std::move(init_params), &window);

  // First paint on a translucent window needs to invalidate the shadow. Once.
  EXPECT_EQ(0, [window invalidateShadowCount]);
  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(rect.size());
  EXPECT_EQ(1, [window invalidateShadowCount]);
  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(rect.size());
  EXPECT_EQ(1, [window invalidateShadowCount]);

  // Resizing the window also needs to trigger a shadow invalidation.
  [window setContentSize:NSMakeSize(123, 456)];
  // A "late" frame swap at the old size should do nothing.
  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(rect.size());
  EXPECT_EQ(1, [window invalidateShadowCount]);

  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(2, [window invalidateShadowCount]);
  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(2, [window invalidateShadowCount]);

  // Hiding the window does not require shadow invalidation.
  widget->Hide();
  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(2, [window invalidateShadowCount]);

  // Showing a translucent window after hiding it, should trigger shadow
  // invalidation.
  widget->Show();
  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(3, [window invalidateShadowCount]);

  widget->CloseNow();
}

// Test that the contentView opacity corresponds to the window type.
TEST_F(NativeWidgetMacTest, ContentOpacity) {
  NativeWidgetMacTestWindow* window;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);

  EXPECT_EQ(init_params.opacity, Widget::InitParams::WindowOpacity::kInferred);
  Widget* widget = CreateWidgetWithTestWindow(std::move(init_params), &window);

  // Infer should default to opaque on Mac.
  EXPECT_TRUE([[window contentView] isOpaque]);
  widget->CloseNow();

  init_params.opacity = Widget::InitParams::WindowOpacity::kTranslucent;
  widget = CreateWidgetWithTestWindow(std::move(init_params), &window);
  EXPECT_FALSE([[window contentView] isOpaque]);
  widget->CloseNow();

  // Test opaque explicitly.
  init_params.opacity = Widget::InitParams::WindowOpacity::kOpaque;
  widget = CreateWidgetWithTestWindow(std::move(init_params), &window);
  EXPECT_TRUE([[window contentView] isOpaque]);
  widget->CloseNow();
}

// Test the expected result of GetWorkAreaBoundsInScreen().
TEST_F(NativeWidgetMacTest, GetWorkAreaBoundsInScreen) {
  Widget widget;
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);

  // This is relative to the top-left of the primary screen, so unless the bot's
  // display is smaller than 400x300, the window will be wholly contained there.
  params.bounds = gfx::Rect(100, 100, 300, 200);
  widget.Init(std::move(params));
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

// Ensure traversing NSView focus correctly updates the views::FocusManager.
TEST_F(NativeWidgetMacTest, ChangeFocusOnChangeFirstResponder) {
  Widget* widget = CreateTopLevelPlatformWidget();
  widget->GetRootView()->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget->Show();

  NSView* child_view = [[FocusableTestNSView alloc]
      initWithFrame:[widget->GetNativeView().GetNativeNSView() bounds]];
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

// Test two kinds of widgets to re-parent.
TEST_F(NativeWidgetMacTest, ReparentNativeViewTypes) {
  auto toplevel1 = std::make_unique<Widget>();
  Widget::InitParams toplevel_params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
  toplevel1->Init(std::move(toplevel_params));
  toplevel1->Show();

  auto toplevel2 = std::make_unique<Widget>();
  toplevel_params = CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                 Widget::InitParams::TYPE_POPUP);
  toplevel2->Init(std::move(toplevel_params));
  toplevel2->Show();

  auto child = std::make_unique<Widget>();
  Widget::InitParams child_params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_CONTROL);
  child->Init(std::move(child_params));
  child->Show();

  Widget::ReparentNativeView(child->GetNativeView(),
                             toplevel1->GetNativeView());
  EXPECT_EQ([child->GetNativeWindow().GetNativeNSWindow() parentWindow],
            [toplevel1->GetNativeView().GetNativeNSView() window]);

  Widget::ReparentNativeView(child->GetNativeView(),
                             toplevel2->GetNativeView());
  EXPECT_EQ([child->GetNativeWindow().GetNativeNSWindow() parentWindow],
            [toplevel2->GetNativeView().GetNativeNSView() window]);

  Widget::ReparentNativeView(toplevel2->GetNativeView(),
                             toplevel1->GetNativeView());
  EXPECT_EQ([toplevel2->GetNativeWindow().GetNativeNSWindow() parentWindow],
            [toplevel1->GetNativeView().GetNativeNSView() window]);
}

// Test class for Full Keyboard Access related tests.
class NativeWidgetMacFullKeyboardAccessTest : public NativeWidgetMacTest {
 public:
  NativeWidgetMacFullKeyboardAccessTest() = default;

 protected:
  // testing::Test:
  void SetUp() override {
    NativeWidgetMacTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget();
    widget_->Show();
  }

  void TearDown() override {
    widget_.ExtractAsDangling()->CloseNow();
    NativeWidgetMacTest::TearDown();
  }

  remote_cocoa::NativeWidgetNSWindowBridge* bridge() {
    return NativeWidgetMacNSWindowHost::GetFromNativeWindow(
               widget_->GetNativeWindow())
        ->GetInProcessNSWindowBridge();
  }

  static ui::test::ScopedFakeFullKeyboardAccess* fake_full_keyboard_access() {
    return ui::test::ScopedFakeFullKeyboardAccess::GetInstance();
  }

  raw_ptr<Widget> widget_ = nullptr;
};

// Ensure that calling SetSize doesn't change the origin.
TEST_F(NativeWidgetMacTest, SetSizeDoesntChangeOrigin) {
  Widget* parent = CreateTopLevelFramelessPlatformWidget();
  gfx::Rect parent_rect(100, 100, 400, 200);
  parent->SetBounds(parent_rect);

  // Popup children specify their bounds relative to their parent window.
  Widget* child_control = new Widget;
  gfx::Rect child_control_rect(50, 70, 300, 100);
  {
    Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
    params.parent = parent->GetNativeView();
    params.bounds = child_control_rect;
    child_control->Init(std::move(params));
    child_control->SetContentsView(new View);
  }

  // Window children specify their bounds in screen coords.
  Widget* child_window = new Widget;
  gfx::Rect child_window_rect(110, 90, 200, 50);
  {
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
    params.parent = parent->GetNativeView();
    params.bounds = child_window_rect;
    child_window->Init(std::move(params));
  }

  // Sanity-check the initial bounds. Note that the CONTROL should be offset by
  // the parent's origin.
  EXPECT_EQ(parent->GetWindowBoundsInScreen(), parent_rect);
  EXPECT_EQ(
      child_control->GetWindowBoundsInScreen(),
      gfx::Rect(child_control_rect.origin() + parent_rect.OffsetFromOrigin(),
                child_control_rect.size()));
  EXPECT_EQ(child_window->GetWindowBoundsInScreen(), child_window_rect);

  // Update the size, but not the origin.
  parent_rect.set_size(gfx::Size(505, 310));
  parent->SetSize(parent_rect.size());
  child_control_rect.set_size(gfx::Size(256, 102));
  child_control->SetSize(child_control_rect.size());
  child_window_rect.set_size(gfx::Size(172, 96));
  child_window->SetSize(child_window_rect.size());

  // Ensure that the origin didn't change.
  EXPECT_EQ(parent->GetWindowBoundsInScreen(), parent_rect);
  EXPECT_EQ(
      child_control->GetWindowBoundsInScreen(),
      gfx::Rect(child_control_rect.origin() + parent_rect.OffsetFromOrigin(),
                child_control_rect.size()));
  EXPECT_EQ(child_window->GetWindowBoundsInScreen(), child_window_rect);

  parent->CloseNow();
}

// Tests that tooltip widgets get the correct accessibility role so that they're
// not announced as windows by VoiceOver.
TEST_F(NativeWidgetMacTest, AccessibilityRole) {
  {
    NativeWidgetMacTestWindow* window;

    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_WINDOW);
    Widget* widget =
        CreateWidgetWithTestWindow(std::move(init_params), &window);
    ASSERT_EQ([window accessibilityRole], NSAccessibilityWindowRole);
    widget->CloseNow();
  }
  {
    NativeWidgetMacTestWindow* window;

    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_TOOLTIP);
    Widget* widget =
        CreateWidgetWithTestWindow(std::move(init_params), &window);
    ASSERT_EQ([window accessibilityRole], NSAccessibilityHelpTagRole);
    widget->CloseNow();
  }
}

// Test that updateFullKeyboardAccess method on BridgedContentView correctly
// sets the keyboard accessibility mode on the associated focus manager.
TEST_F(NativeWidgetMacFullKeyboardAccessTest, FullKeyboardToggle) {
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());
  fake_full_keyboard_access()->set_full_keyboard_access_state(false);
  [bridge()->ns_view() updateFullKeyboardAccess];
  EXPECT_FALSE(widget_->GetFocusManager()->keyboard_accessible());
  fake_full_keyboard_access()->set_full_keyboard_access_state(true);
  [bridge()->ns_view() updateFullKeyboardAccess];
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());
}

// Test that a Widget's associated FocusManager is initialized with the correct
// keyboard accessibility value.
TEST_F(NativeWidgetMacFullKeyboardAccessTest, Initialization) {
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());

  fake_full_keyboard_access()->set_full_keyboard_access_state(false);
  Widget* widget2 = CreateTopLevelPlatformWidget();
  EXPECT_FALSE(widget2->GetFocusManager()->keyboard_accessible());
  widget2->CloseNow();
}

// Test that the correct keyboard accessibility mode is set when the window
// becomes active.
TEST_F(NativeWidgetMacFullKeyboardAccessTest, Activation) {
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());

  widget_->Hide();
  fake_full_keyboard_access()->set_full_keyboard_access_state(false);
  // [bridge()->ns_view() updateFullKeyboardAccess] is not explicitly called
  // since we may not receive full keyboard access toggle notifications when our
  // application is inactive.

  widget_->Show();
  EXPECT_FALSE(widget_->GetFocusManager()->keyboard_accessible());

  widget_->Hide();
  fake_full_keyboard_access()->set_full_keyboard_access_state(true);

  widget_->Show();
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());
}

class NativeWidgetMacViewsOrderTest : public WidgetTest {
 public:
  NativeWidgetMacViewsOrderTest()
      : widget_(nullptr), native_host_parent_(nullptr) {}

  NativeWidgetMacViewsOrderTest(const NativeWidgetMacViewsOrderTest&) = delete;
  NativeWidgetMacViewsOrderTest& operator=(
      const NativeWidgetMacViewsOrderTest&) = delete;

 protected:
  class NativeHostHolder {
   public:
    static std::unique_ptr<NativeHostHolder> CreateAndAddToParent(
        View* parent) {
      std::unique_ptr<NativeHostHolder> holder(new NativeHostHolder(
          parent->AddChildView(std::make_unique<NativeViewHost>())));
      holder->host()->Attach(holder->view());
      return holder;
    }

    NativeHostHolder(const NativeHostHolder&) = delete;
    NativeHostHolder& operator=(const NativeHostHolder&) = delete;

    NSView* view() const { return view_; }
    NativeViewHost* host() const { return host_; }

   private:
    explicit NativeHostHolder(NativeViewHost* host)
        : host_(host), view_([[NSView alloc] init]) {}

    const raw_ptr<NativeViewHost> host_;
    NSView* __strong view_;
  };

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget();

    starting_subviews_ =
        [widget_->GetNativeView().GetNativeNSView().subviews copy];

    native_host_parent_ = new View();
    widget_->GetContentsView()->AddChildView(native_host_parent_.get());

    const size_t kNativeViewCount = 3;
    for (size_t i = 0; i < kNativeViewCount; ++i) {
      hosts_.push_back(
          NativeHostHolder::CreateAndAddToParent(native_host_parent_));
    }
    EXPECT_EQ(kNativeViewCount, native_host_parent_->children().size());
    EXPECT_NSEQ([widget_->GetNativeView().GetNativeNSView() subviews],
                ([GetStartingSubviews() arrayByAddingObjectsFromArray:@[
                  hosts_[0]->view(), hosts_[1]->view(), hosts_[2]->view()
                ]]));
  }

  void TearDown() override {
    hosts_.clear();
    native_host_parent_ = nullptr;
    widget_.ExtractAsDangling()->CloseNow();
    WidgetTest::TearDown();
  }

  NSView* GetContentNativeView() {
    return widget_->GetNativeView().GetNativeNSView();
  }

  NSArray<NSView*>* GetStartingSubviews() { return starting_subviews_; }

  raw_ptr<Widget> widget_ = nullptr;
  raw_ptr<View> native_host_parent_ = nullptr;
  std::vector<std::unique_ptr<NativeHostHolder>> hosts_;
  NSArray<NSView*>* __strong starting_subviews_;
};

// Test that NativeViewHost::Attach()/Detach() method saves the NativeView
// z-order.
TEST_F(NativeWidgetMacViewsOrderTest, NativeViewAttached) {
  NativeHostHolder* second_host = hosts_[1].get();
  second_host->host()->Detach();
  EXPECT_NSEQ([GetContentNativeView() subviews],
              ([GetStartingSubviews() arrayByAddingObjectsFromArray:@[
                hosts_[0]->view(), hosts_[2]->view()
              ]]));

  second_host->host()->Attach(second_host->view());
  EXPECT_NSEQ([GetContentNativeView() subviews],
              ([GetStartingSubviews() arrayByAddingObjectsFromArray:@[
                hosts_[0]->view(), hosts_[1]->view(), hosts_[2]->view()
              ]]));
}

// Tests that NativeViews order changes according to views::View hierarchy.
TEST_F(NativeWidgetMacViewsOrderTest, ReorderViews) {
  native_host_parent_->ReorderChildView(hosts_[2]->host(), 1);
  EXPECT_NSEQ([GetContentNativeView() subviews],
              ([GetStartingSubviews() arrayByAddingObjectsFromArray:@[
                hosts_[0]->view(), hosts_[2]->view(), hosts_[1]->view()
              ]]));

  native_host_parent_->RemoveChildView(hosts_[2]->host());
  EXPECT_NSEQ([GetContentNativeView() subviews],
              ([GetStartingSubviews() arrayByAddingObjectsFromArray:@[
                hosts_[0]->view(), hosts_[1]->view()
              ]]));

  View* new_parent = new View();
  native_host_parent_->RemoveChildView(hosts_[1]->host());
  native_host_parent_->AddChildView(new_parent);
  new_parent->AddChildView(hosts_[1]->host());
  new_parent->AddChildView(hosts_[2]->host());
  EXPECT_NSEQ([GetContentNativeView() subviews],
              ([GetStartingSubviews() arrayByAddingObjectsFromArray:@[
                hosts_[0]->view(), hosts_[1]->view(), hosts_[2]->view()
              ]]));

  native_host_parent_->ReorderChildView(new_parent, 0);
  EXPECT_NSEQ([GetContentNativeView() subviews],
              ([GetStartingSubviews() arrayByAddingObjectsFromArray:@[
                hosts_[1]->view(), hosts_[2]->view(), hosts_[0]->view()
              ]]));
}

// Test that unassociated native views stay on top after reordering.
TEST_F(NativeWidgetMacViewsOrderTest, UnassociatedViewsIsAbove) {
  NSView* child_view = [[NSView alloc] init];
  [GetContentNativeView() addSubview:child_view];
  EXPECT_NSEQ(
      [GetContentNativeView() subviews],
      ([GetStartingSubviews() arrayByAddingObjectsFromArray:@[
        hosts_[0]->view(), hosts_[1]->view(), hosts_[2]->view(), child_view
      ]]));

  native_host_parent_->ReorderChildView(hosts_[2]->host(), 1);
  EXPECT_NSEQ(
      [GetContentNativeView() subviews],
      ([GetStartingSubviews() arrayByAddingObjectsFromArray:@[
        hosts_[0]->view(), hosts_[2]->view(), hosts_[1]->view(), child_view
      ]]));
}

namespace {

// Returns an array of NSTouchBarItemIdentifier (i.e. NSString), extracted from
// the principal of |view|'s touch bar, which must be a NSGroupTouchBarItem.
// Also verifies that the touch bar's delegate returns non-nil for all items.
NSArray* ExtractTouchBarGroupIdentifiers(NSView* view) {
  NSArray* result = nil;
  NSTouchBar* touch_bar = [view touchBar];
  NSTouchBarItemIdentifier principal = [touch_bar principalItemIdentifier];
  EXPECT_TRUE(principal);
  NSGroupTouchBarItem* group = base::apple::ObjCCastStrict<NSGroupTouchBarItem>(
      [[touch_bar delegate] touchBar:touch_bar
               makeItemForIdentifier:principal]);
  EXPECT_TRUE(group);
  NSTouchBar* nested_touch_bar = [group groupTouchBar];
  result = [nested_touch_bar itemIdentifiers];

  for (NSTouchBarItemIdentifier item in result) {
    EXPECT_TRUE([[touch_bar delegate] touchBar:nested_touch_bar
                         makeItemForIdentifier:item]);
  }

  return result;
}

}  // namespace

// Test TouchBar integration.
TEST_F(NativeWidgetMacTest, TouchBar) {
  DialogDelegate* delegate = MakeModalDialog(ui::mojom::ModalType::kNone);
  views::DialogDelegate::CreateDialogWidget(delegate, nullptr, nullptr);
  NSView* content =
      [delegate->GetWidget()->GetNativeWindow().GetNativeNSWindow()
              contentView];

  // Constants from bridged_content_view_touch_bar.mm.
  NSString* const kTouchBarOKId = @"com.google.chrome-OK";
  NSString* const kTouchBarCancelId = @"com.google.chrome-CANCEL";

  EXPECT_TRUE(content);
  EXPECT_TRUE(delegate->GetOkButton());
  EXPECT_TRUE(delegate->GetCancelButton());

  NSTouchBar* touch_bar = [content touchBar];
  EXPECT_TRUE([touch_bar delegate]);
  EXPECT_TRUE([[touch_bar delegate] touchBar:touch_bar
                       makeItemForIdentifier:kTouchBarOKId]);
  EXPECT_TRUE([[touch_bar delegate] touchBar:touch_bar
                       makeItemForIdentifier:kTouchBarCancelId]);

  NSString* principal = [touch_bar principalItemIdentifier];
  EXPECT_NSEQ(@"com.google.chrome-DIALOG-BUTTONS-GROUP", principal);
  EXPECT_NSEQ((@[ kTouchBarCancelId, kTouchBarOKId ]),
              ExtractTouchBarGroupIdentifiers(content));

  // Ensure the touchBar is recreated by comparing pointers.

  // Remove the cancel button.
  delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  delegate->DialogModelChanged();
  EXPECT_TRUE(delegate->GetOkButton());
  EXPECT_FALSE(delegate->GetCancelButton());

  NSTouchBar* new_touch_bar = [content touchBar];
  EXPECT_NSNE(touch_bar, new_touch_bar);
  EXPECT_NSEQ((@[ kTouchBarOKId ]), ExtractTouchBarGroupIdentifiers(content));

  delegate->GetWidget()->CloseNow();
}

TEST_F(NativeWidgetMacTest, InitCallback) {
  NativeWidget* observed_native_widget = nullptr;
  const auto callback = base::BindRepeating(
      [](NativeWidget** observed, NativeWidgetMac* native_widget) {
        *observed = native_widget;
      },
      &observed_native_widget);
  auto subscription =
      NativeWidgetMac::RegisterInitNativeWidgetCallback(callback);

  Widget* widget_a = CreateTopLevelPlatformWidget();
  EXPECT_EQ(observed_native_widget, widget_a->native_widget());
  Widget* widget_b = CreateTopLevelPlatformWidget();
  EXPECT_EQ(observed_native_widget, widget_b->native_widget());

  subscription = {};
  observed_native_widget = nullptr;
  Widget* widget_c = CreateTopLevelPlatformWidget();
  // The original callback from above should no longer be firing.
  EXPECT_EQ(observed_native_widget, nullptr);

  widget_a->CloseNow();
  widget_b->CloseNow();
  widget_c->CloseNow();
}

TEST_F(NativeWidgetMacTest, FocusManagerChangeOnReparentNativeView) {
  WidgetAutoclosePtr toplevel(CreateTopLevelPlatformWidget());
  Widget* child = CreateChildPlatformWidget(toplevel->GetNativeView());
  WidgetAutoclosePtr target_toplevel(CreateTopLevelPlatformWidget());
  EXPECT_EQ(child->GetFocusManager(), toplevel->GetFocusManager());
  EXPECT_NE(child->GetFocusManager(), target_toplevel->GetFocusManager());
  NativeWidgetMac* child_native_widget =
      static_cast<NativeWidgetMac*>(child->native_widget());
  EXPECT_EQ(GetFocusManager(child_native_widget), child->GetFocusManager());

  Widget::ReparentNativeView(child->GetNativeView(),
                             target_toplevel->GetNativeView());
  EXPECT_EQ(child->GetFocusManager(), target_toplevel->GetFocusManager());
  EXPECT_NE(child->GetFocusManager(), toplevel->GetFocusManager());
  EXPECT_EQ(GetFocusManager(child_native_widget), child->GetFocusManager());
}

TEST_F(NativeWidgetMacTest,
       CorrectZOrderForMenuTypeWhenParamsZOrderHasNoValue) {
  NativeWidgetMacTestWindow* parent_window;
  NativeWidgetMacTestWindow* child_window;

  Widget::InitParams init_params_parent =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  Widget* parent =
      CreateWidgetWithTestWindow(std::move(init_params_parent), &parent_window);

  Widget::InitParams init_params_child =
      CreateParams(Widget::InitParams::TYPE_MENU);
  init_params_child.parent = parent->GetNativeView();
  Widget* child =
      CreateWidgetWithTestWindow(std::move(init_params_child), &child_window);

  EXPECT_NE(nil, child_window.parentWindow);
  EXPECT_EQ(parent_window, child_window.parentWindow);

  parent->Show();
  child->Show();

  // Ensure that the child widget has kFloatingWindow z_order, when
  // params.z_order is not specified for a widget of menu type.
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow, child->GetZOrderLevel());

  parent->CloseNow();
}

}  // namespace views::test

@implementation TestStopAnimationWaiter
- (void)setWindowStateForEnd {
  views::test::ScopedSwizzleWaiter::OriginalSetWindowStateForEnd(self, _cmd);
}
@end

@implementation NativeWidgetMacTestWindow

@synthesize invalidateShadowCount = _invalidateShadowCount;
@synthesize fakeOnInactiveSpace = _fakeOnInactiveSpace;
@synthesize deallocFlag = _deallocFlag;

+ (base::RunLoop**)runLoop {
  static base::RunLoop* runLoop = nullptr;
  return &runLoop;
}

// Returns once the NativeWidgetMacTestWindow's -dealloc method has been
// called.
+ (void)waitForDealloc {
  base::RunLoop runLoop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, runLoop.QuitClosure(), TestTimeouts::action_timeout());
  (*[NativeWidgetMacTestWindow runLoop]) = &runLoop;
  runLoop.Run();
  (*[NativeWidgetMacTestWindow runLoop]) = nullptr;
}

- (void)dealloc {
  if (_deallocFlag) {
    DCHECK(!*_deallocFlag);
    *_deallocFlag = true;
    if (*[NativeWidgetMacTestWindow runLoop]) {
      (*[NativeWidgetMacTestWindow runLoop])->Quit();
    }
  }
}

- (void)invalidateShadow {
  ++_invalidateShadowCount;
  [super invalidateShadow];
}

- (BOOL)isOnActiveSpace {
  return !_fakeOnInactiveSpace;
}

@end

@implementation MockBridgedView

@synthesize drawRectCount = _drawRectCount;
@synthesize lastDirtyRect = _lastDirtyRect;

- (void)drawRect:(NSRect)dirtyRect {
  ++_drawRectCount;
  _lastDirtyRect = dirtyRect;
}

@end

@implementation FocusableTestNSView
- (BOOL)acceptsFirstResponder {
  return YES;
}
@end
