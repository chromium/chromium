// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_ACTIVATION_CHANGE_OBSERVER_H_
#define UI_WM_PUBLIC_ACTIVATION_CHANGE_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/wm/public/wm_public_export.h"

namespace aura {
class Window;
}

namespace wm {

class WM_PUBLIC_EXPORT ActivationChangeObserver : public base::CheckedObserver {
 public:
  // The reason or cause of a window activation change.
  enum class ActivationReason {
    // When a window is activated due to a call to the ActivationClient API.
    ACTIVATION_CLIENT,
    // When a user clicks or taps a window in the 2-dimensional screen space or
    // when a user moves a mouse over a window while focus follows cursor is
    // enabled.
    INPUT_EVENT,
    // When a new window is activated as a side effect of a window
    // disposition changing.
    WINDOW_DISPOSITION_CHANGED,
  };

  // Called when |gaining_active| will gain activation, or there is no active
  // window (|gaining_active| is NULL in this case.) |losing_active| refers to
  // the previous active window or NULL if there was no previously active
  // window. |reason| specifies the cause of the activation change.
  virtual void OnWindowActivating(ActivationReason reason,
                                  aura::Window* gaining_active,
                                  aura::Window* losing_active) {}

  // Called when |gained_active| gains activation, or there is no active window
  // (|gained_active| is NULL in this case.) |lost_active| refers to the
  // previous active window or NULL if there was no previously active
  // window. |reason| specifies the cause of the activation change.
  virtual void OnWindowActivated(ActivationReason reason,
                                 aura::Window* gained_active,
                                 aura::Window* lost_active) = 0;

  // Called when during window activation the currently active window is
  // selected for activation. This can happen when a window requested for
  // activation cannot be activated because a system modal window is active.
  virtual void OnAttemptToReactivateWindow(aura::Window* request_active,
                                           aura::Window* actual_active) {}

  ~ActivationChangeObserver() override;
};

// Gets/Sets the ActivationChangeObserver for a specific window. This observer
// is notified after the ActivationClient notifies all registered observers.
WM_PUBLIC_EXPORT void SetActivationChangeObserver(
    aura::Window* window,
    ActivationChangeObserver* observer);
WM_PUBLIC_EXPORT ActivationChangeObserver* GetActivationChangeObserver(
    aura::Window* window);

}  // namespace wm

#endif  // UI_WM_PUBLIC_ACTIVATION_CHANGE_OBSERVER_H_
