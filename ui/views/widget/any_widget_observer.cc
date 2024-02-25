// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/any_widget_observer.h"

#include <utility>

#include "base/functional/bind.h"
#include "ui/views/widget/any_widget_observer_singleton.h"
#include "ui/views/widget/widget.h"

namespace views {

AnyWidgetObserver::AnyWidgetObserver(AnyWidgetPasskey passkey)
    : AnyWidgetObserver() {}
AnyWidgetObserver::AnyWidgetObserver(test::AnyWidgetTestPasskey passkey)
    : AnyWidgetObserver() {}

AnyWidgetObserver::AnyWidgetObserver() {
  internal::AnyWidgetObserverSingleton::GetInstance()->AddObserver(this);
}

AnyWidgetObserver::~AnyWidgetObserver() {
  internal::AnyWidgetObserverSingleton::GetInstance()->RemoveObserver(this);
}

#define PROPAGATE_NOTIFICATION(method, callback)   \
  void AnyWidgetObserver::method(Widget* widget) { \
    if (callback)                                  \
      (callback).Run(widget);                      \
  }

PROPAGATE_NOTIFICATION(OnAnyWidgetInitialized, initialized_callback_)
PROPAGATE_NOTIFICATION(OnAnyWidgetShown, shown_callback_)
PROPAGATE_NOTIFICATION(OnAnyWidgetHidden, hidden_callback_)
PROPAGATE_NOTIFICATION(OnAnyWidgetClosing, closing_callback_)

#undef PROPAGATE_NOTIFICATION

NamedWidgetShownWaiter::NamedWidgetShownWaiter(AnyWidgetPasskey passkey,
                                               const std::string& name)
    : NamedWidgetShownWaiter(name) {}

NamedWidgetShownWaiter::NamedWidgetShownWaiter(
    test::AnyWidgetTestPasskey passkey,
    const std::string& name)
    : NamedWidgetShownWaiter(name) {}

NamedWidgetShownWaiter::~NamedWidgetShownWaiter() = default;

Widget* NamedWidgetShownWaiter::WaitIfNeededAndGet() {
  run_loop_.Run();
  return widget_.get();
}

NamedWidgetShownWaiter::NamedWidgetShownWaiter(const std::string& name)
    : observer_(views::AnyWidgetPasskey{}), name_(name) {
  observer_.set_shown_callback(base::BindRepeating(
      &NamedWidgetShownWaiter::OnAnyWidgetShown, base::Unretained(this)));
}

void NamedWidgetShownWaiter::OnAnyWidgetShown(Widget* widget) {
  if (widget->GetName() == name_) {
    widget_ = widget->GetWeakPtr();
    run_loop_.Quit();
  }
}

AnyWidgetPasskey::AnyWidgetPasskey() = default;

}  // namespace views
