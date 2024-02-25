// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_DEFAULT_ACTIVATION_CLIENT_H_
#define UI_WM_CORE_DEFAULT_ACTIVATION_CLIENT_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace wm {

class ActivationChangeObserver;

// Simple ActivationClient implementation for use by tests and other targets
// that just need basic behavior (e.g. activate windows whenever requested,
// restack windows at the top when they're activated, etc.). This object deletes
// itself when the root window it is associated with is destroyed.
class COMPONENT_EXPORT(UI_WM) DefaultActivationClient
    : public ActivationClient,
      public aura::WindowObserver {
 public:
  explicit DefaultActivationClient(aura::Window* root_window);

  DefaultActivationClient(const DefaultActivationClient&) = delete;
  DefaultActivationClient& operator=(const DefaultActivationClient&) = delete;

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

  // Overridden from WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  class Deleter;

  ~DefaultActivationClient() override;
  void RemoveActiveWindow(aura::Window* window);

  void ActivateWindowImpl(ActivationChangeObserver::ActivationReason reason,
                          aura::Window* window);

  void ClearActiveWindows();

  // This class explicitly does NOT store the active window in a window property
  // to make sure that ActivationChangeObserver is not treated as part of the
  // aura API. Assumptions to that end will cause tests that use this client to
  // fail.
  std::vector<raw_ptr<aura::Window, VectorExperimental>> active_windows_;

  // The window which was active before the currently active one.
  raw_ptr<aura::Window, DanglingUntriaged> last_active_;

  base::ObserverList<ActivationChangeObserver> observers_;
};

}  // namespace wm

#endif  // UI_WM_CORE_DEFAULT_ACTIVATION_CLIENT_H_
