// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include "base/rand_util.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/widget/root_view.h"

#if BUILDFLAG(IS_MAC)
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#endif

#if (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)

#include "ui/views/test/test_desktop_screen_ozone.h"
#elif BUILDFLAG(IS_WIN)
#include "ui/views/widget/desktop_aura/desktop_screen_win.h"
#endif

namespace views::test {

namespace {

View::Views ShuffledChildren(View* view) {
  View::Views children(view->children());
  base::RandomShuffle(children.begin(), children.end());
  return children;
}

}  // namespace

View* AnyViewMatchingPredicate(View* view, const ViewPredicate& predicate) {
  if (predicate.Run(view))
    return view;
  // Note that we randomize the order of the children, to avoid this function
  // always choosing the same View to return out of a set of possible Views.
  // If we didn't do this, client code could accidentally depend on a specific
  // search order.
  for (views::View* child : ShuffledChildren(view)) {
    auto* found = AnyViewMatchingPredicate(child, predicate);
    if (found)
      return found;
  }
  return nullptr;
}

View* AnyViewMatchingPredicate(Widget* widget, const ViewPredicate& predicate) {
  return AnyViewMatchingPredicate(widget->GetRootView(), predicate);
}

View* AnyViewWithClassName(Widget* widget, const std::string& classname) {
  return AnyViewMatchingPredicate(widget, [&](const View* view) {
    return view->GetClassName() == classname;
  });
}

WidgetTest::WidgetTest() = default;

WidgetTest::WidgetTest(
    std::unique_ptr<base::test::TaskEnvironment> task_environment)
    : ViewsTestBase(std::move(task_environment)) {}

WidgetTest::~WidgetTest() = default;

Widget* WidgetTest::CreateTopLevelPlatformWidget(
    Widget::InitParams::Ownership ownership) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(ownership, Widget::InitParams::TYPE_WINDOW);
  params.native_widget =
      CreatePlatformNativeWidgetImpl(widget.get(), kStubCapture, nullptr);
  widget->Init(std::move(params));
  return widget.release();
}

#if BUILDFLAG(ENABLE_DESKTOP_AURA)
Widget* WidgetTest::CreateTopLevelPlatformDesktopWidget(
    Widget::InitParams::Ownership ownership) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(ownership, Widget::InitParams::TYPE_WINDOW);
  params.native_widget = CreatePlatformDesktopNativeWidgetImpl(
      widget.get(), kStubCapture, base::DoNothing());
  widget->Init(std::move(params));
  return widget.release();
}
#endif

