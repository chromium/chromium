// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/transient_window_manager.h"

#include <algorithm>
#include <functional>

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/class_property.h"
#include "ui/wm/core/transient_window_controller.h"
#include "ui/wm/core/transient_window_observer.h"
#include "ui/wm/core/transient_window_stacking_client.h"
#include "ui/wm/core/window_util.h"

using aura::Window;

DEFINE_UI_CLASS_PROPERTY_TYPE(::wm::TransientWindowManager*)

namespace wm {
namespace {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(TransientWindowManager, kPropertyKey, NULL)

}  // namespace

TransientWindowManager::~TransientWindowManager() {
}

// static
TransientWindowManager* TransientWindowManager::GetOrCreate(Window* window) {
  TransientWindowManager* manager = window->GetProperty(kPropertyKey);
  if (!manager) {
    manager = new TransientWindowManager(window);
    window->SetProperty(kPropertyKey, manager);
  }
  return manager;
}

// static
const TransientWindowManager* TransientWindowManager::GetIfExists(
    const Window* window) {
  return window->GetProperty(kPropertyKey);
}

void TransientWindowManager::AddObserver(TransientWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void TransientWindowManager::RemoveObserver(TransientWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TransientWindowManager::AddTransientChild(Window* child) {
  // TransientWindowStackingClient does the stacking of transient windows. If it
  // isn't installed stacking is going to be wrong.
  DCHECK(TransientWindowStackingClient::instance_);

  TransientWindowManager* child_manager = GetOrCreate(child);
  if (child_manager->transient_parent_)
    GetOrCreate(child_manager->transient_parent_)->RemoveTransientChild(child);
  DCHECK(!base::Contains(transient_children_, child));
  transient_children_.push_back(child);
  child_manager->transient_parent_ = window_;

  for (aura::client::TransientWindowClientObserver& observer :
       TransientWindowController::Get()->observers_) {
    observer.OnTransientChildWindowAdded(window_, child);
  }

  // Restack |child| properly above its transient parent, if they share the same
  // parent.
  if (child->parent() == window_->parent())
    RestackTransientDescendants();

  for (auto& observer : observers_)
    observer.OnTransientChildAdded(window_, child);
}

void TransientWindowManager::RemoveTransientChild(Window* child) {
  auto i =
      std::find(transient_children_.begin(), transient_children_.end(), child);
  DCHECK(i != transient_children_.end());
  transient_children_.erase(i);
  TransientWindowManager* child_manager = GetOrCreate(child);
  DCHECK_EQ(window_, child_manager->transient_parent_);
  child_manager->transient_parent_ = nullptr;

  for (aura::client::TransientWindowClientObserver& observer :
       TransientWindowController::Get()->observers_) {
    observer.OnTransientChildWindowRemoved(window_, child);
  }

  // If |child| and its former transient parent share the same parent, |child|
  // should be restacked properly so it is not among transient children of its
  // former parent, anymore.
  if (window_->parent() == child->parent())
    RestackTransientDescendants();

  for (auto& observer : observers_)
    observer.OnTransientChildRemoved(window_, child);
}

bool TransientWindowManager::IsStackingTransient(
    const aura::Window* target) const {
  return stacking_target_ == target;
}

TransientWindowManager::TransientWindowManager(Window* window)
    : window_(window),
      transient_parent_(NULL),
      stacking_target_(NULL),
      parent_controls_visibility_(false),
      show_on_parent_visible_(false),
      ignore_visibility_changed_event_(false) {
  window_->AddObserver(this);
}

void TransientWindowManager::RestackTransientDescendants() {
  if (pause_transient_descendants_restacking_)
    return;

  Window* parent = window_->parent();
  if (!parent)
    return;

  // Stack any transient children that share the same parent to be in front of
  // |window_|. The existing stacking order is preserved by iterating backwards
  // and always stacking on top.
  Window::Windows children(parent->children());
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    if ((*it) != window_ && HasTransientAncestor(*it, window_)) {
      TransientWindowManager* descendant_manager = GetOrCreate(*it);
      base::AutoReset<Window*> resetter(
          &descendant_manager->stacking_target_,
          window_);
      parent->StackChildAbove((*it), window_);
    }
  }
}

void TransientWindowManager::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.target != window_)
    return;

