// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_topmost_window_finder.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/x/test/x11_property_change_waiter.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/shape.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_path.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/ozone/platform/x11/x11_window.h"
#include "ui/ozone/platform/x11/x11_window_manager.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace ui {

namespace {

// Waits until the |x11_window| is mapped and becomes viewable.
class X11VisibilityWaiter : public x11::EventObserver {
 public:
  X11VisibilityWaiter() = default;
  X11VisibilityWaiter(const X11VisibilityWaiter&) = delete;
  X11VisibilityWaiter& operator=(const X11VisibilityWaiter&) = delete;
  ~X11VisibilityWaiter() override = default;

  void WaitUntilWindowIsVisible(x11::Window x11_window) {
    if (IsWindowVisible(x11_window))
      return;

    event_selector_ = std::make_unique<x11::XScopedEventSelector>(
        x11_window,
        x11::EventMask::StructureNotify | x11::EventMask::SubstructureNotify);
    x11_window_ = x11_window;

    x11::Connection::Get()->AddEventObserver(this);

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    DCHECK(IsWindowVisible(x11_window));
  }

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override {
    auto* map = event.As<x11::MapNotifyEvent>();
    if (map && map->window == x11_window_) {
      DCHECK(!quit_closure_.is_null());
      std::move(quit_closure_).Run();
      x11::Connection::Get()->RemoveEventObserver(this);
    }
  }

  x11::Window x11_window_;
  std::unique_ptr<x11::XScopedEventSelector> event_selector_;
  // Ends the run loop.
  base::OnceClosure quit_closure_;
};

class TestPlatformWindowDelegate : public PlatformWindowDelegate {
 public:
  TestPlatformWindowDelegate() = default;
  TestPlatformWindowDelegate(const TestPlatformWindowDelegate&) = delete;
  TestPlatformWindowDelegate& operator=(const TestPlatformWindowDelegate&) =
      delete;
  ~TestPlatformWindowDelegate() override = default;

  PlatformWindowState state() { return state_; }

  // PlatformWindowDelegate:
  void OnBoundsChanged(const BoundsChange& change) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(PlatformWindowState old_state,
                            PlatformWindowState new_state) override {
    state_ = new_state;
  }
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    widget_ = widget;
  }
  void OnWillDestroyAcceleratedWidget() override {}
  void OnAcceleratedWidgetDestroyed() override {
    widget_ = gfx::kNullAcceleratedWidget;
  }
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}

 private:
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  PlatformWindowState state_ = PlatformWindowState::kUnknown;
};

// Waits till |window| is minimized.
class MinimizeWaiter : public X11PropertyChangeWaiter {
 public:
  explicit MinimizeWaiter(x11::Window window)
      : X11PropertyChangeWaiter(window, "_NET_WM_STATE") {}

  MinimizeWaiter(const MinimizeWaiter&) = delete;
  MinimizeWaiter& operator=(const MinimizeWaiter&) = delete;

  ~MinimizeWaiter() override = default;

 private:
  // X11PropertyChangeWaiter:
  bool ShouldKeepOnWaiting() override {
    std::vector<x11::Atom> wm_states;
    if (GetArrayProperty(xwindow(), x11::GetAtom("_NET_WM_STATE"),
                         &wm_states)) {
      return !base::Contains(wm_states, x11::GetAtom("_NET_WM_STATE_HIDDEN"));
    }
    return true;
  }
};

void IconifyWindow(x11::Connection* connection, x11::Window window) {
  SendClientMessage(window, GetX11RootWindow(), x11::GetAtom("WM_CHANGE_STATE"),
                    {WM_STATE_ICONIC, 0, 0, 0, 0});
}

}  // namespace

class X11TopmostWindowFinderTest : public testing::Test {
 public:
  X11TopmostWindowFinderTest()
      : task_env_(base::test::TaskEnvironment::MainThreadType::UI) {}

  X11TopmostWindowFinderTest(const X11TopmostWindowFinderTest&) = delete;
  X11TopmostWindowFinderTest& operator=(const X11TopmostWindowFinderTest&) =
      delete;

  ~X11TopmostWindowFinderTest() override = default;

