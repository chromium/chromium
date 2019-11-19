// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display_switches.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/platform/x11/x11_event_source_glib.h"
#include "ui/events/test/events_test_utils_x11.h"
#include "ui/events/test/platform_event_source_test_api.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/x11_property_change_waiter.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace views {

namespace {

const int kPointerDeviceId = 1;

// Blocks till the window state hint, |hint|, is set or unset.
class WMStateWaiter : public X11PropertyChangeWaiter {
 public:
  WMStateWaiter(XID window, const char* hint, bool wait_till_set)
      : X11PropertyChangeWaiter(window, "_NET_WM_STATE"),
        hint_(hint),
        wait_till_set_(wait_till_set) {}

  ~WMStateWaiter() override = default;

 private:
  // X11PropertyChangeWaiter:
  bool ShouldKeepOnWaiting(const ui::PlatformEvent& event) override {
    std::vector<Atom> hints;
    if (ui::GetAtomArrayProperty(xwindow(), "_NET_WM_STATE", &hints))
      return base::Contains(hints, gfx::GetAtom(hint_)) != wait_till_set_;
    return true;
  }

  // The name of the hint to wait to get set or unset.
  const char* hint_;

  // Whether we are waiting for |hint| to be set or unset.
  bool wait_till_set_;

  DISALLOW_COPY_AND_ASSIGN(WMStateWaiter);
};

// A NonClientFrameView with a window mask with the bottom right corner cut out.
class ShapedNonClientFrameView : public NonClientFrameView {
 public:
  ShapedNonClientFrameView() = default;

  ~ShapedNonClientFrameView() override = default;

  // NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return client_bounds;
  }
  int NonClientHitTest(const gfx::Point& point) override {
    // Fake bottom for non client event test.
    if (point == gfx::Point(500, 500))
      return HTBOTTOM;
    return HTNOWHERE;
  }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {
    int right = size.width();
    int bottom = size.height();

    window_mask->moveTo(0, 0);
    window_mask->lineTo(0, bottom);
    window_mask->lineTo(right, bottom);
    window_mask->lineTo(right, 10);
    window_mask->lineTo(right - 10, 10);
    window_mask->lineTo(right - 10, 0);
    window_mask->close();
  }
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  bool GetAndResetLayoutRequest() {
    bool layout_requested = layout_requested_;
    layout_requested_ = false;
    return layout_requested;
  }

 private:
  void Layout() override { layout_requested_ = true; }

  bool layout_requested_ = false;

  DISALLOW_COPY_AND_ASSIGN(ShapedNonClientFrameView);
};

class ShapedWidgetDelegate : public WidgetDelegateView {
 public:
  ShapedWidgetDelegate() = default;

  ~ShapedWidgetDelegate() override = default;

  // WidgetDelegateView:
  NonClientFrameView* CreateNonClientFrameView(Widget* widget) override {
    return new ShapedNonClientFrameView;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShapedWidgetDelegate);
};

// Creates a widget of size 100x100.
std::unique_ptr<Widget> CreateWidget(WidgetDelegate* delegate) {
  std::unique_ptr<Widget> widget(new Widget);
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.delegate = delegate;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.remove_standard_frame = true;
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget->Init(std::move(params));
  return widget;
}

// Returns the list of rectangles which describe |xid|'s bounding region via the
// X shape extension.
std::vector<gfx::Rect> GetShapeRects(XID xid) {
  int dummy;
  int shape_rects_size;
  gfx::XScopedPtr<XRectangle[]> shape_rects(XShapeGetRectangles(
      gfx::GetXDisplay(), xid, ShapeBounding, &shape_rects_size, &dummy));

  std::vector<gfx::Rect> shape_vector;
  for (int i = 0; i < shape_rects_size; ++i) {
    const XRectangle& rect = shape_rects[i];
    shape_vector.emplace_back(rect.x, rect.y, rect.width, rect.height);
  }
  return shape_vector;
}

// Returns true if one of |rects| contains point (x,y).
bool ShapeRectContainsPoint(const std::vector<gfx::Rect>& shape_rects,
                            int x,
                            int y) {
  gfx::Point point(x, y);
  return std::any_of(
      shape_rects.cbegin(), shape_rects.cend(),
      [&point](const auto& rect) { return rect.Contains(point); });
}

}  // namespace

