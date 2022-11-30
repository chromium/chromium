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
  widget->CloseWithReason(Widget::ClosedReason::kUnspecified);
}

UniqueWidgetPtr::Impl::Impl() = default;

UniqueWidgetPtr::Impl::Impl(std::unique_ptr<Widget> widget)
    : widget_closer_(widget.release()) {
  widget_observation_.Observe(widget_closer_.get());
}

UniqueWidgetPtr::Impl::~Impl() = default;

Widget* UniqueWidgetPtr::Impl::Get() const {
  return widget_closer_.get();
}

void UniqueWidgetPtr::Impl::Reset() {
  if (!widget_closer_)
    return;
  widget_observation_.Reset();
  widget_closer_.reset();
}

void UniqueWidgetPtr::Impl::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(widget, widget_closer_.get());
  widget_observation_.Reset();
  widget_closer_.release();
}

}  // namespace views