  // [1] Move the direct transient children which used to be on the old parent
  // of |window_| to the new parent.
  bool should_restack = false;
  aura::Window* new_parent = params.new_parent;
  aura::Window* old_parent = params.old_parent;
  if (new_parent) {
    // Reparenting multiple sibling transient children will call back onto us
    // (the transient parent) in [2] below, to restack all our descendants. We
    // should pause restacking until we're done with all the reparenting.
    base::AutoReset<bool> reset(&pause_transient_descendants_restacking_, true);
    for (auto* transient_child : transient_children_) {
      if (transient_child->parent() == old_parent) {
        new_parent->AddChild(transient_child);
        should_restack = true;
      }
    }
  }
  if (should_restack)
    RestackTransientDescendants();

  // [2] Stack |window_| properly if it is a transient child of a sibling.
  if (transient_parent_ && transient_parent_->parent() == new_parent) {
    TransientWindowManager* transient_parent_manager =
        GetOrCreate(transient_parent_);
    transient_parent_manager->RestackTransientDescendants();
  }
}

void TransientWindowManager::UpdateTransientChildVisibility(
    bool parent_visible) {
  base::AutoReset<bool> reset(&ignore_visibility_changed_event_, true);
  if (!parent_visible) {
    show_on_parent_visible_ = window_->TargetVisibility();
    window_->Hide();
  } else {
    if (show_on_parent_visible_ && parent_controls_visibility_)
      window_->Show();
    show_on_parent_visible_ = false;
  }
}

void TransientWindowManager::OnWindowVisibilityChanged(Window* window,
                                                       bool visible) {
  if (window_ != window)
    return;

  // If the window has transient children, updates the transient children's
  // visiblity as well.
  // WindowTracker is used because child window
  // could be deleted inside UpdateTransientChildVisibility call.
  aura::WindowTracker tracker(transient_children_);
  while (!tracker.windows().empty())
    GetOrCreate(tracker.Pop())->UpdateTransientChildVisibility(visible);

  // Remember the show request in |show_on_parent_visible_| and hide it again
  // if the following conditions are met
  // - |parent_controls_visibility| is set to true.
  // - the window is hidden while the transient parent is not visible.
  // - Show/Hide was NOT called from TransientWindowManager.
  if (ignore_visibility_changed_event_ ||
      !transient_parent_ || !parent_controls_visibility_) {
    return;
  }

  if (!transient_parent_->TargetVisibility() && visible) {
    base::AutoReset<bool> reset(&ignore_visibility_changed_event_, true);
    show_on_parent_visible_ = true;
    window_->Hide();
  } else if (!visible) {
    DCHECK(!show_on_parent_visible_);
  }
}

void TransientWindowManager::OnWindowStackingChanged(Window* window) {
  DCHECK_EQ(window_, window);
  // Do nothing if we initiated the stacking change.
  const TransientWindowManager* transient_manager = GetIfExists(window);
  if (transient_manager && transient_manager->stacking_target_) {
    auto window_i = std::find(window->parent()->children().begin(),
                              window->parent()->children().end(), window);
    DCHECK(window_i != window->parent()->children().end());
    if (window_i != window->parent()->children().begin() &&
        (*(window_i - 1) == transient_manager->stacking_target_))
      return;
  }

  RestackTransientDescendants();
}

void TransientWindowManager::OnWindowDestroying(Window* window) {
  // Removes ourselves from our transient parent (if it hasn't been done by the
  // RootWindow).
  if (transient_parent_) {
    TransientWindowManager::GetOrCreate(transient_parent_)
        ->RemoveTransientChild(window_);
  }

  // Destroy transient children, only after we've removed ourselves from our
  // parent, as destroying an active transient child may otherwise attempt to
  // refocus us.
  Windows transient_children(transient_children_);
  for (auto* child : transient_children)
    delete child;
  DCHECK(transient_children_.empty());
}

}  // namespace wm
