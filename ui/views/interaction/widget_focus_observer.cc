// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/widget_focus_observer.h"

#include "base/functional/bind.h"
#include "base/logging.h"

namespace views::test {

namespace internal {

namespace {
WidgetFocusSupplierFrame* g_current_supplier_frame = nullptr;
}

WidgetFocusSupplier::WidgetFocusSupplier() = default;
WidgetFocusSupplier::~WidgetFocusSupplier() = default;

base::CallbackListSubscription
WidgetFocusSupplier::AddWidgetFocusChangedCallback(
    WidgetFocusChangedCallback callback) {
  return callbacks_.Add(callback);
}

void WidgetFocusSupplier::OnWidgetFocusChanged(gfx::NativeView focused_now) {
  callbacks_.Notify(focused_now);
}

WidgetFocusSupplierFrame::WidgetFocusSupplierFrame() {
  LOG_IF(ERROR, g_current_supplier_frame)
      << "Old WidgetFocusSupplierFrame was not cleaned up.";
  g_current_supplier_frame = this;
}

WidgetFocusSupplierFrame::~WidgetFocusSupplierFrame() {
  if (g_current_supplier_frame == this) {
    g_current_supplier_frame = nullptr;
  }
}

WidgetFocusSupplierFrame* WidgetFocusSupplierFrame::GetCurrentFrame() {
  return g_current_supplier_frame;
}

}  // namespace internal

WidgetFocusObserver::WidgetFocusObserver() {
  for (auto& supplier :
       internal::WidgetFocusSupplierFrame::GetCurrentFrame()->supplier_list()) {
    subscriptions_.emplace_back(supplier.AddWidgetFocusChangedCallback(
        base::BindRepeating(&WidgetFocusObserver::OnWidgetFocusChanged,
                            base::Unretained(this))));
  }
}
WidgetFocusObserver::~WidgetFocusObserver() = default;

void WidgetFocusObserver::OnWidgetFocusChanged(gfx::NativeView focused_now) {
  OnStateObserverStateChanged(focused_now);
}

DEFINE_STATE_IDENTIFIER_VALUE(WidgetFocusObserver, kCurrentWidgetFocus);

}  // namespace views::test