class DesktopWindowTreeHostX11Test : public ViewsTestBase {
 public:
  DesktopWindowTreeHostX11Test()
      : event_source_(ui::PlatformEventSource::GetInstance()) {}
  ~DesktopWindowTreeHostX11Test() override = default;

  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);

    std::vector<int> pointer_devices;
    pointer_devices.push_back(kPointerDeviceId);
    ui::TouchFactory::GetInstance()->SetPointerDeviceForTest(pointer_devices);

    ViewsTestBase::SetUp();

    // Make X11 synchronous for our display connection. This does not force the
    // window manager to behave synchronously.
    XSynchronize(gfx::GetXDisplay(), x11::True);
  }

  void TearDown() override {
    XSynchronize(gfx::GetXDisplay(), x11::False);
    ViewsTestBase::TearDown();
  }

  void DispatchSingleEventToWidget(XEvent* event, Widget* widget) {
    DCHECK_EQ(GenericEvent, event->type);
    XIDeviceEvent* device_event =
        static_cast<XIDeviceEvent*>(event->xcookie.data);
    device_event->event =
        widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
    event_source_.Dispatch(event);
  }

 private:
  ui::test::PlatformEventSourceTestAPI event_source_;
  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostX11Test);
};

// https://crbug.com/898742: Test is flaky.
// Tests that the shape is properly set on the x window.
TEST_F(DesktopWindowTreeHostX11Test, DISABLED_Shape) {
  if (!ui::IsShapeExtensionAvailable())
    return;

  // 1) Test setting the window shape via the NonClientFrameView. This technique
  // is used to get rounded corners on Chrome windows when not using the native
  // window frame.
  std::unique_ptr<Widget> widget1 = CreateWidget(new ShapedWidgetDelegate());
  widget1->Show();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  XID xid1 = widget1->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  std::vector<gfx::Rect> shape_rects = GetShapeRects(xid1);
  ASSERT_FALSE(shape_rects.empty());

  // The widget was supposed to be 100x100, but the WM might have ignored this
  // suggestion.
  int widget_width = widget1->GetWindowBoundsInScreen().width();
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, widget_width - 15, 5));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, widget_width - 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, widget_width - 5, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, widget_width + 5, 15));

  // Changing widget's size should update the shape.
  widget1->SetBounds(gfx::Rect(100, 100, 200, 200));
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  if (widget1->GetWindowBoundsInScreen().width() == 200) {
    shape_rects = GetShapeRects(xid1);
    ASSERT_FALSE(shape_rects.empty());
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 85, 5));
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 5));
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 185, 5));
    EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 195, 5));
    EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 195, 15));
    EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 205, 15));
  }

  if (ui::WmSupportsHint(gfx::GetAtom("_NET_WM_STATE_MAXIMIZED_VERT"))) {
    // The shape should be changed to a rectangle which fills the entire screen
    // when |widget1| is maximized.
    {
      WMStateWaiter waiter(xid1, "_NET_WM_STATE_MAXIMIZED_VERT", true);
      widget1->Maximize();
      waiter.Wait();
    }

    // Ensure that the task which is posted when a window is resized is run.
    base::RunLoop().RunUntilIdle();

    // xvfb does not support Xrandr so we cannot check the maximized window's
    // bounds.
    gfx::Rect maximized_bounds;
    ui::GetOuterWindowBounds(xid1, &maximized_bounds);

    shape_rects = GetShapeRects(xid1);
    ASSERT_FALSE(shape_rects.empty());
    EXPECT_TRUE(
        ShapeRectContainsPoint(shape_rects, maximized_bounds.width() - 1, 5));
    EXPECT_TRUE(
        ShapeRectContainsPoint(shape_rects, maximized_bounds.width() - 1, 15));
  }

  // 2) Test setting the window shape via Widget::SetShape().
  auto shape_region = std::make_unique<Widget::ShapeRects>();
  shape_region->emplace_back(10, 0, 90, 10);
  shape_region->emplace_back(0, 10, 10, 90);
  shape_region->emplace_back(10, 10, 90, 90);

  std::unique_ptr<Widget> widget2(CreateWidget(nullptr));
  widget2->Show();
  widget2->SetShape(std::move(shape_region));
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  XID xid2 = widget2->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  shape_rects = GetShapeRects(xid2);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 15, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 105, 15));

  // Changing the widget's size should not affect the shape.
  widget2->SetBounds(gfx::Rect(100, 100, 200, 200));
  shape_rects = GetShapeRects(xid2);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 15, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 105, 15));

  // Setting the shape to NULL resets the shape back to the entire
  // window bounds.
  widget2->SetShape(nullptr);
  shape_rects = GetShapeRects(xid2);
  ASSERT_FALSE(shape_rects.empty());
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 5, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 15, 5));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 95, 15));
  EXPECT_TRUE(ShapeRectContainsPoint(shape_rects, 105, 15));
  EXPECT_FALSE(ShapeRectContainsPoint(shape_rects, 500, 500));
}

