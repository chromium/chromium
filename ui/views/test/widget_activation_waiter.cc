// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_activation_waiter.h"

#include "base/run_loop.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if BUILDFLAG(IS_MAC)
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#endif

namespace views {

namespace test {

namespace {

// Use in tests to wait until a Widget's activation change to a particular
// value. To use create and call Wait().
class WidgetActivationWaiter : public WidgetObserver {
 public:
  WidgetActivationWaiter(Widget* widget, bool active)
      : active_(active), widget_(*widget) {
    if (active == widget->native_widget_active()) {
      observed_ = true;
      return;
    }
    widget_observation_.Observe(widget);
  }

  WidgetActivationWaiter(const WidgetActivationWaiter&) = delete;
  WidgetActivationWaiter& operator=(const WidgetActivationWaiter&) = delete;

  ~WidgetActivationWaiter() override = default;

  // Returns when the active status matches that supplied to the constructor. If
  // the active status does not match that of the constructor a RunLoop is used
  // until the active status matches, otherwise this returns immediately.
  void Wait() {
    if (!observed_) {
#if BUILDFLAG(IS_MAC)
      // Some tests waiting on widget creation + activation are flaky due to
      // timeout. crbug.com/1327590.
      const base::test::ScopedRunLoopTimeout increased_run_timeout(
          FROM_HERE, TestTimeouts::action_max_timeout(),
          base::BindLambdaForTesting([&]() {
            return "Requested activation state: " +
                   base::NumberToString(active_) + ", actual: " +
                   base::NumberToString(widget_->native_widget_active());
          }));
#endif
      run_loop_.Run();
    }
  }

 private:
  // views::WidgetObserver override:
  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active_ != active) {
      return;
    }

    observed_ = true;
    widget_observation_.Reset();
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

  bool observed_ = false;
  bool active_;
  const raw_ref<Widget> widget_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  base::ScopedObservation<Widget, WidgetObserver> widget_observation_{this};
};

}  // namespace

void WaitForWidgetActive(Widget* widget, bool active) {
  WidgetActivationWaiter waiter(widget, active);
  waiter.Wait();
}

}  // namespace test

}  // namespace views
