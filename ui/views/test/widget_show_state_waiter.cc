// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_show_state_waiter.h"

#include "base/run_loop.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views::test {

namespace {

bool IsWidgetInShowState(Widget* widget,
                         ui::mojom::WindowShowState show_state) {
  switch (show_state) {
    case ui::mojom::WindowShowState::kMinimized:
      return widget->IsMinimized();
    case ui::mojom::WindowShowState::kMaximized:
      return widget->IsMaximized();
    default:
      NOTIMPLEMENTED();
      return false;
  }
}

// Use in tests to wait until a Widget's show state changes to a particular
// value. To use create and call Wait().
class WidgetShowStateWaiter : public WidgetObserver {
 public:
  WidgetShowStateWaiter(Widget* widget, ui::mojom::WindowShowState show_state)
      : show_state_(show_state) {
    CHECK(show_state == ui::mojom::WindowShowState::kMinimized ||
          show_state == ui::mojom::WindowShowState::kMaximized)
        << "WidgetShowStateWaiter only supports minimized or maximized state "
           "for now.";
    if (IsWidgetInShowState(widget, show_state_)) {
      observed_ = true;
      return;
    }
    widget_observation_.Observe(widget);
  }

  WidgetShowStateWaiter(const WidgetShowStateWaiter&) = delete;
  WidgetShowStateWaiter& operator=(const WidgetShowStateWaiter&) = delete;

  ~WidgetShowStateWaiter() override = default;

  void Wait() {
    if (!observed_) {
      run_loop_.Run();
    }
  }

 private:
  // views::WidgetObserver override:
  void OnWidgetShowStateChanged(Widget* widget) override {
    if (!IsWidgetInShowState(widget, show_state_)) {
      return;
    }

    observed_ = true;
    widget_observation_.Reset();
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

  bool observed_ = false;
  ui::mojom::WindowShowState show_state_;

  base::RunLoop run_loop_;
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

}  // namespace

void WaitForWidgetShowState(Widget* widget,
                            ui::mojom::WindowShowState show_state) {
  WidgetShowStateWaiter waiter(widget, show_state);
  waiter.Wait();
}

}  // namespace views::test
