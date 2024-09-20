// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_TEST_H_
#define UI_VIEWS_TEST_WIDGET_TEST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

#if (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) || \
    BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)

#include "ui/display/screen.h"
#endif

namespace ui {
class EventSink;
class ImeKeyEventDispatcher;
}  // namespace ui

namespace views {

class View;
namespace internal {

class RootView;

}  // namespace internal

namespace test {

// These functions return an arbitrary view v such that:
//
// 1) v is a descendant of the root view of the provided widget, and
// 2) predicate.Run(v) returned true
//
// They are *not* guaranteed to return first child matching the predicate for
// any specific ordering of the children. In fact, these methods deliberately
// randomly choose a child to return, so make sure your predicate matches
// *only* the view you want!
using ViewPredicate = base::RepeatingCallback<bool(const View*)>;
View* AnyViewMatchingPredicate(View* root, const ViewPredicate& predicate);
template <typename Pred>
View* AnyViewMatchingPredicate(View* root, Pred predicate) {
  return AnyViewMatchingPredicate(root, base::BindLambdaForTesting(predicate));
}
View* AnyViewMatchingPredicate(Widget* widget, const ViewPredicate& predicate);
template <typename Pred>
View* AnyViewMatchingPredicate(Widget* widget, Pred predicate) {
  return AnyViewMatchingPredicate(widget,
                                  base::BindLambdaForTesting(predicate));
}

View* AnyViewWithClassName(Widget* widget, const std::string& classname);

class WidgetTest : public ViewsTestBase {
 public:
  WidgetTest();
  explicit WidgetTest(
      std::unique_ptr<base::test::TaskEnvironment> task_environment);

  template <typename... TaskEnvironmentTraits>
  explicit WidgetTest(TaskEnvironmentTraits&&... traits)
      : ViewsTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}

  WidgetTest(const WidgetTest&) = delete;
  WidgetTest& operator=(const WidgetTest&) = delete;

  ~WidgetTest() override;

