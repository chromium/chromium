// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_util.h"

#include "base/bind.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
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
  typedef std::vector<ui::Layer*> Layers;
  // Make a copy of the children since RecreateLayer() mutates it.
  Layers children(to_clone->children());
  for (Layers::const_iterator i = children.begin(); i != children.end(); ++i) {
    ui::LayerOwner* owner = (*i)->owner();
    ui::Layer* old_layer = owner ? map_func.Run(owner).release() : NULL;
    if (old_layer) {
      parent->Add(old_layer);
      // RecreateLayer() moves the existing children to the new layer. Create a
      // copy of those.
      CloneChildren(owner->layer(), old_layer, map_func);
    }
  }
}

// Invokes Mirror() on all the children of |to_mirror|, adding the newly cloned
// children to |parent|.
//
// WARNING: It is assumed that |parent| is ultimately owned by a LayerTreeOwner.
void MirrorChildren(ui::Layer* to_mirror,
                    ui::Layer* parent,
                    bool sync_bounds) {
  for (auto* child : to_mirror->children()) {
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

void SetWindowFullscreen(aura::Window* window, bool fullscreen) {
  DCHECK(window);
  ui::WindowShowState current_show_state =
      window->GetProperty(aura::client::kShowStateKey);
  bool is_fullscreen = current_show_state == ui::SHOW_STATE_FULLSCREEN;
  if (fullscreen == is_fullscreen)
    return;
  if (fullscreen) {
    // Save the previous show state so that we can correctly restore it after
    // exiting the fullscreen mode.
    ui::WindowShowState pre_show_state = current_show_state;
    // If the previous show state is ui::SHOW_STATE_MINIMIZED, we will use
    // the show state before the window was minimized. But if the window was
    // fullscreen before it was minimized, we will keep the
    // PreMinimizedShowState unchanged.
    if (pre_show_state == ui::SHOW_STATE_MINIMIZED) {
      pre_show_state =
          window->GetProperty(aura::client::kPreMinimizedShowStateKey);
    }
    if (pre_show_state != ui::SHOW_STATE_FULLSCREEN) {
      window->SetProperty(aura::client::kPreFullscreenShowStateKey,
                          pre_show_state);
    }
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  } else {
    ui::WindowShowState pre_fullscreen_show_state =
        window->GetProperty(aura::client::kPreFullscreenShowStateKey);
    DCHECK_NE(pre_fullscreen_show_state, ui::SHOW_STATE_MINIMIZED);
    window->SetProperty(aura::client::kShowStateKey, pre_fullscreen_show_state);
    window->ClearProperty(aura::client::kPreFullscreenShowStateKey);
  }
}

bool WindowStateIs(const aura::Window* window, ui::WindowShowState state) {
  return window->GetProperty(aura::client::kShowStateKey) == state;
}

void SetWindowState(aura::Window* window, ui::WindowShowState state) {
  window->SetProperty(aura::client::kShowStateKey, state);
}

void Unminimize(aura::Window* window) {
  DCHECK_EQ(window->GetProperty(aura::client::kShowStateKey),
            ui::SHOW_STATE_MINIMIZED);
  window->SetProperty(
      aura::client::kShowStateKey,
      window->GetProperty(aura::client::kPreMinimizedShowStateKey));
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  ActivationClient* client = GetActivationClient(window->GetRootWindow());
  return client ? client->GetActivatableWindow(window) : NULL;
}

aura::Window* GetToplevelWindow(aura::Window* window) {
  return const_cast<aura::Window*>(
      GetToplevelWindow(const_cast<const aura::Window*>(window)));
}

const aura::Window* GetToplevelWindow(const aura::Window* window) {
  const ActivationClient* client = GetActivationClient(window->GetRootWindow());
  return client ? client->GetToplevelWindow(window) : NULL;
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

const std::vector<aura::Window*>& GetTransientChildren(
    const aura::Window* window) {
  const TransientWindowManager* manager =
      TransientWindowManager::GetIfExists(window);
  if (manager)
    return manager->transient_children();

  static std::vector<aura::Window*>* shared = new std::vector<aura::Window*>;
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
