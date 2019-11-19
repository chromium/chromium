// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

void WidgetTest::WidgetCloser::operator()(Widget* widget) const {
  widget->CloseNow();
}

WidgetTest::~WidgetTest() = default;

Widget* WidgetTest::CreateTopLevelPlatformWidget() {
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.native_widget =
      CreatePlatformNativeWidgetImpl(params, widget, kStubCapture, nullptr);
  widget->Init(std::move(params));
  return widget;
}

Widget* WidgetTest::CreateTopLevelFramelessPlatformWidget() {
  Widget* widget = new Widget;
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget =
      CreatePlatformNativeWidgetImpl(params, widget, kStubCapture, nullptr);
  widget->Init(std::move(params));
  return widget;
}

Widget* WidgetTest::CreateChildPlatformWidget(
    gfx::NativeView parent_native_view) {
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_CONTROL);
  params.parent = parent_native_view;
  Widget* child = new Widget;
  params.native_widget =
      CreatePlatformNativeWidgetImpl(params, child, kStubCapture, nullptr);
  child->Init(std::move(params));
  child->SetContentsView(new View);
  return child;
}

Widget* WidgetTest::CreateTopLevelNativeWidget() {
  Widget* toplevel = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  toplevel->Init(std::move(params));
  return toplevel;
}

Widget* WidgetTest::CreateChildNativeWidgetWithParent(Widget* parent) {
  Widget* child = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_CONTROL);
  params.parent = parent->GetNativeView();
  child->Init(std::move(params));
  child->SetContentsView(new View);
  return child;
}

Widget* WidgetTest::CreateChildNativeWidget() {
  return CreateChildNativeWidgetWithParent(nullptr);
}

View* WidgetTest::GetMousePressedHandler(internal::RootView* root_view) {
  return root_view->mouse_pressed_handler_;
}

View* WidgetTest::GetMouseMoveHandler(internal::RootView* root_view) {
  return root_view->mouse_move_handler_;
}

View* WidgetTest::GetGestureHandler(internal::RootView* root_view) {
  return root_view->gesture_handler_;
}

DesktopWidgetTest::DesktopWidgetTest() = default;
DesktopWidgetTest::~DesktopWidgetTest() = default;

void DesktopWidgetTest::SetUp() {
  set_native_widget_type(NativeWidgetType::kDesktop);
  WidgetTest::SetUp();
}

TestDesktopWidgetDelegate::TestDesktopWidgetDelegate() : widget_(new Widget) {}

TestDesktopWidgetDelegate::TestDesktopWidgetDelegate(Widget* widget)
    : widget_(widget) {}

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
}

Widget* TestDesktopWidgetDelegate::GetWidget() {
  return widget_;
}

const Widget* TestDesktopWidgetDelegate::GetWidget() const {
  return widget_;
}

View* TestDesktopWidgetDelegate::GetContentsView() {
  return contents_view_ ? contents_view_ : WidgetDelegate::GetContentsView();
}

bool TestDesktopWidgetDelegate::ShouldAdvanceFocusToTopLevelWidget() const {
  return true;  // Same default as DefaultWidgetDelegate in widget.cc.
}

bool TestDesktopWidgetDelegate::OnCloseRequested(
    Widget::ClosedReason close_reason) {
  last_closed_reason_ = close_reason;
  return can_close_;
}

TestInitialFocusWidgetDelegate::TestInitialFocusWidgetDelegate(
    gfx::NativeWindow context)
    : view_(new View) {
  view_->SetFocusBehavior(View::FocusBehavior::ALWAYS);

  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
  params.context = context;
  params.delegate = this;
  GetWidget()->Init(std::move(params));
  GetWidget()->GetContentsView()->AddChildView(view_);
}

TestInitialFocusWidgetDelegate::~TestInitialFocusWidgetDelegate() = default;

View* TestInitialFocusWidgetDelegate::GetInitiallyFocusedView() {
  return view_;
}

WidgetActivationWaiter::WidgetActivationWaiter(Widget* widget, bool active)
    : observed_(false), active_(active) {
  if (active == widget->IsActive()) {
    observed_ = true;
    return;
  }
  widget->AddObserver(this);
}

WidgetActivationWaiter::~WidgetActivationWaiter() = default;

void WidgetActivationWaiter::Wait() {
  if (!observed_)
    run_loop_.Run();
}

void WidgetActivationWaiter::OnWidgetActivationChanged(Widget* widget,
                                                       bool active) {
  if (active_ != active)
    return;

  observed_ = true;
  widget->RemoveObserver(this);
  if (run_loop_.running())
    run_loop_.Quit();
}

WidgetClosingObserver::WidgetClosingObserver(Widget* widget) : widget_(widget) {
  widget_->AddObserver(this);
}

WidgetClosingObserver::~WidgetClosingObserver() {
  if (widget_)
    widget_->RemoveObserver(this);
}

void WidgetClosingObserver::Wait() {
  if (widget_)
    run_loop_.Run();
}

void WidgetClosingObserver::OnWidgetClosing(Widget* widget) {
  DCHECK_EQ(widget_, widget);
  widget_->RemoveObserver(this);
  widget_ = nullptr;
  if (run_loop_.running())
    run_loop_.Quit();
}

WidgetDestroyedWaiter::WidgetDestroyedWaiter(Widget* widget) {
  widget->AddObserver(this);
}

void WidgetDestroyedWaiter::Wait() {
  run_loop_.Run();
}

void WidgetDestroyedWaiter::OnWidgetDestroyed(Widget* widget) {
  widget->RemoveObserver(this);
  run_loop_.Quit();
}

}  // namespace test
}  // namespace views
