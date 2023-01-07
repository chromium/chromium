// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/base_focus_rules.h"

#include "base/containers/adapters.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_delegate.h"

namespace wm {
namespace {

aura::Window* GetFocusedWindow(aura::Window* context) {
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(context);
  return focus_client ? focus_client->GetFocusedWindow() : nullptr;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BaseFocusRules, protected:

BaseFocusRules::BaseFocusRules() = default;

BaseFocusRules::~BaseFocusRules() = default;

bool BaseFocusRules::IsWindowConsideredVisibleForActivation(
    const aura::Window* window) const {
  return window->IsVisible();
}

////////////////////////////////////////////////////////////////////////////////
// BaseFocusRules, FocusRules implementation:

bool BaseFocusRules::IsToplevelWindow(const aura::Window* window) const {
  // The window must in a valid hierarchy.
  if (!window->GetRootWindow())
    return false;

  // The window must exist within a container that supports activation.
  // The window cannot be blocked by a modal transient.
  return SupportsChildActivation(window->parent());
}

bool BaseFocusRules::CanActivateWindow(const aura::Window* window) const {
  // It is possible to activate a NULL window, it is equivalent to clearing
  // activation.
  if (!window)
    return true;

  // A window that is being destroyed should not be activatable.
  if (window->is_destroying())
    return false;

  // Only toplevel windows can be activated.
  if (!IsToplevelWindow(window))
    return false;

  // The window must be visible.
  if (!IsWindowConsideredVisibleForActivation(window))
    return false;

  // The window's activation delegate must allow this window to be activated.
  if (GetActivationDelegate(window) &&
      !GetActivationDelegate(window)->ShouldActivate()) {
    return false;
  }

  // A window must be focusable to be activatable. We don't call
  // CanFocusWindow() from here because it will call back to us via
  // GetActivatableWindow().
  if (!window->CanFocus())
    return false;

  // The window cannot be blocked by a modal transient.
  return !GetModalTransient(window);
}

bool BaseFocusRules::CanFocusWindow(const aura::Window* window,
                                    const ui::Event* event) const {
  // It is possible to focus a NULL window, it is equivalent to clearing focus.
  if (!window)
    return true;

  // The focused window is always inside the active window, so windows that
  // aren't activatable can't contain the focused window.
  const aura::Window* activatable = GetActivatableWindow(window);
  if (!activatable || !activatable->Contains(window))
    return false;
  return window->CanFocus();
}

const aura::Window* BaseFocusRules::GetToplevelWindow(
    const aura::Window* window) const {
  const aura::Window* parent = window->parent();
  const aura::Window* child = window;
  while (parent) {
    if (IsToplevelWindow(child))
      return child;

    parent = parent->parent();
    child = child->parent();
  }
  return nullptr;
}

aura::Window* BaseFocusRules::GetActivatableWindow(aura::Window* window) const {
  return const_cast<aura::Window*>(
      GetActivatableWindow(const_cast<const aura::Window*>(window)));
}

aura::Window* BaseFocusRules::GetFocusableWindow(aura::Window* window) const {
  if (CanFocusWindow(window, nullptr))
    return window;

  // |window| may be in a hierarchy that is non-activatable, in which case we
  // need to cut over to the activatable hierarchy.
  aura::Window* activatable = GetActivatableWindow(window);
  if (!activatable) {
    // There may not be a related activatable hierarchy to cut over to, in which
    // case we try an unrelated one.
    aura::Window* toplevel = GetToplevelWindow(window);
    if (toplevel)
      activatable = GetNextActivatableWindow(toplevel);
    if (!activatable)
      return nullptr;
  }

  if (!activatable->Contains(window)) {
    // If there's already a child window focused in the activatable hierarchy,
    // just use that (i.e. don't shift focus), otherwise we need to at least cut
    // over to the activatable hierarchy.
    aura::Window* focused = GetFocusedWindow(activatable);
    return activatable->Contains(focused) ? focused : activatable;
  }

  while (window && !CanFocusWindow(window, nullptr))
    window = window->parent();
  return window;
}

aura::Window* BaseFocusRules::GetNextActivatableWindow(
    aura::Window* ignore) const {
  DCHECK(ignore);

  // Can be called from the RootWindow's destruction, which has a NULL parent.
  if (!ignore->parent())
    return nullptr;

  // In the basic scenarios handled by BasicFocusRules, the pool of activatable
  // windows is limited to the |ignore|'s siblings.
  const aura::Window::Windows& siblings = ignore->parent()->children();
  DCHECK(!siblings.empty());

  for (aura::Window* cur : base::Reversed(siblings)) {
    if (cur == ignore)
      continue;
    if (CanActivateWindow(cur))
      return cur;
  }
  return nullptr;
}

const aura::Window* BaseFocusRules::GetActivatableWindow(
    const aura::Window* window) const {
  const aura::Window* parent = window->parent();
  const aura::Window* child = window;
  while (parent) {
    if (CanActivateWindow(child))
      return child;

    // CanActivateWindow() above will return false if |child| is blocked by a
    // modal transient. In this case the modal is or contains the activatable
    // window. We recurse because the modal may itself be blocked by a modal
    // transient.
    const aura::Window* modal_transient = GetModalTransient(child);
    if (modal_transient)
      return GetActivatableWindow(modal_transient);

    if (wm::GetTransientParent(child)) {
      // To avoid infinite recursion, if |child| has a transient parent
      // whose own modal transient is |child| itself, just return |child|.
      const aura::Window* parent_modal_transient =
          GetModalTransient(wm::GetTransientParent(child));
      if (parent_modal_transient == child)
        return child;

      return GetActivatableWindow(wm::GetTransientParent(child));
    }

    parent = parent->parent();
    child = child->parent();
  }
  return nullptr;
}

}  // namespace wm
