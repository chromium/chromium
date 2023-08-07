// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/widget_focus_observer.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"

namespace views::test {

namespace internal {

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

// static
ui::FrameworkSpecificRegistrationList<WidgetFocusSupplier>&
WidgetFocusSupplier::GetRegisteredFocusSuppliers() {
  static base::NoDestructor<
      ui::FrameworkSpecificRegistrationList<WidgetFocusSupplier>>
      suppliers;
  return *suppliers.get();
}

}  // namespace internal

WidgetFocusObserver::WidgetFocusObserver() {
  for (auto& supplier :
       internal::WidgetFocusSupplier::GetRegisteredFocusSuppliers()) {
    subscriptions_.emplace_back(supplier.AddWidgetFocusChangedCallback(
        base::BindRepeating(&WidgetFocusObserver::OnWidgetFocusChanged,
                            base::Unretained(this))));
  }
}
WidgetFocusObserver::~WidgetFocusObserver() = default;

void WidgetFocusObserver::OnWidgetFocusChanged(gfx::NativeView focused_now) {
  OnStateObserverStateChanged(focused_now);
}

DEFINE_STATE_IDENTIFIER_VALUE(kCurrentWidgetFocus);

}  // namespace views::test
