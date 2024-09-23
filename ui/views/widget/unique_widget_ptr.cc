// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/unique_widget_ptr.h"

#include <utility>

namespace views {

UniqueWidgetPtr::UniqueWidgetPtr() = default;

UniqueWidgetPtr::UniqueWidgetPtr(std::unique_ptr<Widget> widget) {
  Init(std::move(widget));
}

UniqueWidgetPtr::UniqueWidgetPtr(UniqueWidgetPtr&& other) = default;

UniqueWidgetPtr& UniqueWidgetPtr::operator=(UniqueWidgetPtr&& other) = default;

UniqueWidgetPtr::~UniqueWidgetPtr() = default;

UniqueWidgetPtr::operator bool() const {
  return !!get();
}

Widget& UniqueWidgetPtr::operator*() const {
  return *get();
}

Widget* UniqueWidgetPtr::operator->() const {
  return get();
}

void UniqueWidgetPtr::reset() {
  unique_widget_ptr_impl_.reset();
}

Widget* UniqueWidgetPtr::get() const {
  return unique_widget_ptr_impl_ ? unique_widget_ptr_impl_->Get() : nullptr;
}

void UniqueWidgetPtr::Init(std::unique_ptr<Widget> widget) {
  unique_widget_ptr_impl_ = std::make_unique<Impl>(std::move(widget));
}

void UniqueWidgetPtr::Impl::WidgetAutoCloser::operator()(Widget* widget) {
  switch (widget->ownership()) {
    case Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET:
      // Causes the `widget`'s internal native widget to be deleted, which in
      // turn deletes the `widget` itself. Cannot delete the `widget` directly
      // since it's owned by the native widget in this case.
      widget->CloseWithReason(Widget::ClosedReason::kUnspecified);
      break;
    case Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET:
    case Widget::InitParams::CLIENT_OWNS_WIDGET:
      // Deleting the `widget` causes the native widget in both of these
      // cases to be destroyed under the hood.
      delete widget;
      break;
  }
}

UniqueWidgetPtr::Impl::Impl() = default;

UniqueWidgetPtr::Impl::Impl(std::unique_ptr<Widget> widget)
    : widget_closer_(widget.release()) {
  widget_observation_.Observe(Get());
}

UniqueWidgetPtr::Impl::~Impl() = default;

Widget* UniqueWidgetPtr::Impl::Get() const {
  // See cases 2 and 3 in `UniqueWidgetPtr::Impl::OnWidgetDestroying()`. In
  // these cases, the `widget_closer_` may still be non-null even though the
  // destruction signal has been received. In this case, the widget is alive but
  // unusable, so returning `nullptr` to the caller is appropriate.
  return received_widget_destruction_signal_ ? nullptr : widget_closer_.get();
}

void UniqueWidgetPtr::Impl::OnWidgetDestroying(Widget* widget) {
  // Releasing `widget_closer_` here when the ownership model is
  // `NATIVE_WIDGET_OWNS_WIDGET` prevents closing the widget in
  // `UniqueWidgetPtr::Impl::WidgetAutoCloser::operator()` after it's
  // destroyed (use-after-free).
  //
  // For the other ownership models, there are 3 cases that reach this point
  // in the code:
  // 1) The `UniqueWidgetPtr` went out of scope and the `widget` is being
  //    `delete`ed in `UniqueWidgetPtr::Impl::WidgetAutoCloser::operator()`.
  // 2) The caller explicitly `Close()`ed the `widget`.
  // 3) The `widget`'s internal native widget was destroyed somehow without any
  //    action from the caller.
  //
  // In case 1, there's no action needed and `widget_closer_` is already null
  // at this point; it should not be touched. In cases 2 and 3, the `widget`
  // needs to be deleted still, which will happen when the `UniqueWidgetPtr`
  // goes out of scope, so the `widget_closer_` should not be released. In
  // all cases, the `widget_closer_` should not be touched.
  widget_observation_.Reset();
  received_widget_destruction_signal_ = true;
  if (widget->ownership() == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
    DCHECK_EQ(widget, widget_closer_.get());
    widget_closer_.release();
  }
}

}  // namespace views
