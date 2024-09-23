// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/mac/mac_util.h"
#import "testing/gtest_mac.h"
#import "ui/base/cocoa/views_hostable.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/native/native_view_host_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class TestViewsHostable : public ui::ViewsHostableView {
 public:
  id parent_accessibility_element() const {
    return parent_accessibility_element_;
  }

 private:
  // ui::ViewsHostableView:
  void ViewsHostableAttach(ui::ViewsHostableView::Host* host) override {
  }
  void ViewsHostableDetach() override { parent_accessibility_element_ = nil; }
  void ViewsHostableSetBounds(const gfx::Rect& bounds_in_window) override {}
  void ViewsHostableSetVisible(bool visible) override {}
  void ViewsHostableMakeFirstResponder() override {}
  void ViewsHostableSetParentAccessible(
      gfx::NativeViewAccessible parent_accessibility_element) override {
    parent_accessibility_element_ = parent_accessibility_element;
  }
  gfx::NativeViewAccessible ViewsHostableGetParentAccessible() override {
    return parent_accessibility_element_;
  }
  gfx::NativeViewAccessible ViewsHostableGetAccessibilityElement() override {
    return nil;
  }

  id parent_accessibility_element_ = nil;
};

@interface TestViewsHostableView : NSView<ViewsHostable>
@property(nonatomic, assign) ui::ViewsHostableView* viewsHostableView;
@end
@implementation TestViewsHostableView
@synthesize viewsHostableView = _viewsHostableView;
@end

namespace views {

class NativeViewHostMacTest : public test::NativeViewHostTestBase {
 public:
  NativeViewHostMacTest() = default;

  NativeViewHostMacTest(const NativeViewHostMacTest&) = delete;
  NativeViewHostMacTest& operator=(const NativeViewHostMacTest&) = delete;

  // testing::Test:
  void TearDown() override {
    // On Aura, the compositor is destroyed when the WindowTreeHost provided by
    // AuraTestHelper is destroyed. On Mac, the Widget is the host, so it must
    // be closed before the ContextFactory is torn down by ViewsTestBase.
    DestroyTopLevel();
    NativeViewHostTestBase::TearDown();
  }

  NativeViewHostMac* native_host() {
    return static_cast<NativeViewHostMac*>(GetNativeWrapper());
  }

  void CreateHost() {
    CreateTopLevel();
    CreateTestingHost();
    native_view_ = [[NSView alloc] initWithFrame:NSZeroRect];

    // Verify the expectation that the NativeViewHostWrapper is only created
    // after the NativeViewHost is added to a widget.
    EXPECT_FALSE(native_host());
    toplevel()->GetRootView()->AddChildView(host());
    EXPECT_TRUE(native_host());

    host()->Attach(native_view_);
  }

  NSView* GetMovedContentViewForWidget(const std::unique_ptr<Widget>& widget) {
    return (__bridge NSView*)widget->GetNativeWindowProperty(
        views::NativeWidgetMacNSWindowHost::kMovedContentNSView);
  }

 protected:
  NSView* __strong native_view_;
};

// Test destroying the top level widget before destroying the NativeViewHost.
// On Mac, also ensure that the native view is removed from its superview when
// the Widget containing its host is destroyed.
TEST_F(NativeViewHostMacTest, DestroyWidget) {
  ResetHostDestroyedCount();
  CreateHost();
  ReleaseHost();
  EXPECT_EQ(0, host_destroyed_count());
  EXPECT_TRUE([native_view_ superview]);
  DestroyTopLevel();
  EXPECT_FALSE([native_view_ superview]);
  EXPECT_EQ(1, host_destroyed_count());
}

// Ensure the native view receives the correct bounds when it is attached. On
// Mac, the bounds of the native view is relative to the NSWindow it is in, not
// the screen, and the coordinates have to be flipped.
TEST_F(NativeViewHostMacTest, Attach) {
  CreateHost();
  EXPECT_TRUE([native_view_ superview]);
  EXPECT_TRUE([native_view_ window]);

  host()->Detach();

  [native_view_ setFrame:NSZeroRect];
  toplevel()->SetBounds(gfx::Rect(64, 48, 100, 200));
  host()->SetBounds(10, 10, 80, 60);

  EXPECT_FALSE([native_view_ superview]);
  EXPECT_FALSE([native_view_ window]);
  EXPECT_NSEQ(NSZeroRect, [native_view_ frame]);

  host()->Attach(native_view_);
  EXPECT_TRUE([native_view_ superview]);
  EXPECT_TRUE([native_view_ window]);

  // Layout is normally async, trigger it now to ensure bounds have been
  // applied.
  host()->DeprecatedLayoutImmediately();
  // Expect the top-left to be 10 pixels below the titlebar.
  int bottom = toplevel()->GetClientAreaBoundsInScreen().height() - 10 - 60;
  EXPECT_NSEQ(NSMakeRect(10, bottom, 80, 60), [native_view_ frame]);

  DestroyHost();
}

// If Widget A has been attached to Widget B, ensure Widget A maintains a
// reference to its native view.
TEST_F(NativeViewHostMacTest, CheckNativeViewReferenceOnAttach) {
  CreateTopLevel();
  CreateTestingHost();
  toplevel()->GetRootView()->AddChildView(host());

  // Create a second widget.
  auto second_widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.delegate = nullptr;
  second_widget->Init(std::move(params));

  // No reference to its native view should exist currently.
  EXPECT_EQ(GetMovedContentViewForWidget(second_widget), nullptr);

  NSView* view = second_widget->GetNativeView().GetNativeNSView();
  NSWindow* native_window = [view window];
  host()->Attach(second_widget->GetNativeView());

  // On Ventura, the attach rips Widget A's contentView from its window.
  // NativeViewHostMac::AttachNativeView() should have stored a reference.
  if (base::mac::MacOSMajorVersion() >= 13) {
    EXPECT_EQ([native_window contentView], nullptr);
    EXPECT_EQ(GetMovedContentViewForWidget(second_widget), view);
  } else {
    EXPECT_EQ([native_window contentView], view);
  }

  // After detaching, there should be no reference, and the native view should
  // be restored to its widget's window.
  host()->Detach();
  EXPECT_EQ(GetMovedContentViewForWidget(second_widget), nullptr);
  EXPECT_EQ([native_window contentView], view);

  DestroyHost();
}

// On macOS13, if Widget A has been attached to Widget B, ensure Widget A's
// reference to its native view disappears when the native view is freed.
TEST_F(NativeViewHostMacTest, CheckNoNativeViewReferenceOnDestruct) {
  if (base::mac::MacOSMajorVersion() < 13) {
    return;
  }

  CreateTopLevel();
  CreateTestingHost();
  toplevel()->GetRootView()->AddChildView(host());

  // Create a second widget.
  auto second_widget = std::make_unique<Widget>();
  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_WINDOW);
  params.delegate = nullptr;
  second_widget->Init(std::move(params));

