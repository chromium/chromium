// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WINDOW_REORDERER_H_
#define UI_VIEWS_WIDGET_WINDOW_REORDERER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view_observer.h"

namespace aura {
class Window;
}

namespace views {
class View;

// Class which reorders the widget's child windows which have an associated view
// in the widget's view tree according the z-order of the views in the view
// tree. Windows not associated to a view are stacked above windows with an
// associated view. The child windows' layers are additionally reordered
// according to the z-order of the associated views relative to views with
// layers.
class WindowReorderer : public aura::WindowObserver, public ViewObserver {
 public:
  WindowReorderer(aura::Window* window, View* root_view);

  WindowReorderer(const WindowReorderer&) = delete;
  WindowReorderer& operator=(const WindowReorderer&) = delete;

  ~WindowReorderer() override;

  // Explicitly reorder the children of |window_| (and their layers). This
  // method should be called when the position of a view with an associated
  // window changes in the view hierarchy. This method assumes that the
  // child layers of |window_| which are owned by views are already in the
  // correct z-order relative to each other and does no reordering if there
  // are no views with an associated window.
  void ReorderChildWindows();

 private:
  // aura::WindowObserver overrides:
  void OnWindowAdded(aura::Window* new_window) override;
  void OnWillRemoveWindow(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

  // The observation of the window of native widget that owns `this`.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      parent_window_observation_{this};

  // The observation of the root view of the native widget that owns `this`.
  base::ScopedObservation<View, ViewObserver> view_observation_{this};

  // Reorders windows as a result of the kHostViewKey being set on a child of
  // |parent_window_|.
  class AssociationObserver;
  std::unique_ptr<AssociationObserver> association_observer_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WINDOW_REORDERER_H_
