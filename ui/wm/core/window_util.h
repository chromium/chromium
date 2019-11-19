// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WINDOW_UTIL_H_
#define UI_WM_CORE_WINDOW_UTIL_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "ui/base/ui_base_types.h"
#include "ui/wm/core/wm_core_export.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
class LayerOwner;
class LayerTreeOwner;
}

namespace wm {

WM_CORE_EXPORT void ActivateWindow(aura::Window* window);
WM_CORE_EXPORT void DeactivateWindow(aura::Window* window);
WM_CORE_EXPORT bool IsActiveWindow(const aura::Window* window);
WM_CORE_EXPORT bool CanActivateWindow(const aura::Window* window);
WM_CORE_EXPORT void SetWindowFullscreen(aura::Window* window, bool fullscreen);

// Returns true if |window|'s show state is |state|.
WM_CORE_EXPORT bool WindowStateIs(const aura::Window* window,
                                  ui::WindowShowState state);

// Sets the window state to |state|.
WM_CORE_EXPORT void SetWindowState(aura::Window* window,
                                   ui::WindowShowState state);

// Changes a window's state to its pre-minimized state.
WM_CORE_EXPORT void Unminimize(aura::Window* window);

// Retrieves the activatable window for |window|. If |window| is activatable,
// this will just return it, otherwise it will climb the parent/transient parent
// chain looking for a window that is activatable, per the ActivationClient.
// If you're looking for a function to get the activatable "top level" window,
// this is probably the function you're looking for.
WM_CORE_EXPORT aura::Window* GetActivatableWindow(aura::Window* window);

// Retrieves the toplevel window for |window|. The ActivationClient makes this
// determination.
WM_CORE_EXPORT aura::Window* GetToplevelWindow(aura::Window* window);
WM_CORE_EXPORT const aura::Window* GetToplevelWindow(
    const aura::Window* window);

// Returns the existing Layer for |root| (and all its descendants) and creates
// a new layer for |root| and all its descendants. This is intended for
// animations that want to animate between the existing visuals and a new state.
//
// As a result of this |root| has freshly created layers, meaning the layers
// have not yet been painted to.
WM_CORE_EXPORT std::unique_ptr<ui::LayerTreeOwner> RecreateLayers(
    ui::LayerOwner* root);

using MapLayerFunc =
    base::RepeatingCallback<std::unique_ptr<ui::Layer>(ui::LayerOwner*)>;

// Maps |map_func| over each layer of the layer tree and returns a copy of the
// layer tree. The recursion stops at the level when |map_func| returns nullptr
// on the owner's layer. MapLayers might return nullptr when |map_func| returns
// nullptr on the root layer's owner.
WM_CORE_EXPORT std::unique_ptr<ui::LayerTreeOwner> RecreateLayersWithClosure(
    ui::LayerOwner* root,
    const MapLayerFunc& map_func);

// Returns a layer tree that mirrors |root|. Used for live window previews. If
// |sync_bounds| is true, the bounds of all mirror layers except the root are
// synchronized. See |sync_bounds_with_source_| in ui::Layer.
WM_CORE_EXPORT std::unique_ptr<ui::LayerTreeOwner> MirrorLayers(
    ui::LayerOwner* root,
    bool sync_bounds);

// Convenience functions that get the TransientWindowManager for the window and
// redirect appropriately. These are preferable to calling functions on
// TransientWindowManager as they handle the appropriate null checks.
WM_CORE_EXPORT aura::Window* GetTransientParent(aura::Window* window);
WM_CORE_EXPORT const aura::Window* GetTransientParent(
    const aura::Window* window);
WM_CORE_EXPORT const std::vector<aura::Window*>& GetTransientChildren(
    const aura::Window* window);
WM_CORE_EXPORT void AddTransientChild(aura::Window* parent,
                                      aura::Window* child);
WM_CORE_EXPORT void RemoveTransientChild(aura::Window* parent,
                                         aura::Window* child);
WM_CORE_EXPORT aura::Window* GetTransientRoot(aura::Window* window);

// Returns true if |window| has |ancestor| as a transient ancestor. A transient
// ancestor is found by following the transient parent chain of the window.
WM_CORE_EXPORT bool HasTransientAncestor(const aura::Window* window,
                                         const aura::Window* ancestor);

// Snap the window's layer to physical pixel boundary.
WM_CORE_EXPORT void SnapWindowToPixelBoundary(aura::Window* window);
}  // namespace wm

#endif  // UI_WM_CORE_WINDOW_UTIL_H_