  // No reference should to the native view should exist currently.
  EXPECT_EQ(GetMovedContentViewForWidget(second_widget), nullptr);

  // Attaching the widget's native view should store a reference.
  NSView* view = second_widget->GetNativeView().GetNativeNSView();
  host()->Attach(second_widget->GetNativeView());
  EXPECT_EQ(GetMovedContentViewForWidget(second_widget), view);

  // Tearing down the destination widget (where we attached the second widget)
  // will free the native view and should remove the reference.
  DestroyHost();

  EXPECT_EQ(GetMovedContentViewForWidget(second_widget), nullptr);
}

// Ensure the native view is integrated into the views accessibility
// hierarchy if the native view conforms to the AccessibilityParent
// protocol.
TEST_F(NativeViewHostMacTest, AccessibilityParent) {
  CreateHost();
  host()->Detach();

  TestViewsHostableView* view = [[TestViewsHostableView alloc] init];
  TestViewsHostable views_hostable;
  [view setViewsHostableView:&views_hostable];

  host()->Attach(view);
  EXPECT_NSEQ(views_hostable.parent_accessibility_element(),
              toplevel()->GetRootView()->GetNativeViewAccessible());

  host()->Detach();
  DestroyHost();
  EXPECT_FALSE(views_hostable.parent_accessibility_element());
}

// Test that the content windows' bounds are set to the correct values while the
// native size is equal or not equal to the View size.
TEST_F(NativeViewHostMacTest, ContentViewPositionAndSize) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(0, 0, 100, 100));

  native_host()->ShowWidget(5, 10, 100, 100, 200, 200);
  EXPECT_NSEQ(NSMakeRect(5, -38, 100, 100), native_view_.frame);

  native_host()->ShowWidget(10, 25, 50, 50, 50, 50);
  EXPECT_NSEQ(NSMakeRect(10, -3, 50, 50), native_view_.frame);

  DestroyHost();
}

// Ensure the native view is hidden along with its host, and when detaching, or
// when attaching to a host that is already hidden.
TEST_F(NativeViewHostMacTest, NativeViewHidden) {
  CreateHost();
  toplevel()->SetBounds(gfx::Rect(0, 0, 100, 100));
  host()->SetBounds(10, 10, 80, 60);

  EXPECT_FALSE([native_view_ isHidden]);
  host()->SetVisible(false);
  EXPECT_TRUE([native_view_ isHidden]);
  host()->SetVisible(true);
  EXPECT_FALSE([native_view_ isHidden]);

  host()->Detach();
  EXPECT_TRUE([native_view_ isHidden]);  // Hidden when detached.
  [native_view_ setHidden:NO];

  host()->SetVisible(false);
  EXPECT_FALSE([native_view_ isHidden]);  // Stays visible.
  host()->Attach(native_view_);
  EXPECT_TRUE([native_view_ isHidden]);  // Hidden when attached.

  host()->Detach();
  [native_view_ setHidden:YES];
  host()->SetVisible(true);
  EXPECT_TRUE([native_view_ isHidden]);  // Stays hidden.
  host()->Attach(native_view_);
  // Layout updates visibility, and is normally async, trigger it now to ensure
  // visibility updated.
  host()->DeprecatedLayoutImmediately();
  EXPECT_FALSE([native_view_ isHidden]);  // Made visible when attached.

  EXPECT_TRUE([native_view_ superview]);
  toplevel()->GetRootView()->RemoveChildView(host());
  EXPECT_TRUE([native_view_ isHidden]);  // Hidden when removed from Widget.
  EXPECT_FALSE([native_view_ superview]);

  toplevel()->GetRootView()->AddChildView(host());
  EXPECT_FALSE([native_view_ isHidden]);  // And visible when added.
  EXPECT_TRUE([native_view_ superview]);

  DestroyHost();
}

// Check that we can destroy cleanly even if the native view has already been
// released.
TEST_F(NativeViewHostMacTest, NativeViewReleased) {
  @autoreleasepool {
    CreateHost();
    // In practice the native view is a WebContentsViewCocoa which is retained
    // by its superview (a TabContentsContainerView) and by WebContentsViewMac.
    // It's possible for both of them to be destroyed without calling
    // NativeHostView::Detach().
    [native_view_ removeFromSuperview];
    native_view_ = nil;
  }

  // During teardown, NativeViewDetaching() is called in RemovedFromWidget().
  // Just trigger it with Detach().
  host()->Detach();

  DestroyHost();
}

}  // namespace views
