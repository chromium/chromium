// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_MOCK_ACTIVATION_CONTROLLER_H_
#define UI_VIEWS_TEST_MOCK_ACTIVATION_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/widget_activation_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
namespace test {

// MockActivationController class provides platform-independent widget
// activation by emulating the window stacking, activation, and deactivation.
//
// Normally, the underlying platform/OS handles activation/deactivation, but
// relying on such an environment prevents us from running multiple tests in
// parallel because there is only one active window at a time, and activating a
// window in one test may deactivate a window in another test. This class allows
// such tests to run in parallel.
//
// Caveats:
// 1) Do not use this class directly in test code. Just instantiating this class
// should enable the emulation.
// 2) This can only handle a window created by views::Widget. If you create a
// native window (e.g. aura::Window) without the views framework, it will not be
// emulated. Such a case should not exist, as a window that should be activated
// should have UI elements.
// 3) Mac does not support Widget::Deactivate, but deactivation can still
// happen by hiding a Widget. This class emulates this scenario as well.
class MockActivationController : public views::WidgetObserver,
                                 public WidgetActivationDelegate {
 public:
  MockActivationController();
  MockActivationController(const MockActivationController&) = delete;
  MockActivationController operator=(const MockActivationController&) = delete;
  ~MockActivationController() override;

  void MaybeActivate(Widget* widget, bool activate) override;
  void Deactivate(Widget* widget) override;
  bool IsActive(const Widget* widget) override;

 private:
  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override;

  using WidgetList = std::vector<raw_ptr<Widget, VectorExperimental>>;

  WidgetList::reverse_iterator FindActivatableWidget();

  WidgetList widgets_;
  raw_ptr<Widget> active_widget_ = nullptr;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_MOCK_ACTIVATION_CONTROLLER_H_
