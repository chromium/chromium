// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/unique_widget_ptr.h"

#include <utility>

#include "base/scoped_observation.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

namespace {

struct WidgetAutoCloser {
  void operator()(Widget* widget) {
    widget->CloseWithReason(Widget::ClosedReason::kUnspecified);
  }
};

using WidgetAutoClosePtr = std::unique_ptr<Widget, WidgetAutoCloser>;

}  // namespace

class UniqueWidgetPtr::UniqueWidgetPtrImpl : public WidgetObserver {
 public:
  UniqueWidgetPtrImpl() = default;
  // This class acts like unique_ptr<Widget>, so this constructor is
  // deliberately implicit.
  UniqueWidgetPtrImpl(std::unique_ptr<Widget> widget)  // NOLINT
      : widget_closer_(widget.release()) {
    widget_observation_.Observe(widget_closer_.get());
  }

  UniqueWidgetPtrImpl(const UniqueWidgetPtrImpl&) = delete;

  UniqueWidgetPtrImpl& operator=(const UniqueWidgetPtrImpl&) = delete;

  ~UniqueWidgetPtrImpl() override = default;

  Widget* Get() const { return widget_closer_.get(); }

  void Reset() {
    if (!widget_closer_)
      return;
    widget_observation_.Reset();
    widget_closer_.reset();
  }

  // WidgetObserver overrides.
  void OnWidgetDestroying(Widget* widget) override {
    DCHECK_EQ(widget, widget_closer_.get());
    widget_observation_.Reset();
    widget_closer_.release();
  }

 private:
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
  WidgetAutoClosePtr widget_closer_;
};

UniqueWidgetPtr::UniqueWidgetPtr() = default;

UniqueWidgetPtr::UniqueWidgetPtr(std::unique_ptr<Widget> widget)
    : unique_widget_ptr_impl_(
          std::make_unique<UniqueWidgetPtrImpl>(std::move(widget))) {}

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

}  // namespace views
