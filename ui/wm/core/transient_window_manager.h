// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_TRANSIENT_WINDOW_MANAGER_H_
#define UI_WM_CORE_TRANSIENT_WINDOW_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/wm_core_export.h"

namespace wm {

class TransientWindowObserver;

// TransientWindowManager manages the set of transient children for a window
// along with the transient parent. Transient children get the following
// behavior:
// . The transient parent destroys any transient children when it is
//   destroyed. This means a transient child is destroyed if either its parent
//   or transient parent is destroyed.
// . If a transient child and its transient parent share the same parent, then
//   transient children are always ordered above the transient parent.
// . If a transient parent is hidden, it hides all transient children.
//   For show operation, please refer to |set_parent_controls_visibility(bool)|.
// Transient windows are typically used for popups and menus.
class WM_CORE_EXPORT TransientWindowManager : public aura::WindowObserver {
 public:
  using Windows = std::vector<aura::Window*>;

  ~TransientWindowManager() override;

  // Returns the TransientWindowManager for |window|, creating if necessary.
  // This never returns NULL.
  static TransientWindowManager* GetOrCreate(aura::Window* window);

  // Returns the TransientWindowManager for |window| only if it already exists.
  // WARNING: this may return NULL.
  static const TransientWindowManager* GetIfExists(const aura::Window* window);

  void AddObserver(TransientWindowObserver* observer);
  void RemoveObserver(TransientWindowObserver* observer);

  // Adds or removes a transient child.
  void AddTransientChild(aura::Window* child);
  void RemoveTransientChild(aura::Window* child);

  // Setting true lets the transient parent show this transient
  // child when the parent is shown. If this was shown when the
  // transient parent is hidden, it remains hidden and gets shown
  // when the transient parent is shown. This is false by default.
  void set_parent_controls_visibility(bool parent_controls_visibility) {
    parent_controls_visibility_ = parent_controls_visibility;
  }

  const Windows& transient_children() const { return transient_children_; }

  aura::Window* transient_parent() { return transient_parent_; }
  const aura::Window* transient_parent() const { return transient_parent_; }

  // Returns true if in the process of stacking |window_| on top of |target|.
  // That is, when the stacking order of a window changes
  // (OnWindowStackingChanged()) the transients may get restacked as well. This
  // function can be used to detect if TransientWindowManager is in the process
  // of stacking a transient as the result of window stacking changing.
  bool IsStackingTransient(const aura::Window* target) const;

 private:
  explicit TransientWindowManager(aura::Window* window);

  // Stacks transient descendants of this window that are its siblings just
  // above it.
  void RestackTransientDescendants();

  // Update the window's visibility following the transient parent's
  // visibility. See |set_parent_controls_visibility(bool)| for more details.
  void UpdateTransientChildVisibility(bool visible);

  // WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  aura::Window* window_;
  aura::Window* transient_parent_;
  Windows transient_children_;

  // If non-null we're actively restacking transient as the result of a
  // transient ancestor changing.
  aura::Window* stacking_target_;

  bool parent_controls_visibility_;
  bool show_on_parent_visible_;
  bool ignore_visibility_changed_event_;

  // Set to true to pause updating the stacking order of transient descandant
  // windows while we reparent those transient children which used to be on the
  // the same old parent as that of |window| to its new parent.
  // This avoid recursively restacking multiple times when we have to reparent
  // multiple transient children.
  bool pause_transient_descendants_restacking_ = false;

  base::ObserverList<TransientWindowObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowManager);
};

}  // namespace wm

#endif  // UI_WM_CORE_TRANSIENT_WINDOW_MANAGER_H_
