// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_WIDGET_FOCUS_OBSERVER_H_
#define UI_VIEWS_INTERACTION_WIDGET_FOCUS_OBSERVER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

namespace views::test {

namespace internal {

// Represents a singleton object that observes widget activation in some way.
// Useful because not all browser activations register as low-level widget
// activations, and there needs to be a way to track non-browser windows too, so
// there is no single system that can provide this information.
//
// Subclasses should be private to a specific Interactive[X]TestApi
// implementation, and be registered on the test's `WidgetFocusSupplierFrame`.
class WidgetFocusSupplier : public ui::FrameworkSpecificImplementation {
 public:
  WidgetFocusSupplier();
  ~WidgetFocusSupplier() override;

  // Allows a specific WidgetFocusObserver to register for callbacks.
  using WidgetFocusChangedCallback =
      base::RepeatingCallback<void(gfx::NativeView)>;
  base::CallbackListSubscription AddWidgetFocusChangedCallback(
      WidgetFocusChangedCallback callback);

 protected:
  // Derived classes should call this when the focus changes.
  void OnWidgetFocusChanged(gfx::NativeView focused_now);

  // Used to retrieve a set of widgets. Results from multiple suppliers may be
  // combined to get a full set.
  virtual Widget::Widgets GetAllWidgets() const = 0;

 private:
  friend class WidgetFocusSupplierFrame;

  base::RepeatingCallbackList<void(gfx::NativeView)> callbacks_;
};

// Creates a frame in which WidgetFocusSuppliers can be registered.
// Use `GetCurrentFrame()->supplier_list()`.
class WidgetFocusSupplierFrame {
 public:
  WidgetFocusSupplierFrame();
  ~WidgetFocusSupplierFrame();

  WidgetFocusSupplierFrame(const WidgetFocusSupplierFrame&) = delete;
  void operator=(const WidgetFocusSupplierFrame&) = delete;

  // Returns the current frame (there should only be one).
  static WidgetFocusSupplierFrame* GetCurrentFrame();

  using SupplierList =
      ui::FrameworkSpecificRegistrationList<WidgetFocusSupplier>;

  SupplierList& supplier_list() { return supplier_list_; }

  // Returns the current active widget, if it can be determined, or null
  // otherwise.
  Widget* GetActiveWidget();

 private:
  // The actual list of widget focus suppliers.
  SupplierList supplier_list_;
};

}  // namespace internal

// Tracks widget focus as a `StateObserver`. Use ObserveState and WaitForState.
// Can only be created inside of a `WidgetFocusSupplierFrame`.
class WidgetFocusObserver : public ui::test::StateObserver<gfx::NativeView> {
 public:
  WidgetFocusObserver();
  ~WidgetFocusObserver() override;

  // ui::test::StateObserver:
  gfx::NativeView GetStateObserverInitialState() const override;

 private:
  void OnWidgetFocusChanged(gfx::NativeView focused_now);

  std::vector<base::CallbackListSubscription> subscriptions_;
};

// Since there is only one WidgetFocusManager, there only ever needs to be one
// WidgetFocusObserver.
DECLARE_STATE_IDENTIFIER_VALUE(WidgetFocusObserver, kCurrentWidgetFocus);

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_WIDGET_FOCUS_OBSERVER_H_