  // testing::Test:
  void SetUp() override {
    // Run tests only for X11.
    if (OzonePlatform::GetPlatformNameForTest() != "x11")
      GTEST_SKIP();

    auto* connection = x11::Connection::Get();
    // Not initialized when runs on CrOS builds.
    if (!X11EventSource::GetInstance())
      event_source_ = std::make_unique<X11EventSource>(connection);
    // Make X11 synchronous for our display connection. This does not force the
    // window manager to behave synchronously.
    connection->SynchronizeForTest(true);
    testing::Test::SetUp();
  }

  void TearDown() override {
    if (!IsSkipped())
      connection()->SynchronizeForTest(false);
  }

  // Creates and shows an X11Window with |bounds|. The caller takes ownership of
  // the returned window.
  std::unique_ptr<X11Window> CreateAndShowX11Window(
      PlatformWindowDelegate* delegate,
      const gfx::Rect& bounds) {
    PlatformWindowInitProperties init_params(bounds);
    init_params.type = PlatformWindowType::kWindow;
    init_params.remove_standard_frame = true;
    auto window = std::make_unique<X11Window>(delegate);
    window->Initialize(std::move(init_params));
    window->Show(true);
    // The window must have a title. Otherwise, the X11TopmostWindowFinder
    // refuses to use this window.
    window->SetTitle(u"X11FinderTest");

    // Wait until the window becomes visible so that window finder doesn't skip
    // these windows (it's required to wait because mapping and searching for
    // toplevel window is a subject to races).
    X11VisibilityWaiter waiter;
    waiter.WaitUntilWindowIsVisible(
        static_cast<x11::Window>(window->GetWidget()));
    return window;
  }

