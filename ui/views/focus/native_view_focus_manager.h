// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_FOCUS_NATIVE_VIEW_FOCUS_MANAGER_H_
#define UI_VIEWS_FOCUS_NATIVE_VIEW_FOCUS_MANAGER_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/scoped_observation_traits.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/views_export.h"

namespace views {

// This interface should be implemented by classes that want to be notified when
// the native focus is about to change.  Listeners implementing this interface
// will be invoked for all native focus changes across the entire Chrome
// application.  FocusChangeListeners are only called for changes within the
// children of a single top-level native-view.
class NativeViewFocusChangeListener {
 public:
  virtual void OnNativeFocusChanged(gfx::NativeView focused_now) = 0;

 protected:
  virtual ~NativeViewFocusChangeListener() = default;
};

class VIEWS_EXPORT NativeViewFocusManager {
 public:
  // Returns the singleton instance.
  static NativeViewFocusManager* GetInstance();

  NativeViewFocusManager(const NativeViewFocusManager&) = delete;
  NativeViewFocusManager& operator=(const NativeViewFocusManager&) = delete;
  ~NativeViewFocusManager();

  // Adds/removes a NativeViewFocusChangeListener |listener| to the set of
  // active listeners.
  void AddFocusChangeListener(NativeViewFocusChangeListener* listener);
  void RemoveFocusChangeListener(NativeViewFocusChangeListener* listener);

  // Called when native-focus shifts within, or off, a widget. |focused_now| is
  // null when focus is lost.
  void OnNativeFocusChanged(gfx::NativeView focused_now);

  // Enable/Disable notification of registered listeners during calls
  // to OnNativeFocusChanged.  Used to prevent unwanted focus changes from
  // propagating notifications.
  void EnableNotifications() { enabled_ = true; }
  void DisableNotifications() { enabled_ = false; }

 private:
  class Owner;
  friend class base::NoDestructor<NativeViewFocusManager>;

  NativeViewFocusManager();

  base::ObserverList<NativeViewFocusChangeListener>::Unchecked
      focus_change_listeners_;

  bool enabled_ = true;
};

// A basic helper class that is used to disable native focus change
// notifications within a scope.
class VIEWS_EXPORT AutoNativeNotificationDisabler {
 public:
  AutoNativeNotificationDisabler();
  AutoNativeNotificationDisabler(const AutoNativeNotificationDisabler&) =
      delete;
  AutoNativeNotificationDisabler& operator=(
      const AutoNativeNotificationDisabler&) = delete;

  ~AutoNativeNotificationDisabler();
};

}  // namespace views

namespace base {

// Specialization for use with base::ScopedObservation:
template <>
struct ScopedObservationTraits<views::NativeViewFocusManager,
                               views::NativeViewFocusChangeListener> {
 public:
  static void AddObserver(views::NativeViewFocusManager* source,
                          views::NativeViewFocusChangeListener* observer) {
    source->AddFocusChangeListener(observer);
  }
  static void RemoveObserver(views::NativeViewFocusManager* source,
                             views::NativeViewFocusChangeListener* observer) {
    source->RemoveFocusChangeListener(observer);
  }
};

}  // namespace base

#endif  // UI_VIEWS_FOCUS_NATIVE_VIEW_FOCUS_MANAGER_H_
