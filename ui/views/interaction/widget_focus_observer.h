// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_WIDGET_FOCUS_OBSERVER_H_
#define UI_VIEWS_INTERACTION_WIDGET_FOCUS_OBSERVER_H_

#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/gfx/native_widget_types.h"

namespace views::test {

namespace internal {

// Represents a singleton object that observes widget activation in some way.
// Useful because not all browser activations register as low-level widget
// activations, and there needs to be a way to track non-browser windows too, so
// there is no single system that can provide this information.
//
// Subclasses should be private to a specific Interactive[X]TestApi
// implementation.
class WidgetFocusSupplier : public ui::FrameworkSpecificImplementation {
 public:
  WidgetFocusSupplier();
  ~WidgetFocusSupplier() override;

  // Allows a specific WidgetFocusObserver to register for callbacks.
  using WidgetFocusChangedCallback =
      base::RepeatingCallback<void(gfx::NativeView)>;
  base::CallbackListSubscription AddWidgetFocusChangedCallback(
      WidgetFocusChangedCallback callback);

  // Returns a singleton list of suppliers.
  static ui::FrameworkSpecificRegistrationList<WidgetFocusSupplier>&
  GetRegisteredFocusSuppliers();

  // Registers a new supplier if no supplier of that type is present yet.
  template <typename T, typename... Args>
  static void MaybeRegisterFocusSupplier(Args&&... args) {
    GetRegisteredFocusSuppliers().MaybeRegister<T>(std::forward<Args>(args)...);
  }

 protected:
  // Derived classes should call this when the focus changes.
  void OnWidgetFocusChanged(gfx::NativeView focused_now);

 private:
  base::RepeatingCallbackList<void(gfx::NativeView)> callbacks_;
};

}  // namespace internal

// Tracks widget focus as a StateObserver. Use ObserveState and WaitForState.
class WidgetFocusObserver : public ui::test::StateObserver<gfx::NativeView> {
 public:
  WidgetFocusObserver();
  ~WidgetFocusObserver() override;

 private:
  void OnWidgetFocusChanged(gfx::NativeView focused_now);

  std::vector<base::CallbackListSubscription> subscriptions_;
};

// Since there is only one WidgetFocusManager, there only ever needs to be one
// WidgetFocusObserver.
DECLARE_STATE_IDENTIFIER_VALUE(WidgetFocusObserver, kCurrentWidgetFocus);

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_WIDGET_FOCUS_OBSERVER_H_