  // Creates and shows an X window with |bounds|.
  x11::Window CreateAndShowXWindow(const gfx::Rect& bounds) {
    x11::Window root = GetX11RootWindow();
    auto window = connection()->GenerateId<x11::Window>();
    connection()->CreateWindow({
        .wid = window,
        .parent = root,
        .width = 1,
        .height = 1,
    });

    // This is necessary because X11TopmostWindowFinder skips over unnamed
    // windows.
    SetStringProperty(window, x11::Atom::WM_NAME, x11::Atom::STRING, "");

    SetUseOSWindowFrame(window, false);
    ShowAndSetXWindowBounds(window, bounds);

    // Wait until the window becomes visible so that window finder doesn't skip
    // these windows (it's required to wait because mapping and searching for
    // toplevel window is a subject to races).
    X11VisibilityWaiter waiter;
    waiter.WaitUntilWindowIsVisible(static_cast<x11::Window>(window));
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

  // Returns the topmost X window at the passed in screen position.
  x11::Window FindTopmostXWindowAt(int screen_x, int screen_y) {
    X11TopmostWindowFinder finder({});
    return finder.FindWindowAt(gfx::Point(screen_x, screen_y));
  }

  // Returns the topmost X window at the passed in screen position ignoring
  // |ignore_window|.
  x11::Window FindTopmostXWindowWithIgnore(int screen_x,
                                           int screen_y,
                                           x11::Window ignore_window) {
    std::set<gfx::AcceleratedWidget> ignore;
    ignore.insert(static_cast<gfx::AcceleratedWidget>(ignore_window));
    X11TopmostWindowFinder finder(ignore);
    return finder.FindWindowAt(gfx::Point(screen_x, screen_y));
  }

  // Returns the topmost X11Window at the passed in screen position. Returns
  // nullptr if the topmost window does not have an associated X11Window.
  X11Window* FindTopmostLocalProcessWindowAt(int screen_x, int screen_y) {
    X11TopmostWindowFinder finder({});
    auto x11_window =
        finder.FindLocalProcessWindowAt(gfx::Point(screen_x, screen_y));
    return x11_window == x11::Window::None
               ? nullptr
               : X11WindowManager::GetInstance()->GetWindow(
                     static_cast<gfx::AcceleratedWidget>(x11_window));
  }

  // Returns the topmost X11Window at the passed in screen position ignoring
  // |ignore_window|. Returns nullptr if the topmost window does not have an
  // associated X11Window.
  X11Window* FindTopmostLocalProcessWindowWithIgnore(
      int screen_x,
      int screen_y,
      x11::Window ignore_window) {
    std::set<gfx::AcceleratedWidget> ignore;
    ignore.insert(static_cast<gfx::AcceleratedWidget>(ignore_window));
    X11TopmostWindowFinder finder(ignore);
    auto x11_window =
        finder.FindLocalProcessWindowAt(gfx::Point(screen_x, screen_y));
    return x11_window == x11::Window::None
               ? nullptr
               : X11WindowManager::GetInstance()->GetWindow(
                     static_cast<gfx::AcceleratedWidget>(x11_window));
  }

 private:
  base::test::TaskEnvironment task_env_;
  std::unique_ptr<X11EventSource> event_source_;
};

TEST_F(X11TopmostWindowFinderTest, Basic) {
  // Avoid positioning test windows at 0x0 because window managers often have a
  // panel/launcher along one of the screen edges and do not allow windows to
  // position themselves to overlap the panel/launcher.
  TestPlatformWindowDelegate delegate;
  auto window1 = CreateAndShowX11Window(&delegate, {100, 100, 200, 100});
  auto x11_window1 = static_cast<x11::Window>(window1->GetWidget());

  x11::Window x11_window2 = CreateAndShowXWindow(gfx::Rect(200, 100, 100, 200));

  TestPlatformWindowDelegate delegate2;
  auto window3 = CreateAndShowX11Window(&delegate2, {100, 190, 200, 110});
  auto x11_window3 = static_cast<x11::Window>(window3->GetWidget());

  connection()->DispatchAll();

  EXPECT_EQ(x11_window1, FindTopmostXWindowAt(150, 150));
  EXPECT_EQ(window1.get(), FindTopmostLocalProcessWindowAt(150, 150));

  EXPECT_EQ(x11_window2, FindTopmostXWindowAt(250, 150));
  EXPECT_FALSE(FindTopmostLocalProcessWindowAt(250, 150));

  EXPECT_EQ(x11_window3, FindTopmostXWindowAt(250, 250));
  EXPECT_EQ(window3.get(), FindTopmostLocalProcessWindowAt(250, 250));

  EXPECT_EQ(x11_window3, FindTopmostXWindowAt(150, 250));
  EXPECT_EQ(window3.get(), FindTopmostLocalProcessWindowAt(150, 250));

  EXPECT_EQ(x11_window3, FindTopmostXWindowAt(150, 195));
  EXPECT_EQ(window3.get(), FindTopmostLocalProcessWindowAt(150, 195));

  EXPECT_NE(x11_window1, FindTopmostXWindowAt(1000, 1000));
  EXPECT_NE(x11_window2, FindTopmostXWindowAt(1000, 1000));
  EXPECT_NE(x11_window3, FindTopmostXWindowAt(1000, 1000));
  EXPECT_FALSE(FindTopmostLocalProcessWindowAt(1000, 1000));

  EXPECT_EQ(x11_window1, FindTopmostXWindowWithIgnore(150, 150, x11_window3));
  EXPECT_EQ(window1.get(),
            FindTopmostLocalProcessWindowWithIgnore(150, 150, x11_window3));
  EXPECT_EQ(x11_window2, FindTopmostXWindowWithIgnore(250, 250, x11_window3));
  EXPECT_FALSE(FindTopmostLocalProcessWindowWithIgnore(250, 250, x11_window3));
  EXPECT_EQ(x11::Window::None,
            FindTopmostXWindowWithIgnore(150, 250, x11_window3));
  EXPECT_FALSE(FindTopmostLocalProcessWindowWithIgnore(150, 250, x11_window3));
  EXPECT_EQ(x11_window1, FindTopmostXWindowWithIgnore(150, 195, x11_window3));
  EXPECT_EQ(window1.get(),
            FindTopmostLocalProcessWindowWithIgnore(150, 195, x11_window3));

  connection()->DestroyWindow({x11_window2});
}

// Test that the minimized state is properly handled.
// The test is flaky (https://crbug.com/955316)
TEST_F(X11TopmostWindowFinderTest, DISABLED_Minimized) {
  TestPlatformWindowDelegate delegate;
  auto window1 = CreateAndShowX11Window(&delegate, {100, 100, 100, 100});
  auto x11_window1 = static_cast<x11::Window>(window1->GetWidget());
  x11::Window x11_window2 = CreateAndShowXWindow(gfx::Rect(300, 100, 100, 100));

  connection()->DispatchAll();

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
  if (!IsShapeExtensionAvailable())
    return;

  TestPlatformWindowDelegate delegate;
  auto x11_window1 = CreateAndShowX11Window(&delegate, {100, 100, 100, 100});
  auto window1 = static_cast<x11::Window>(x11_window1->GetWidget());
  auto shape1 = std::make_unique<std::vector<gfx::Rect>>();
  shape1->emplace_back(0, 10, 10, 90);
  shape1->emplace_back(10, 0, 90, 100);
  gfx::Transform transform;
  transform.Scale(1.0f, 1.0f);
  x11_window1->SetShape(std::move(shape1), transform);

  SkRegion skregion2;
  skregion2.op(SkIRect::MakeXYWH(0, 10, 10, 90), SkRegion::kUnion_Op);
  skregion2.op(SkIRect::MakeXYWH(10, 0, 90, 100), SkRegion::kUnion_Op);
  x11::Window window2 = CreateAndShowXWindow(gfx::Rect(300, 100, 100, 100));
  auto region2 = x11::CreateRegionFromSkRegion(skregion2);
  x11::Connection::Get()->shape().Rectangles({
      .operation = x11::Shape::So::Set,
      .destination_kind = x11::Shape::Sk::Bounding,
      .ordering = x11::ClipOrdering::YXBanded,
      .destination_window = window2,
      .rectangles = *region2,
  });
  connection()->DispatchAll();

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
  if (!IsShapeExtensionAvailable())
    return;

  TestPlatformWindowDelegate delegate;
  auto x11_window1 = CreateAndShowX11Window(&delegate, {100, 100, 100, 100});
  auto window1 = static_cast<x11::Window>(x11_window1->GetWidget());

  auto shape1 = std::make_unique<std::vector<gfx::Rect>>();
  shape1->emplace_back();
  gfx::Transform transform;
  transform.Scale(1.0f, 1.0f);
  x11_window1->SetShape(std::move(shape1), transform);

  connection()->DispatchAll();

  EXPECT_NE(window1, FindTopmostXWindowAt(105, 105));
}

// Test that setting a Null shape removes the shape.
// crbug.com/955316: flaky on Linux
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_NonRectangularNullShape DISABLED_NonRectangularNullShape
#else
#define MAYBE_NonRectangularNullShape NonRectangularNullShape
#endif
TEST_F(X11TopmostWindowFinderTest, MAYBE_NonRectangularNullShape) {
  if (!IsShapeExtensionAvailable())
    return;

  TestPlatformWindowDelegate delegate;
  auto x11_window1 = CreateAndShowX11Window(&delegate, {100, 100, 100, 100});
  auto window1 = static_cast<x11::Window>(x11_window1->GetWidget());

  auto shape1 = std::make_unique<std::vector<gfx::Rect>>();
  shape1->emplace_back();
  gfx::Transform transform;
  transform.Scale(1.0f, 1.0f);
  x11_window1->SetShape(std::move(shape1), transform);

  // Remove the shape - this is now just a normal window.
  x11_window1->SetShape(nullptr, transform);

  connection()->DispatchAll();

  EXPECT_EQ(window1, FindTopmostXWindowAt(105, 105));
}

// Test that the TopmostWindowFinder finds windows which belong to menus
// (which may or may not belong to Chrome).
//
// Flakes (https://crbug.com/955316)
TEST_F(X11TopmostWindowFinderTest, DISABLED_Menu) {
  x11::Window window = CreateAndShowXWindow(gfx::Rect(100, 100, 100, 100));

  x11::Window root = GetX11RootWindow();
  auto menu_window = connection()->GenerateId<x11::Window>();
  connection()->CreateWindow({
      .wid = menu_window,
      .parent = root,
      .width = 1,
      .height = 1,
      .c_class = x11::WindowClass::CopyFromParent,
      .override_redirect = x11::Bool32(true),
  });

  SetProperty(menu_window, x11::GetAtom("_NET_WM_WINDOW_TYPE"), x11::Atom::ATOM,
              x11::GetAtom("_NET_WM_WINDOW_TYPE_MENU"));

  SetUseOSWindowFrame(menu_window, false);
  ShowAndSetXWindowBounds(menu_window, gfx::Rect(140, 110, 100, 100));
  connection()->DispatchAll();

  EXPECT_EQ(window, FindTopmostXWindowAt(110, 110));
  EXPECT_EQ(menu_window, FindTopmostXWindowAt(150, 120));
  EXPECT_EQ(menu_window, FindTopmostXWindowAt(210, 120));

  connection()->DestroyWindow({window});
  connection()->DestroyWindow({menu_window});
}

}  // namespace ui
