// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_util.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/public/activation_client.h"

namespace {

// Invokes |map_func| on all the children of |to_clone|, adding the newly
// cloned children to |parent|. If |map_func| returns nullptr on
// the layer owner, all its layer's children will not be cloned.
//
// WARNING: It is assumed that |parent| is ultimately owned by a LayerTreeOwner.
void CloneChildren(ui::Layer* to_clone,
                   ui::Layer* parent,
                   const wm::MapLayerFunc& map_func) {
  typedef std::vector<raw_ptr<ui::Layer, VectorExperimental>> Layers;
  // Make a copy of the children since RecreateLayer() mutates it.
  Layers children(to_clone->children());
  for (Layers::const_iterator i = children.begin(); i != children.end(); ++i) {
    ui::LayerOwner* owner = (*i)->owner();
    ui::Layer* old_layer = owner ? map_func.Run(owner).release() : nullptr;
    if (old_layer) {
      parent->Add(old_layer);
      // RecreateLayer() moves the existing children to the new layer. Create a
      // copy of those.
      CloneChildren(owner->layer(), old_layer, map_func);
    }
  }
  parent->set_no_mutation(true);
}

// Invokes Mirror() on all the children of |to_mirror|, adding the newly cloned
// children to |parent|.
//
// WARNING: It is assumed that |parent| is ultimately owned by a LayerTreeOwner.
void MirrorChildren(ui::Layer* to_mirror,
                    ui::Layer* parent,
                    bool sync_bounds) {
  for (ui::Layer* child : to_mirror->children()) {
    ui::Layer* mirror = child->Mirror().release();
    mirror->set_sync_bounds_with_source(sync_bounds);
    parent->Add(mirror);
    MirrorChildren(child, mirror, sync_bounds);
  }
}

}  // namespace

namespace wm {

void ActivateWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  GetActivationClient(window->GetRootWindow())->ActivateWindow(window);
}

void DeactivateWindow(aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());
  GetActivationClient(window->GetRootWindow())->DeactivateWindow(window);
}

bool IsActiveWindow(const aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  const ActivationClient* client = GetActivationClient(window->GetRootWindow());
  return client && client->GetActiveWindow() == window;
}

bool CanActivateWindow(const aura::Window* window) {
  DCHECK(window);
  if (!window->GetRootWindow())
    return false;
  const ActivationClient* client = GetActivationClient(window->GetRootWindow());
  return client && client->CanActivateWindow(window);
}

void SetWindowFullscreen(aura::Window* window,
                         bool fullscreen,
                         int64_t target_display_id) {
  DCHECK(window);
  // Should only specify display id when entering fullscreen.
  DCHECK(target_display_id == display::kInvalidDisplayId || fullscreen);

  ui::mojom::WindowShowState current_show_state =
      window->GetProperty(aura::client::kShowStateKey);
  const bool is_fullscreen =
      current_show_state == ui::mojom::WindowShowState::kFullscreen;
  if (fullscreen == is_fullscreen &&
      target_display_id == display::kInvalidDisplayId) {
    return;
  }
  if (fullscreen) {
    // We only want the current display id if the window is rooted in a display.
    // Need to check for root window, otherwise GetDisplayNearestWindow() would
    // return the primary display by default.
    int64_t current_display_id =
        window->GetRootWindow()
            ? display::Screen::GetScreen()->GetDisplayNearestWindow(window).id()
            : display::kInvalidDisplayId;
    if (is_fullscreen && target_display_id == current_display_id) {
      // Already fullscreened on the target display.
      return;
    }

    // Save the current show state as its restore show state so that we can
    // correctly restore it after exiting the fullscreen mode.
    // Note `aura::client::kRestoreShowStateKey` can be overwritten later by the
    // window state restore history stack on Chrome OS, see the function
    // WindowState::UpdateWindowStateRestoreHistoryStack(). But We still set the
    // `aura::client::kRestoreShowStateKey` here since this function is also
    // used on other non-ChromeOS platforms.
    if (current_show_state != ui::mojom::WindowShowState::kMinimized) {
      window->SetProperty(aura::client::kRestoreShowStateKey,
                          current_show_state);
    }
    // Set fullscreen display id first, so it's available when the show state
    // property change is processed.
    window->SetProperty(aura::client::kFullscreenTargetDisplayIdKey,
                        target_display_id);
    window->SetProperty(aura::client::kShowStateKey,
                        ui::mojom::WindowShowState::kFullscreen);
    window->ClearProperty(aura::client::kFullscreenTargetDisplayIdKey);
  } else {
    Restore(window);
  }
}