// Test that the widget reacts on changes in fullscreen state initiated by the
// window manager (e.g. via a window manager accelerator key).
TEST_F(DesktopWindowTreeHostX11Test, WindowManagerTogglesFullscreen) {
  if (!ui::WmSupportsHint(gfx::GetAtom("_NET_WM_STATE_FULLSCREEN")))
    return;

  std::unique_ptr<Widget> widget = CreateWidget(new ShapedWidgetDelegate());
  auto* non_client_view = static_cast<ShapedNonClientFrameView*>(
      widget->non_client_view()->frame_view());
  ASSERT_TRUE(non_client_view);
  XID xid = widget->GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  widget->Show();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  gfx::Rect initial_bounds = widget->GetWindowBoundsInScreen();
  {
    WMStateWaiter waiter(xid, "_NET_WM_STATE_FULLSCREEN", true);
    widget->SetFullscreen(true);
    waiter.Wait();
  }
  EXPECT_TRUE(widget->IsFullscreen());

  // After the fullscreen state has been set, there must be a relayout request
  EXPECT_TRUE(non_client_view->GetAndResetLayoutRequest());

  // Ensure there is not request before we proceed.
  EXPECT_FALSE(non_client_view->GetAndResetLayoutRequest());

  // Emulate the window manager exiting fullscreen via a window manager
  // accelerator key. It should affect the widget's fullscreen state.
  {
    Display* display = gfx::GetXDisplay();

    XEvent xclient;
    memset(&xclient, 0, sizeof(xclient));
    xclient.type = ClientMessage;
    xclient.xclient.window = xid;
    xclient.xclient.message_type = gfx::GetAtom("_NET_WM_STATE");
    xclient.xclient.format = 32;
    xclient.xclient.data.l[0] = 0;
    xclient.xclient.data.l[1] = gfx::GetAtom("_NET_WM_STATE_FULLSCREEN");
    xclient.xclient.data.l[2] = 0;
    xclient.xclient.data.l[3] = 1;
    xclient.xclient.data.l[4] = 0;
    XSendEvent(display, DefaultRootWindow(display), x11::False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xclient);

    WMStateWaiter waiter(xid, "_NET_WM_STATE_FULLSCREEN", false);
    waiter.Wait();
  }
  EXPECT_FALSE(widget->IsFullscreen());
  EXPECT_EQ(initial_bounds.ToString(),
            widget->GetWindowBoundsInScreen().ToString());

  // Even though the unfullscreen request came from the window manager, we must
  // still react and relayout.
  EXPECT_TRUE(non_client_view->GetAndResetLayoutRequest());
}

// Tests that the minimization information is propagated to the content window.
TEST_F(DesktopWindowTreeHostX11Test, ToggleMinimizePropogateToContentWindow) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(std::move(params));
  widget.Show();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  XID xid = widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget();
  Display* display = gfx::GetXDisplay();

  // Minimize by sending _NET_WM_STATE_HIDDEN
  {
    std::vector<::Atom> atom_list;
    atom_list.push_back(gfx::GetAtom("_NET_WM_STATE_HIDDEN"));
    ui::SetAtomArrayProperty(xid, "_NET_WM_STATE", "ATOM", atom_list);

    XEvent xevent;
    memset(&xevent, 0, sizeof(xevent));
    xevent.type = PropertyNotify;
    xevent.xproperty.type = PropertyNotify;
    xevent.xproperty.send_event = 1;
    xevent.xproperty.display = display;
    xevent.xproperty.window = xid;
    xevent.xproperty.atom = gfx::GetAtom("_NET_WM_STATE");
    xevent.xproperty.state = 0;
    XSendEvent(display, DefaultRootWindow(display), x11::False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xevent);

    WMStateWaiter waiter(xid, "_NET_WM_STATE_HIDDEN", true);
    waiter.Wait();
  }
  EXPECT_FALSE(widget.GetNativeWindow()->IsVisible());

  // Show from minimized by sending _NET_WM_STATE_FOCUSED
  {
    std::vector<::Atom> atom_list;
    atom_list.push_back(gfx::GetAtom("_NET_WM_STATE_FOCUSED"));
    ui::SetAtomArrayProperty(xid, "_NET_WM_STATE", "ATOM", atom_list);

    XEvent xevent;
    memset(&xevent, 0, sizeof(xevent));
    xevent.type = PropertyNotify;
    xevent.xproperty.type = PropertyNotify;
    xevent.xproperty.send_event = 1;
    xevent.xproperty.display = display;
    xevent.xproperty.window = xid;
    xevent.xproperty.atom = gfx::GetAtom("_NET_WM_STATE");
    xevent.xproperty.state = 0;
    XSendEvent(display, DefaultRootWindow(display), x11::False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xevent);

    WMStateWaiter waiter(xid, "_NET_WM_STATE_FOCUSED", true);
    waiter.Wait();
  }
  EXPECT_TRUE(widget.GetNativeWindow()->IsVisible());
}