  // TODO(crbug.com/40232479): Once work on the referenced bug is complete,
  // update the following functions to return a std::unique_ptr<Widget> and
  // remove the ownership parameter.
  //
  // Create Widgets with |native_widget| in InitParams set to an instance of
  // platform specific widget type that has stubbled capture calls. This will
  // create a non-desktop widget.
  Widget* CreateTopLevelPlatformWidget(
      Widget::InitParams::Ownership ownership =
          Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  Widget* CreateTopLevelFramelessPlatformWidget(
      Widget::InitParams::Ownership ownership =
          Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  Widget* CreateChildPlatformWidget(
      gfx::NativeView parent_native_view,
      Widget::InitParams::Ownership ownership =
          Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
  // Create Widgets with |native_widget| in InitParams set to an instance of
  // platform specific widget type that has stubbled capture calls. This will
  // create a desktop widget.
  Widget* CreateTopLevelPlatformDesktopWidget(
      Widget::InitParams::Ownership ownership =
          Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
#endif

  // Create Widgets initialized without a |native_widget| set in InitParams.
  // Depending on the test environment, ViewsDelegate::OnBeforeWidgetInit() may
  // provide a desktop or non-desktop NativeWidget.
  Widget* CreateTopLevelNativeWidget(
      Widget::InitParams::Ownership ownership =
          Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  Widget* CreateChildNativeWidgetWithParent(
      Widget* parent,
      Widget::InitParams::Ownership ownership =
          Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);

  View* GetMousePressedHandler(views::internal::RootView* root_view);

  View* GetMouseMoveHandler(views::internal::RootView* root_view);

  View* GetGestureHandler(views::internal::RootView* root_view);

  // Simulate an activation of the native window held by |widget|, as if it was
  // clicked by the user. This is a synchronous method for use in
  // non-interactive tests that do not spin a RunLoop in the test body (since
  // that may cause real focus changes to flakily manifest).
  static void SimulateNativeActivate(Widget* widget);

  // Return true if |window| is visible according to the native platform.
  static bool IsNativeWindowVisible(gfx::NativeWindow window);

  // Return true if |above| is higher than |below| in the native window Z-order.
  // Both windows must be visible.
  // WARNING: This does not work for Aura desktop widgets (crbug.com/1333445).
  static bool IsWindowStackedAbove(Widget* above, Widget* below);

  // Query the native window system for the minimum size configured for user
  // initiated window resizes.
  gfx::Size GetNativeWidgetMinimumContentSize(Widget* widget);

  // Return the event sink for |widget|. On aura platforms, this is an
  // aura::WindowEventDispatcher. Otherwise, it is a bridge to the OS event
  // sink.
  static ui::EventSink* GetEventSink(Widget* widget);

  // Get the ImeKeyEventDispatcher, for setting on a Mock InputMethod in tests.
  static ui::ImeKeyEventDispatcher* GetImeKeyEventDispatcherForWidget(
      Widget* widget);

  // Return true if |window| is transparent according to the native platform.
  static bool IsNativeWindowTransparent(gfx::NativeWindow window);

  // Returns whether |widget| has a Window shadow managed in this process. That
  // is, a shadow that is drawn outside of the Widget bounds, and managed by the
  // WindowManager.
  static bool WidgetHasInProcessShadow(Widget* widget);

  // Returns the set of all Widgets that currently have a NativeWindow.
  static Widget::Widgets GetAllWidgets();

  // Waits for system app activation events, if any, to have happened. This is
  // necessary on macOS 10.15+, where the system will attempt to find and
  // activate a window owned by the app shortly after app startup, if there is
  // one. See https://crbug.com/998868 for details.
  static void WaitForSystemAppActivation();
};

class DesktopWidgetTest : public WidgetTest {
 public:
  DesktopWidgetTest();

  DesktopWidgetTest(const DesktopWidgetTest&) = delete;
  DesktopWidgetTest& operator=(const DesktopWidgetTest&) = delete;

  ~DesktopWidgetTest() override;

  // WidgetTest:
  void SetUp() override;
};

class DesktopWidgetTestInteractive : public DesktopWidgetTest {
 public:
  DesktopWidgetTestInteractive();

  DesktopWidgetTestInteractive(const DesktopWidgetTestInteractive&) = delete;
  DesktopWidgetTestInteractive& operator=(const DesktopWidgetTestInteractive&) =
      delete;

  ~DesktopWidgetTestInteractive() override;

  // DesktopWidgetTest
  void SetUp() override;

#if (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) || \
    BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
  void TearDown() override;
  std::unique_ptr<display::Screen> screen_;
#endif
};

// A helper WidgetDelegate for tests that require hooks into WidgetDelegate
// calls, and removes some of the boilerplate for initializing a Widget. Calls
// Widget::CloseNow() when destroyed if it hasn't already been done.
class TestDesktopWidgetDelegate : public WidgetDelegate {
 public:
  TestDesktopWidgetDelegate();
  explicit TestDesktopWidgetDelegate(Widget* widget);

  TestDesktopWidgetDelegate(const TestDesktopWidgetDelegate&) = delete;
  TestDesktopWidgetDelegate& operator=(const TestDesktopWidgetDelegate&) =
      delete;

  ~TestDesktopWidgetDelegate() override;

  // Initialize the Widget, adding some meaningful default InitParams.
  void InitWidget(Widget::InitParams init_params);

  // Set the contents view to be used during Widget initialization. For Widgets
  // that use non-client views, this will be the contents_view used to
  // initialize the ClientView in WidgetDelegate::CreateClientView(). Otherwise,
  // it is the ContentsView of the Widget's RootView. Ownership passes to the
  // view hierarchy during InitWidget().
  void set_contents_view(View* contents_view) {
    contents_view_ = contents_view;
  }
  // Sets the return value for CloseRequested().
  void set_can_close(bool can_close) { can_close_ = can_close; }

  int window_closing_count() const { return window_closing_count_; }
  const gfx::Rect& initial_bounds() { return initial_bounds_; }
  Widget::ClosedReason last_closed_reason() const {
    return last_closed_reason_;
  }
  bool can_close() const { return can_close_; }

  // WidgetDelegate:
  void WindowClosing() override;
  Widget* GetWidget() override;
  const Widget* GetWidget() const override;
  View* GetContentsView() override;
  bool OnCloseRequested(Widget::ClosedReason close_reason) override;

 private:
  std::unique_ptr<Widget> owned_widget_;
  raw_ptr<Widget> widget_;
  raw_ptr<View> contents_view_ = nullptr;
  int window_closing_count_ = 0;
  gfx::Rect initial_bounds_ = gfx::Rect(100, 100, 200, 200);
  bool can_close_ = true;
  Widget::ClosedReason last_closed_reason_ = Widget::ClosedReason::kUnspecified;
};

// Testing widget delegate that creates a widget with a single view, which is
// the initially focused view.
class TestInitialFocusWidgetDelegate : public TestDesktopWidgetDelegate {
 public:
  explicit TestInitialFocusWidgetDelegate(gfx::NativeWindow context);

  TestInitialFocusWidgetDelegate(const TestInitialFocusWidgetDelegate&) =
      delete;
  TestInitialFocusWidgetDelegate& operator=(
      const TestInitialFocusWidgetDelegate&) = delete;

  ~TestInitialFocusWidgetDelegate() override;

  View* view() { return view_; }

  // WidgetDelegate override:
  View* GetInitiallyFocusedView() override;

 private:
  raw_ptr<View> view_;
};

// Use in tests to wait for a widget to be destroyed.
class WidgetDestroyedWaiter : public WidgetObserver {
 public:
  explicit WidgetDestroyedWaiter(Widget* widget);

  WidgetDestroyedWaiter(const WidgetDestroyedWaiter&) = delete;
  WidgetDestroyedWaiter& operator=(const WidgetDestroyedWaiter&) = delete;

  ~WidgetDestroyedWaiter() override;

  // Wait for the widget to be destroyed, or return immediately if it was
  // already destroyed since this object was created.
  void Wait();

 private:
  // views::WidgetObserver
  void OnWidgetDestroyed(Widget* widget) override;

  base::RunLoop run_loop_;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

// Helper class to wait for a Widget to become visible. This will add a failure
// to the currently-running test if the widget is destroyed before becoming
// visible.
class WidgetVisibleWaiter : public WidgetObserver {
 public:
  explicit WidgetVisibleWaiter(Widget* widget);
  WidgetVisibleWaiter(const WidgetVisibleWaiter&) = delete;
  WidgetVisibleWaiter& operator=(const WidgetVisibleWaiter&) = delete;
  ~WidgetVisibleWaiter() override;

  // Waits for the widget to become visible.
  void Wait();

 private:
  // WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override;
  void OnWidgetDestroying(Widget* widget) override;

  base::RunLoop run_loop_;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_TEST_H_
