// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WINDOW_UTIL_H_
#define UI_WM_CORE_WINDOW_UTIL_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/display/types/display_constants.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
class LayerOwner;
class LayerTreeOwner;
}  // namespace ui

namespace wm {

COMPONENT_EXPORT(UI_WM) void ActivateWindow(aura::Window* window);
COMPONENT_EXPORT(UI_WM) void DeactivateWindow(aura::Window* window);
COMPONENT_EXPORT(UI_WM) bool IsActiveWindow(const aura::Window* window);
COMPONENT_EXPORT(UI_WM) bool CanActivateWindow(const aura::Window* window);
COMPONENT_EXPORT(UI_WM)
void SetWindowFullscreen(
    aura::Window* window,
    bool fullscreen,
    int64_t target_display_id = display::kInvalidDisplayId);

// Returns true if |window|'s show state is |state|.
COMPONENT_EXPORT(UI_WM)
bool WindowStateIs(const aura::Window* window,
                   ui::mojom::WindowShowState state);

// Returns |window|'s current show state.
COMPONENT_EXPORT(UI_WM)
ui::mojom::WindowShowState GetWindowState(const aura::Window* window);

// Sets the window state to |state|.
COMPONENT_EXPORT(UI_WM)
void SetWindowState(aura::Window* window, ui::mojom::WindowShowState state);

// Restores the window state from the current state to its previous applicable
// state. As an example, if the current state is minimized, Restore() will
// change the window's sate to its applicable pre-minimized state, which is the
// same as calling Unminimize() function.
COMPONENT_EXPORT(UI_WM) void Restore(aura::Window* window);

// Changes a window's state to its pre-minimized state.
COMPONENT_EXPORT(UI_WM) void Unminimize(aura::Window* window);

// Retrieves the activatable window for |window|. If |window| is activatable,
// this will just return it, otherwise it will climb the parent/transient parent
// chain looking for a window that is activatable, per the ActivationClient.
// If you're looking for a function to get the activatable "top level" window,
// this is probably the function you're looking for.
COMPONENT_EXPORT(UI_WM)
aura::Window* GetActivatableWindow(aura::Window* window);

// Retrieves the toplevel window for |window|. The ActivationClient makes this
// determination.
COMPONENT_EXPORT(UI_WM) aura::Window* GetToplevelWindow(aura::Window* window);
COMPONENT_EXPORT(UI_WM)
const aura::Window* GetToplevelWindow(const aura::Window* window);

// Returns the existing Layer for |root| (and all its descendants) and creates
// a new layer for |root| and all its descendants. This is intended for
// animations that want to animate between the existing visuals and a new state.
//
// As a result of this |root| has freshly created layers, meaning the layers
// have not yet been painted to.
COMPONENT_EXPORT(UI_WM)
std::unique_ptr<ui::LayerTreeOwner> RecreateLayers(ui::LayerOwner* root);

using MapLayerFunc =
    base::RepeatingCallback<std::unique_ptr<ui::Layer>(ui::LayerOwner*)>;

// Maps |map_func| over each layer of the layer tree and returns a copy of the
// layer tree. The recursion stops at the level when |map_func| returns nullptr
// on the owner's layer. MapLayers might return nullptr when |map_func| returns
// nullptr on the root layer's owner.
COMPONENT_EXPORT(UI_WM)
std::unique_ptr<ui::LayerTreeOwner> RecreateLayersWithClosure(
    ui::LayerOwner* root,
    const MapLayerFunc& map_func);

// Returns a layer tree that mirrors |root|. Used for live window previews. If
// |sync_bounds| is true, the bounds of all mirror layers except the root are
// synchronized. See |sync_bounds_with_source_| in ui::Layer.
COMPONENT_EXPORT(UI_WM)
std::unique_ptr<ui::LayerTreeOwner> MirrorLayers(ui::LayerOwner* root,
                                                 bool sync_bounds);

// Convenience functions that get the TransientWindowManager for the window and
// redirect appropriately. These are preferable to calling functions on
// TransientWindowManager as they handle the appropriate null checks.
COMPONENT_EXPORT(UI_WM) aura::Window* GetTransientParent(aura::Window* window);
COMPONENT_EXPORT(UI_WM)
const aura::Window* GetTransientParent(const aura::Window* window);
COMPONENT_EXPORT(UI_WM)
const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
GetTransientChildren(const aura::Window* window);
COMPONENT_EXPORT(UI_WM)
void AddTransientChild(aura::Window* parent, aura::Window* child);
COMPONENT_EXPORT(UI_WM)
void RemoveTransientChild(aura::Window* parent, aura::Window* child);
COMPONENT_EXPORT(UI_WM) aura::Window* GetTransientRoot(aura::Window* window);

// Returns true if |window| has |ancestor| as a transient ancestor. A transient
// ancestor is found by following the transient parent chain of the window.
COMPONENT_EXPORT(UI_WM)
bool HasTransientAncestor(const aura::Window* window,
                          const aura::Window* ancestor);

// Snap the window's layer to physical pixel boundary.
COMPONENT_EXPORT(UI_WM) void SnapWindowToPixelBoundary(aura::Window* window);

}  // namespace wm

#endif  // UI_WM_CORE_WINDOW_UTIL_H_