TEST_F(DesktopWindowTreeHostX11Test, ChildWindowDestructionDuringTearDown) {
  Widget parent_widget;
  Widget::InitParams parent_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  parent_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  parent_widget.Init(std::move(parent_params));
  parent_widget.Show();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  Widget child_widget;
  Widget::InitParams child_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW);
  child_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  child_params.parent = parent_widget.GetNativeWindow();
  child_widget.Init(std::move(child_params));
  child_widget.Show();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  // Sanity check that the two widgets each have their own XID.
  ASSERT_NE(parent_widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget(),
            child_widget.GetNativeWindow()->GetHost()->GetAcceleratedWidget());
  Widget::CloseAllSecondaryWidgets();
  EXPECT_TRUE(DesktopWindowTreeHostLinux::GetAllOpenWindows().empty());
}

// A Widget that allows setting the min/max size for the widget.
class CustomSizeWidget : public Widget {
 public:
  CustomSizeWidget() = default;
  ~CustomSizeWidget() override = default;

  void set_min_size(const gfx::Size& size) { min_size_ = size; }
  void set_max_size(const gfx::Size& size) { max_size_ = size; }

  // Widget:
  gfx::Size GetMinimumSize() const override { return min_size_; }
  gfx::Size GetMaximumSize() const override { return max_size_; }

 private:
  gfx::Size min_size_;
  gfx::Size max_size_;

  DISALLOW_COPY_AND_ASSIGN(CustomSizeWidget);
};

TEST_F(DesktopWindowTreeHostX11Test, SetBoundsWithMinMax) {
  CustomSizeWidget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(200, 100);
  widget.Init(std::move(params));
  widget.Show();
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  EXPECT_EQ(gfx::Size(200, 100).ToString(),
            widget.GetWindowBoundsInScreen().size().ToString());
  widget.SetBounds(gfx::Rect(300, 200));
  EXPECT_EQ(gfx::Size(300, 200).ToString(),
            widget.GetWindowBoundsInScreen().size().ToString());

  widget.set_min_size(gfx::Size(100, 100));
  widget.SetBounds(gfx::Rect(50, 500));
  EXPECT_EQ(gfx::Size(100, 500).ToString(),
            widget.GetWindowBoundsInScreen().size().ToString());
}

class MouseEventRecorder : public ui::EventHandler {
 public:
  MouseEventRecorder() = default;
  ~MouseEventRecorder() override = default;

  void Reset() { mouse_events_.clear(); }

  const std::vector<ui::MouseEvent>& mouse_events() const {
    return mouse_events_;
  }

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* mouse) override {
    mouse_events_.push_back(*mouse);
  }

  std::vector<ui::MouseEvent> mouse_events_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventRecorder);
};

class DesktopWindowTreeHostX11HighDPITest
    : public DesktopWindowTreeHostX11Test {
 public:
  DesktopWindowTreeHostX11HighDPITest() = default;
  ~DesktopWindowTreeHostX11HighDPITest() override = default;

  void PretendCapture(views::Widget* capture_widget) {
    if (capture_widget)
      capture_widget->GetNativeWindow()->SetCapture();
  }

 private:
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "2");

    DesktopWindowTreeHostX11Test::SetUp();
  }

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostX11HighDPITest);
};

