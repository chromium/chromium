// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_PUBLIC_ACTIVATION_CLIENT_H_
#define UI_WM_PUBLIC_ACTIVATION_CLIENT_H_

#include "ui/wm/public/wm_public_export.h"

namespace aura {
class Window;
}

namespace wm {
class ActivationChangeObserver;

// An interface implemented by an object that manages window activation.
class WM_PUBLIC_EXPORT ActivationClient {
 public:
  // Adds/Removes ActivationChangeObservers.
  virtual void AddObserver(ActivationChangeObserver* observer) = 0;
  virtual void RemoveObserver(ActivationChangeObserver* observer) = 0;

  // Activates |window|. If |window| is NULL, nothing happens.
  virtual void ActivateWindow(aura::Window* window) = 0;

  // Deactivates |window|. What (if anything) is activated next is up to the
  // client. If |window| is NULL, nothing happens.
  virtual void DeactivateWindow(aura::Window* window) = 0;

  // Retrieves the active window, or NULL if there is none.
  aura::Window* GetActiveWindow() {
    return const_cast<aura::Window*>(
        const_cast<const ActivationClient*>(this)->GetActiveWindow());
  }
  virtual const aura::Window* GetActiveWindow() const = 0;

  // Retrieves the activatable window for |window|, or NULL if there is none.
  // Note that this is often but not always the toplevel window (see
  // GetToplevelWindow() below), as the toplevel window may not be activatable
  // (for example it may be blocked by a modal transient, or some other
  // condition).
  virtual aura::Window* GetActivatableWindow(aura::Window* window) const = 0;

  // Retrieves the toplevel window for |window|, or NULL if there is none.
  virtual const aura::Window* GetToplevelWindow(
      const aura::Window* window) const = 0;

  // Returns true if |window| can be activated, false otherwise. If |window| has
  // a modal child it can not be activated.
  virtual bool CanActivateWindow(const aura::Window* window) const = 0;

 protected:
  virtual ~ActivationClient() {}
};

// Sets/Gets the activation client on the root Window.
WM_PUBLIC_EXPORT void SetActivationClient(aura::Window* root_window,
                                          ActivationClient* client);
WM_PUBLIC_EXPORT ActivationClient* GetActivationClient(
    aura::Window* root_window);
WM_PUBLIC_EXPORT const ActivationClient* GetActivationClient(
    const aura::Window* root_window);

// Some types of transient window are only visible when active.
// The transient parents of these windows may have visual appearance properties
// that differ from transient parents that can be deactivated.
// The presence of this property implies these traits.
// TODO(beng): currently the UI framework (views) implements the actual
//             close-on-deactivate component of this feature but it should be
//             possible to implement in the aura client.
WM_PUBLIC_EXPORT void SetHideOnDeactivate(aura::Window* window,
                                          bool hide_on_deactivate);
WM_PUBLIC_EXPORT bool GetHideOnDeactivate(aura::Window* window);

}  // namespace wm

#endif  // UI_WM_PUBLIC_ACTIVATION_CLIENT_H_
