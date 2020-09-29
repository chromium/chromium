// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_topmost_window_finder.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/stl_util.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/test/x11_property_change_waiter.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/shape.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_path.h"
#include "ui/gfx/x/xproto.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#include "ui/views/widget/widget.h"

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views {

namespace {

// Waits till |window| is minimized.
class MinimizeWaiter : public ui::X11PropertyChangeWaiter {
 public:
  explicit MinimizeWaiter(x11::Window window)
      : ui::X11PropertyChangeWaiter(window, "_NET_WM_STATE") {}

  ~MinimizeWaiter() override = default;

 private:
  // ui::X11PropertyChangeWaiter:
  bool ShouldKeepOnWaiting(x11::Event* event) override {
    std::vector<x11::Atom> wm_states;
    if (ui::GetAtomArrayProperty(xwindow(), "_NET_WM_STATE", &wm_states)) {
      return !base::Contains(wm_states, gfx::GetAtom("_NET_WM_STATE_HIDDEN"));
    }
    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(MinimizeWaiter);
};

// Waits till |_NET_CLIENT_LIST_STACKING| is updated to include
// |expected_windows|.
class StackingClientListWaiter : public ui::X11PropertyChangeWaiter {
 public:
  StackingClientListWaiter(x11::Window* expected_windows, size_t count)
      : ui::X11PropertyChangeWaiter(ui::GetX11RootWindow(),
                                    "_NET_CLIENT_LIST_STACKING"),
        expected_windows_(expected_windows, expected_windows + count) {}

  ~StackingClientListWaiter() override = default;

  // X11PropertyChangeWaiter:
  void Wait() override {
    // StackingClientListWaiter may be created after
    // _NET_CLIENT_LIST_STACKING already contains |expected_windows|.
    if (!ShouldKeepOnWaiting(nullptr))
      return;

    ui::X11PropertyChangeWaiter::Wait();
  }

 private:
  // ui::X11PropertyChangeWaiter:
  bool ShouldKeepOnWaiting(x11::Event* event) override {
    std::vector<x11::Window> stack;
    ui::GetXWindowStack(ui::GetX11RootWindow(), &stack);
    return !std::all_of(
        expected_windows_.cbegin(), expected_windows_.cend(),
        [&stack](x11::Window window) { return base::Contains(stack, window); });
  }

  std::vector<x11::Window> expected_windows_;

  DISALLOW_COPY_AND_ASSIGN(StackingClientListWaiter);
};

void IconifyWindow(x11::Connection* connection, x11::Window window) {
  ui::SendClientMessage(window, ui::GetX11RootWindow(),
                        gfx::GetAtom("WM_CHANGE_STATE"),
                        {ui::WM_STATE_ICONIC, 0, 0, 0, 0});
}

}  // namespace

class X11TopmostWindowFinderTest : public test::DesktopWidgetTestInteractive {
 public:
  X11TopmostWindowFinderTest() = default;
  ~X11TopmostWindowFinderTest() override = default;

  // DesktopWidgetTestInteractive
  void SetUp() override {
#if defined(USE_OZONE)
    // Run tests only for X11 (ozone or not Ozone).
    if (features::IsUsingOzonePlatform() &&
        std::strcmp(ui::OzonePlatform::GetInstance()->GetPlatformName(),
                    "x11") != 0) {
      // SetUp still is required to be run. Otherwise, ViewsTestBase CHECKs in
      // the dtor.
      DesktopWidgetTestInteractive::SetUp();
      GTEST_SKIP();
    }
#endif
    // Make X11 synchronous for our display connection. This does not force the
    // window manager to behave synchronously.
    XSynchronize(xdisplay(), true);
    DesktopWidgetTestInteractive::SetUp();
  }

  void TearDown() override {
    if (!IsSkipped())
      XSynchronize(xdisplay(), false);
    DesktopWidgetTestInteractive::TearDown();
  }

  // Creates and shows a Widget with |bounds|. The caller takes ownership of
  // the returned widget.
  std::unique_ptr<Widget> CreateAndShowWidget(const gfx::Rect& bounds) {
    std::unique_ptr<Widget> toplevel(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.native_widget = new DesktopNativeWidgetAura(toplevel.get());
    params.bounds = bounds;
    params.remove_standard_frame = true;
    toplevel->Init(std::move(params));
    toplevel->Show();
    return toplevel;
  }

  // Creates and shows an X window with |bounds|.
  x11::Window CreateAndShowXWindow(const gfx::Rect& bounds) {
    x11::Window root = ui::GetX11RootWindow();
    auto window = connection()->GenerateId<x11::Window>();
    connection()->CreateWindow({
        .wid = window,
        .parent = root,
        .width = 1,
        .height = 1,
    });

    ui::SetUseOSWindowFrame(window, false);
    ShowAndSetXWindowBounds(window, bounds);
    return window;
  }

  // Shows |window| and sets its bounds.
  void ShowAndSetXWindowBounds(x11::Window window, const gfx::Rect& bounds) {
    connection()->MapWindow({window});

    connection()->ConfigureWindow({
        .window = window,
        .x = bounds.x(),
        .y = bounds.y(),
        .width = bounds.width(),
        .height = bounds.height(),
    });
  }

  x11::Connection* connection() { return x11::Connection::Get(); }
  Display* xdisplay() { return gfx::GetXDisplay(); }

  // Returns the topmost X window at the passed in screen position.
  x11::Window FindTopmostXWindowAt(int screen_x, int screen_y) {
    ui::X11TopmostWindowFinder finder;
    return finder.FindWindowAt(gfx::Point(screen_x, screen_y));
  }

  // Returns the topmost aura::Window at the passed in screen position. Returns
  // NULL if the topmost window does not have an associated aura::Window.
  aura::Window* FindTopmostLocalProcessWindowAt(int screen_x, int screen_y) {
    ui::X11TopmostWindowFinder finder;
    auto widget = static_cast<gfx::AcceleratedWidget>(
        finder.FindLocalProcessWindowAt(gfx::Point(screen_x, screen_y), {}));
    return widget != gfx::kNullAcceleratedWidget
               ? DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
                     widget)
               : nullptr;
  }

  // Returns the topmost aura::Window at the passed in screen position ignoring
  // |ignore_window|. Returns NULL if the topmost window does not have an
  // associated aura::Window.
  aura::Window* FindTopmostLocalProcessWindowWithIgnore(
      int screen_x,
      int screen_y,
      aura::Window* ignore_window) {
    std::set<gfx::AcceleratedWidget> ignore;
    ignore.insert(ignore_window->GetHost()->GetAcceleratedWidget());
    ui::X11TopmostWindowFinder finder;
    auto widget =
        static_cast<gfx::AcceleratedWidget>(finder.FindLocalProcessWindowAt(
            gfx::Point(screen_x, screen_y), ignore));
    return widget != gfx::kNullAcceleratedWidget
               ? DesktopWindowTreeHostPlatform::GetContentWindowForWidget(
                     widget)
               : nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(X11TopmostWindowFinderTest);
};

TEST_F(X11TopmostWindowFinderTest, Basic) {
  // Avoid positioning test windows at 0x0 because window managers often have a
  // panel/launcher along one of the screen edges and do not allow windows to
  // position themselves to overlap the panel/launcher.
  std::unique_ptr<Widget> widget1(
      CreateAndShowWidget(gfx::Rect(100, 100, 200, 100)));
  aura::Window* window1 = widget1->GetNativeWindow();
  x11::Window x11_window1 =
      static_cast<x11::Window>(window1->GetHost()->GetAcceleratedWidget());

  x11::Window x11_window2 = CreateAndShowXWindow(gfx::Rect(200, 100, 100, 200));

  std::unique_ptr<Widget> widget3(
      CreateAndShowWidget(gfx::Rect(100, 190, 200, 110)));
  aura::Window* window3 = widget3->GetNativeWindow();
  x11::Window x11_window3 =
      static_cast<x11::Window>(window3->GetHost()->GetAcceleratedWidget());

  x11::Window windows[] = {x11_window1, x11_window2, x11_window3};
  StackingClientListWaiter waiter(windows, base::size(windows));
  waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_EQ(x11_window1, FindTopmostXWindowAt(150, 150));
  EXPECT_EQ(window1, FindTopmostLocalProcessWindowAt(150, 150));

  EXPECT_EQ(x11_window2, FindTopmostXWindowAt(250, 150));
  EXPECT_FALSE(FindTopmostLocalProcessWindowAt(250, 150));

  EXPECT_EQ(x11_window3, FindTopmostXWindowAt(250, 250));
  EXPECT_EQ(window3, FindTopmostLocalProcessWindowAt(250, 250));

  EXPECT_EQ(x11_window3, FindTopmostXWindowAt(150, 250));
  EXPECT_EQ(window3, FindTopmostLocalProcessWindowAt(150, 250));

  EXPECT_EQ(x11_window3, FindTopmostXWindowAt(150, 195));
  EXPECT_EQ(window3, FindTopmostLocalProcessWindowAt(150, 195));

  EXPECT_NE(x11_window1, FindTopmostXWindowAt(1000, 1000));
  EXPECT_NE(x11_window2, FindTopmostXWindowAt(1000, 1000));
  EXPECT_NE(x11_window3, FindTopmostXWindowAt(1000, 1000));
  EXPECT_FALSE(FindTopmostLocalProcessWindowAt(1000, 1000));

  EXPECT_EQ(window1,
            FindTopmostLocalProcessWindowWithIgnore(150, 150, window3));
  EXPECT_FALSE(FindTopmostLocalProcessWindowWithIgnore(250, 250, window3));
  EXPECT_FALSE(FindTopmostLocalProcessWindowWithIgnore(150, 250, window3));
  EXPECT_EQ(window1,
            FindTopmostLocalProcessWindowWithIgnore(150, 195, window3));

  connection()->DestroyWindow({x11_window2});
}

// Test that the minimized state is properly handled.
TEST_F(X11TopmostWindowFinderTest, Minimized) {
  std::unique_ptr<Widget> widget1(
      CreateAndShowWidget(gfx::Rect(100, 100, 100, 100)));
  aura::Window* window1 = widget1->GetNativeWindow();
  x11::Window x11_window1 =
      static_cast<x11::Window>(window1->GetHost()->GetAcceleratedWidget());
  x11::Window x11_window2 = CreateAndShowXWindow(gfx::Rect(300, 100, 100, 100));

  x11::Window windows[] = {x11_window1, x11_window2};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_EQ(x11_window1, FindTopmostXWindowAt(150, 150));
  {
    MinimizeWaiter minimize_waiter(x11_window1);
    IconifyWindow(connection(), x11_window1);
    minimize_waiter.Wait();
  }
  EXPECT_NE(x11_window1, FindTopmostXWindowAt(150, 150));
  EXPECT_NE(x11_window2, FindTopmostXWindowAt(150, 150));

  // Repeat test for an X window which does not belong to a views::Widget
  // because the code path is different.
  EXPECT_EQ(x11_window2, FindTopmostXWindowAt(350, 150));
  {
    MinimizeWaiter minimize_waiter(x11_window2);
    IconifyWindow(connection(), x11_window2);
    minimize_waiter.Wait();
  }
  EXPECT_NE(x11_window1, FindTopmostXWindowAt(350, 150));
  EXPECT_NE(x11_window2, FindTopmostXWindowAt(350, 150));

  connection()->DestroyWindow({x11_window2});
}

// Test that non-rectangular windows are properly handled.
TEST_F(X11TopmostWindowFinderTest, NonRectangular) {
  if (!ui::IsShapeExtensionAvailable())
    return;

  std::unique_ptr<Widget> widget1(
      CreateAndShowWidget(gfx::Rect(100, 100, 100, 100)));
  x11::Window window1 = static_cast<x11::Window>(
      widget1->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  auto shape1 = std::make_unique<Widget::ShapeRects>();
  shape1->emplace_back(0, 10, 10, 90);
  shape1->emplace_back(10, 0, 90, 100);
  widget1->SetShape(std::move(shape1));

  SkRegion skregion2;
  skregion2.op(SkIRect::MakeXYWH(0, 10, 10, 90), SkRegion::kUnion_Op);
  skregion2.op(SkIRect::MakeXYWH(10, 0, 90, 100), SkRegion::kUnion_Op);
  x11::Window window2 = CreateAndShowXWindow(gfx::Rect(300, 100, 100, 100));
  auto region2 = gfx::CreateRegionFromSkRegion(skregion2);
  x11::Connection::Get()->shape().Rectangles({
      .operation = x11::Shape::So::Set,
      .destination_kind = x11::Shape::Sk::Bounding,
      .ordering = x11::ClipOrdering::YXBanded,
      .destination_window = window2,
      .rectangles = *region2,
  });
  x11::Window windows[] = {window1, window2};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_EQ(window1, FindTopmostXWindowAt(105, 120));
  EXPECT_NE(window1, FindTopmostXWindowAt(105, 105));
  EXPECT_NE(window2, FindTopmostXWindowAt(105, 105));

  // Repeat test for an X window which does not belong to a views::Widget
  // because the code path is different.
  EXPECT_EQ(window2, FindTopmostXWindowAt(305, 120));
  EXPECT_NE(window1, FindTopmostXWindowAt(305, 105));
  EXPECT_NE(window2, FindTopmostXWindowAt(305, 105));

  connection()->DestroyWindow({window2});
}

// Test that a window with an empty shape are properly handled.
TEST_F(X11TopmostWindowFinderTest, NonRectangularEmptyShape) {
  if (!ui::IsShapeExtensionAvailable())
    return;

  std::unique_ptr<Widget> widget1(
      CreateAndShowWidget(gfx::Rect(100, 100, 100, 100)));
  x11::Window window1 = static_cast<x11::Window>(
      widget1->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  auto shape1 = std::make_unique<Widget::ShapeRects>();
  shape1->emplace_back();
  // Widget takes ownership of |shape1|.
  widget1->SetShape(std::move(shape1));

  x11::Window windows[] = {window1};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_NE(window1, FindTopmostXWindowAt(105, 105));
}

// Test that setting a Null shape removes the shape.
TEST_F(X11TopmostWindowFinderTest, NonRectangularNullShape) {
  if (!ui::IsShapeExtensionAvailable())
    return;

  std::unique_ptr<Widget> widget1(
      CreateAndShowWidget(gfx::Rect(100, 100, 100, 100)));
  x11::Window window1 = static_cast<x11::Window>(
      widget1->GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  auto shape1 = std::make_unique<Widget::ShapeRects>();
  shape1->emplace_back();
  widget1->SetShape(std::move(shape1));

  // Remove the shape - this is now just a normal window.
  widget1->SetShape(nullptr);

  x11::Window windows[] = {window1};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_EQ(window1, FindTopmostXWindowAt(105, 105));
}

// Test that the TopmostWindowFinder finds windows which belong to menus
// (which may or may not belong to Chrome).
//
// Flakes (https://crbug.com/955316)
TEST_F(X11TopmostWindowFinderTest, DISABLED_Menu) {
  x11::Window window = CreateAndShowXWindow(gfx::Rect(100, 100, 100, 100));

  x11::Window root = ui::GetX11RootWindow();
  auto menu_window = connection()->GenerateId<x11::Window>();
  connection()->CreateWindow({
      .wid = menu_window,
      .parent = root,
      .width = 1,
      .height = 1,
      .c_class = x11::WindowClass::CopyFromParent,
      .override_redirect = x11::Bool32(true),
  });

  ui::SetAtomProperty(menu_window, "_NET_WM_WINDOW_TYPE", "ATOM",
                      gfx::GetAtom("_NET_WM_WINDOW_TYPE_MENU"));

  ui::SetUseOSWindowFrame(menu_window, false);
  ShowAndSetXWindowBounds(menu_window, gfx::Rect(140, 110, 100, 100));
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  // |menu_window| is never added to _NET_CLIENT_LIST_STACKING.
  x11::Window windows[] = {window};
  StackingClientListWaiter stack_waiter(windows, base::size(windows));
  stack_waiter.Wait();

  EXPECT_EQ(window, FindTopmostXWindowAt(110, 110));
  EXPECT_EQ(menu_window, FindTopmostXWindowAt(150, 120));
  EXPECT_EQ(menu_window, FindTopmostXWindowAt(210, 120));

  connection()->DestroyWindow({window});
  connection()->DestroyWindow({menu_window});
}

}  // namespace views