// https://crbug.com/702687
TEST_F(DesktopWindowTreeHostX11HighDPITest,
       DISABLED_LocatedEventDispatchWithCapture) {
  Widget first;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 50, 50);
  first.Init(std::move(params));
  first.Show();

  Widget second;
  params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 50, 50);
  second.Init(std::move(params));
  second.Show();

  ui::X11EventSource::GetInstance()->DispatchXEvents();

  MouseEventRecorder first_recorder, second_recorder;
  first.GetNativeWindow()->AddPreTargetHandler(&first_recorder);
  second.GetNativeWindow()->AddPreTargetHandler(&second_recorder);

  // Dispatch an event on |first|. Verify it gets the event.
  ui::ScopedXI2Event event;
  event.InitGenericButtonEvent(kPointerDeviceId, ui::ET_MOUSEWHEEL,
                               gfx::Point(50, 50), ui::EF_NONE);
  DispatchSingleEventToWidget(event, &first);
  ASSERT_EQ(1u, first_recorder.mouse_events().size());
  EXPECT_EQ(ui::ET_MOUSEWHEEL, first_recorder.mouse_events()[0].type());
  EXPECT_EQ(gfx::Point(25, 25).ToString(),
            first_recorder.mouse_events()[0].location().ToString());
  ASSERT_EQ(0u, second_recorder.mouse_events().size());

  first_recorder.Reset();
  second_recorder.Reset();

  // Set a capture on |second|, and dispatch the same event to |first|. This
  // event should reach |second| instead.
  PretendCapture(&second);
  event.InitGenericButtonEvent(kPointerDeviceId, ui::ET_MOUSEWHEEL,
                               gfx::Point(50, 50), ui::EF_NONE);
  DispatchSingleEventToWidget(event, &first);

  ASSERT_EQ(0u, first_recorder.mouse_events().size());
  ASSERT_EQ(1u, second_recorder.mouse_events().size());
  EXPECT_EQ(ui::ET_MOUSEWHEEL, second_recorder.mouse_events()[0].type());
  EXPECT_EQ(gfx::Point(-25, -25).ToString(),
            second_recorder.mouse_events()[0].location().ToString());

  PretendCapture(nullptr);
  first.GetNativeWindow()->RemovePreTargetHandler(&first_recorder);
  second.GetNativeWindow()->RemovePreTargetHandler(&second_recorder);
}

TEST_F(DesktopWindowTreeHostX11Test, MouseNCEvents) {
  std::unique_ptr<Widget> widget = CreateWidget(new ShapedWidgetDelegate());
  widget->Show();

  ui::X11EventSource::GetInstance()->DispatchXEvents();

  widget->SetBounds(gfx::Rect(100, 100, 501, 501));
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  MouseEventRecorder recorder;
  widget->GetNativeWindow()->AddPreTargetHandler(&recorder);

  ui::ScopedXI2Event event;
  event.InitGenericButtonEvent(kPointerDeviceId, ui::ET_MOUSE_PRESSED,
                               gfx::Point(500, 500), ui::EF_LEFT_MOUSE_BUTTON);

  DispatchSingleEventToWidget(event, widget.get());
  ASSERT_EQ(1u, recorder.mouse_events().size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, recorder.mouse_events()[0].type());
  EXPECT_TRUE(recorder.mouse_events()[0].flags() & ui::EF_IS_NON_CLIENT);

  widget->GetNativeWindow()->RemovePreTargetHandler(&recorder);
}

TEST_F(DesktopWindowTreeHostX11HighDPITest, MouseNCEvents) {
  std::unique_ptr<Widget> widget = CreateWidget(new ShapedWidgetDelegate());
  widget->Show();

  ui::X11EventSource::GetInstance()->DispatchXEvents();

  widget->SetBounds(gfx::Rect(100, 100, 1000, 1000));
  ui::X11EventSource::GetInstance()->DispatchXEvents();

  MouseEventRecorder recorder;
  widget->GetNativeWindow()->AddPreTargetHandler(&recorder);

  ui::ScopedXI2Event event;
  event.InitGenericButtonEvent(kPointerDeviceId, ui::ET_MOUSE_PRESSED,
                               gfx::Point(1001, 1001),
                               ui::EF_LEFT_MOUSE_BUTTON);
  DispatchSingleEventToWidget(event, widget.get());
  ASSERT_EQ(1u, recorder.mouse_events().size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, recorder.mouse_events()[0].type());
  EXPECT_TRUE(recorder.mouse_events()[0].flags() & ui::EF_IS_NON_CLIENT);

  widget->GetNativeWindow()->RemovePreTargetHandler(&recorder);
}

}  // namespace views