Widget* WidgetTest::CreateTopLevelFramelessPlatformWidget(
    Widget::InitParams::Ownership ownership) {
  auto widget = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(ownership, Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget =
      CreatePlatformNativeWidgetImpl(widget.get(), kStubCapture, nullptr);
  widget->Init(std::move(params));
  return widget.release();
}

Widget* WidgetTest::CreateChildPlatformWidget(
    gfx::NativeView parent_native_view,
    Widget::InitParams::Ownership ownership) {
  Widget::InitParams params =
      CreateParams(ownership, Widget::InitParams::TYPE_CONTROL);
  params.parent = parent_native_view;
  auto child = std::make_unique<Widget>();
  params.native_widget =
      CreatePlatformNativeWidgetImpl(child.get(), kStubCapture, nullptr);
  child->Init(std::move(params));
  child->SetContentsView(std::make_unique<View>());
  return child.release();
}

Widget* WidgetTest::CreateTopLevelNativeWidget(
    Widget::InitParams::Ownership ownership) {
  auto toplevel = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(ownership, Widget::InitParams::TYPE_WINDOW);
  toplevel->Init(std::move(params));
  return toplevel.release();
}

Widget* WidgetTest::CreateChildNativeWidgetWithParent(
    Widget* parent,
    Widget::InitParams::Ownership ownership) {
  auto child = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(ownership, Widget::InitParams::TYPE_CONTROL);
  params.parent = parent->GetNativeView();
  child->Init(std::move(params));
  child->SetContentsView(std::make_unique<View>());
  return child.release();
}

View* WidgetTest::GetMousePressedHandler(views::internal::RootView* root_view) {
  return root_view->mouse_pressed_handler_;
}

View* WidgetTest::GetMouseMoveHandler(views::internal::RootView* root_view) {
  return root_view->mouse_move_handler_;
}

View* WidgetTest::GetGestureHandler(views::internal::RootView* root_view) {
  return root_view->gesture_handler_;
}

DesktopWidgetTest::DesktopWidgetTest() = default;
DesktopWidgetTest::~DesktopWidgetTest() = default;

void DesktopWidgetTest::SetUp() {
  set_native_widget_type(NativeWidgetType::kDesktop);
  WidgetTest::SetUp();
}

DesktopWidgetTestInteractive::DesktopWidgetTestInteractive() = default;
DesktopWidgetTestInteractive::~DesktopWidgetTestInteractive() = default;

void DesktopWidgetTestInteractive::SetUp() {
  SetUpForInteractiveTests();
#if (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) || \
    BUILDFLAG(IS_CHROMEOS_LACROS)
  screen_ = views::test::TestDesktopScreenOzone::Create();
#elif BUILDFLAG(IS_WIN)
  screen_ = std::make_unique<views::DesktopScreenWin>();
#endif
  DesktopWidgetTest::SetUp();
}

#if (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) || \
    BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
void DesktopWidgetTestInteractive::TearDown() {
  DesktopWidgetTest::TearDown();
  screen_.reset();
}
#endif

TestDesktopWidgetDelegate::TestDesktopWidgetDelegate()
    : TestDesktopWidgetDelegate(nullptr) {}

TestDesktopWidgetDelegate::TestDesktopWidgetDelegate(Widget* widget)
    : widget_(widget) {
  SetFocusTraversesOut(true);
  if (!widget_) {
    owned_widget_ = std::make_unique<Widget>();
    widget_ = owned_widget_.get();
  }
}

TestDesktopWidgetDelegate::~TestDesktopWidgetDelegate() {
  if (widget_)
    widget_->CloseNow();
  EXPECT_FALSE(widget_);
}

void TestDesktopWidgetDelegate::InitWidget(Widget::InitParams init_params) {
  init_params.delegate = this;
  init_params.bounds = initial_bounds_;
  widget_->Init(std::move(init_params));
}

void TestDesktopWidgetDelegate::WindowClosing() {
  window_closing_count_++;
  widget_ = nullptr;
  contents_view_ = nullptr;
}

Widget* TestDesktopWidgetDelegate::GetWidget() {
  return widget_;
}

const Widget* TestDesktopWidgetDelegate::GetWidget() const {
  return widget_;
}

View* TestDesktopWidgetDelegate::GetContentsView() {
  return contents_view_ ? contents_view_.get()
                        : WidgetDelegate::GetContentsView();
}

bool TestDesktopWidgetDelegate::OnCloseRequested(
    Widget::ClosedReason close_reason) {
  last_closed_reason_ = close_reason;
  return can_close_;
}

TestInitialFocusWidgetDelegate::TestInitialFocusWidgetDelegate(
    gfx::NativeWindow context) {
  Widget::InitParams params(Widget::InitParams::CLIENT_OWNS_WIDGET,
                            Widget::InitParams::TYPE_WINDOW);
  params.context = context;
  params.delegate = this;
  GetWidget()->Init(std::move(params));
  view_ =
      GetWidget()->GetContentsView()->AddChildView(std::make_unique<View>());
  view_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
}

TestInitialFocusWidgetDelegate::~TestInitialFocusWidgetDelegate() = default;

View* TestInitialFocusWidgetDelegate::GetInitiallyFocusedView() {
  return view_;
}

WidgetDestroyedWaiter::WidgetDestroyedWaiter(Widget* widget) {
  widget_observation_.Observe(widget);
}

WidgetDestroyedWaiter::~WidgetDestroyedWaiter() = default;

void WidgetDestroyedWaiter::Wait() {
  run_loop_.Run();
}

void WidgetDestroyedWaiter::OnWidgetDestroyed(Widget* widget) {
  widget_observation_.Reset();
  run_loop_.Quit();
}

WidgetVisibleWaiter::WidgetVisibleWaiter(Widget* widget) {
  widget_observation_.Observe(widget);
}

WidgetVisibleWaiter::~WidgetVisibleWaiter() = default;

void WidgetVisibleWaiter::Wait() {
  if (!widget_observation_.GetSource()->IsVisible()) {
    run_loop_.Run();
  }
}

void WidgetVisibleWaiter::OnWidgetVisibilityChanged(Widget* widget,
                                                    bool visible) {
  if (!run_loop_.running()) {
    return;
  }
  if (visible) {
    DCHECK(widget_observation_.IsObservingSource(widget));
    run_loop_.Quit();
  }
}

void WidgetVisibleWaiter::OnWidgetDestroying(Widget* widget) {
  if (run_loop_.running()) {
    ADD_FAILURE() << "Widget destroying before it became visible!";
  }
  DCHECK(widget_observation_.IsObservingSource(widget));
  widget_observation_.Reset();
}

}  // namespace views::test
