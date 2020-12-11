// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_FOCUS_CONTROLLER_H_
#define UI_WM_CORE_FOCUS_CONTROLLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/core/wm_core_export.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace wm {

class FocusRules;

// FocusController handles focus and activation changes for an environment
// encompassing one or more RootWindows. Within an environment there can be
// only one focused and one active window at a time. When focus or activation
// changes notifications are sent using the
// aura::client::Focus/ActivationChangeObserver interfaces.
// Changes to focus and activation can be from three sources. The source can be
// determined by the ActivationReason parameter in
// ActivationChangeObserver::OnWindowActivated(...).
// . ActivationReason::ACTIVATION_CLIENT: The Aura Client API (implemented here
//   in ActivationClient). (The FocusController must be set as the
//   ActivationClient implementation for all RootWindows).
// . ActivationReason::INPUT_EVENT: Input events (implemented here in
//   ui::EventHandler). (The FocusController must be registered as a pre-target
//   handler for the applicable environment owner, either a RootWindow or
//   another type).
// . ActivationReason::WINDOW_DISPOSITION_CHANGED: Window disposition changes
//   (implemented here in aura::WindowObserver). (The FocusController registers
//   itself as an observer of the active and focused windows).
class WM_CORE_EXPORT FocusController : public ActivationClient,
                                       public aura::client::FocusClient,
                                       public ui::EventHandler,
                                       public aura::WindowObserver {
 public:
  // |rules| cannot be NULL.
  explicit FocusController(FocusRules* rules);
  ~FocusController() override;

  // Overridden from ActivationClient:
  void AddObserver(ActivationChangeObserver* observer) override;
  void RemoveObserver(ActivationChangeObserver* observer) override;
  void ActivateWindow(aura::Window* window) override;
  void DeactivateWindow(aura::Window* window) override;
  const aura::Window* GetActiveWindow() const override;
  aura::Window* GetActivatableWindow(aura::Window* window) const override;
  const aura::Window* GetToplevelWindow(
      const aura::Window* window) const override;
  bool CanActivateWindow(const aura::Window* window) const override;

  // Overridden from aura::client::FocusClient:
  void AddObserver(aura::client::FocusChangeObserver* observer) override;
  void RemoveObserver(aura::client::FocusChangeObserver* observer) override;
  void FocusWindow(aura::Window* window) override;
  void ResetFocusWithinActiveWindow(aura::Window* window) override;
  aura::Window* GetFocusedWindow() override;

 protected:
  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;

 private:
  // Internal implementation that coordinates window focus and activation
  // changes.
  void FocusAndActivateWindow(ActivationChangeObserver::ActivationReason reason,
                              aura::Window* window);

  // Internal implementation that sets the focused window, fires events etc.
  // This function must be called with a valid focusable window.
  void SetFocusedWindow(aura::Window* window);

  // Internal implementation that sets the active window, fires events etc.
  // This function must be called with a valid |activatable_window|.
  // |requested_window| refers to the window that was passed in to an external
  // request (e.g. FocusWindow or ActivateWindow). It may be NULL, e.g. if
  // SetActiveWindow was not called by an external request. |activatable_window|
  // refers to the actual window to be activated, which may be different.
  // Returns true if activation should proceed, or false if activation was
  // interrupted, e.g. by the destruction of the window gaining activation
  // during the process, and therefore activation should be aborted.
  bool SetActiveWindow(ActivationChangeObserver::ActivationReason reason,
                       aura::Window* requested_window,
                       aura::Window* activatable_window);

  // Stack the |active_window_| on top of the window stack. This function is
  // called when activating a window or re-activating the current active window.
  void StackActiveWindow();

  // Called when a window's disposition changed such that it and its hierarchy
  // are no longer focusable/activatable. |next| is a valid window that is used
  // as a starting point for finding a window to focus next based on rules.
  void WindowLostFocusFromDispositionChange(aura::Window* window,
                                            aura::Window* next);

  // Called when an attempt is made to focus or activate a window via an input
  // event targeted at that window. Rules determine the best focusable window
  // for the input window.
  void WindowFocusedFromInputEvent(aura::Window* window,
                                   const ui::Event* event);

  aura::Window* active_window_ = nullptr;
  aura::Window* focused_window_ = nullptr;

  bool updating_focus_ = false;

  // An optional value. It is set to the window being activated and is unset
  // after it is activated.
  base::Optional<aura::Window*> pending_activation_;

  std::unique_ptr<FocusRules> rules_;

  base::ObserverList<ActivationChangeObserver>::Unchecked activation_observers_;
  base::ObserverList<aura::client::FocusChangeObserver>::Unchecked
      focus_observers_;

  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_{this};

  DISALLOW_COPY_AND_ASSIGN(FocusController);
};

}  // namespace wm

#endif  // UI_WM_CORE_FOCUS_CONTROLLER_H_