bool WindowStateIs(const aura::Window* window,
                   ui::mojom::WindowShowState state) {
  return window->GetProperty(aura::client::kShowStateKey) == state;
}

ui::mojom::WindowShowState GetWindowState(const aura::Window* window) {
  return window->GetProperty(aura::client::kShowStateKey);
}

void SetWindowState(aura::Window* window, ui::mojom::WindowShowState state) {
  window->SetProperty(aura::client::kShowStateKey, state);
}

void Restore(aura::Window* window) {
  window->SetProperty(aura::client::kIsRestoringKey, true);
  window->SetProperty(aura::client::kShowStateKey,
                      window->GetProperty(aura::client::kRestoreShowStateKey));
  window->ClearProperty(aura::client::kIsRestoringKey);
}

void Unminimize(aura::Window* window) {
  DCHECK_EQ(window->GetProperty(aura::client::kShowStateKey),
            ui::mojom::WindowShowState::kMinimized);
  Restore(window);
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  ActivationClient* client = GetActivationClient(window->GetRootWindow());
  return client ? client->GetActivatableWindow(window) : nullptr;
}

aura::Window* GetToplevelWindow(aura::Window* window) {
  return const_cast<aura::Window*>(
      GetToplevelWindow(const_cast<const aura::Window*>(window)));
}

const aura::Window* GetToplevelWindow(const aura::Window* window) {
  const ActivationClient* client = GetActivationClient(window->GetRootWindow());
  return client ? client->GetToplevelWindow(window) : nullptr;
}

std::unique_ptr<ui::LayerTreeOwner> RecreateLayers(ui::LayerOwner* root) {
  DCHECK(root->OwnsLayer());
  return RecreateLayersWithClosure(
      root, base::BindRepeating(
                [](ui::LayerOwner* owner) { return owner->RecreateLayer(); }));
}

std::unique_ptr<ui::LayerTreeOwner> RecreateLayersWithClosure(
    ui::LayerOwner* root,
    const MapLayerFunc& map_func) {
  DCHECK(root->OwnsLayer());
  auto layer = map_func.Run(root);
  if (!layer)
    return nullptr;
  auto old_layer = std::make_unique<ui::LayerTreeOwner>(std::move(layer));
  CloneChildren(root->layer(), old_layer->root(), map_func);
  return old_layer;
}

std::unique_ptr<ui::LayerTreeOwner> MirrorLayers(
    ui::LayerOwner* root, bool sync_bounds) {
  auto mirror = std::make_unique<ui::LayerTreeOwner>(root->layer()->Mirror());
  MirrorChildren(root->layer(), mirror->root(), sync_bounds);
  return mirror;
}

aura::Window* GetTransientParent(aura::Window* window) {
  return const_cast<aura::Window*>(GetTransientParent(
                                 const_cast<const aura::Window*>(window)));
}

const aura::Window* GetTransientParent(const aura::Window* window) {
  const TransientWindowManager* manager =
      TransientWindowManager::GetIfExists(window);
  return manager ? manager->transient_parent() : nullptr;
}

const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
GetTransientChildren(const aura::Window* window) {
  const TransientWindowManager* manager =
      TransientWindowManager::GetIfExists(window);
  if (manager)
    return manager->transient_children();

  static std::vector<raw_ptr<aura::Window, VectorExperimental>>* shared =
      new std::vector<raw_ptr<aura::Window, VectorExperimental>>;
  return *shared;
}

aura::Window* GetTransientRoot(aura::Window* window) {
  while (window && GetTransientParent(window))
    window = GetTransientParent(window);
  return window;
}

void AddTransientChild(aura::Window* parent, aura::Window* child) {
  TransientWindowManager::GetOrCreate(parent)->AddTransientChild(child);
}

void RemoveTransientChild(aura::Window* parent, aura::Window* child) {
  TransientWindowManager::GetOrCreate(parent)->RemoveTransientChild(child);
}

bool HasTransientAncestor(const aura::Window* window,
                          const aura::Window* ancestor) {
  const aura::Window* transient_parent = GetTransientParent(window);
  if (transient_parent == ancestor)
    return true;
  return transient_parent ?
      HasTransientAncestor(transient_parent, ancestor) : false;
}

}  // namespace wm
