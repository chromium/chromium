// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WINDOW_REORDERER_H_
#define UI_VIEWS_WIDGET_WINDOW_REORDERER_H_

#include "base/scoped_observation.h"
#include "ui/aura/window_observer.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace views {
class View;

// Class which reorders the widget's child windows according to the z-order
// of the views in the view tree. It uses a hybrid View/Layer tree traversal
// to find associations structurally without relying on window properties
// like kHostViewKey. It is used when the kNativeViewHostManagesLayers
// feature is enabled.
class VIEWS_EXPORT WindowReorderer : public aura::WindowObserver,
                                     public ViewObserver {
 public:
  WindowReorderer(aura::Window* parent_window, View* root_view);
  WindowReorderer(const WindowReorderer&) = delete;
  WindowReorderer& operator=(const WindowReorderer&) = delete;
  ~WindowReorderer() override;

  // Explicitly reorder the children of the parent window (and their layers).
  // This method should be called when the position of a view with an associated
  // window changes in the view hierarchy. This method assumes that the
  // child layers of the parent window which are owned by views are already in
  // the correct z-order relative to each other and does no reordering if there
  // are no views with an associated window.
  //
  // Unassociated windows (windows not hosting a NativeViewHost) preserve
  // their relative positions:
  // 1) Windows below the bottom-most window that participates in the views
  //    layer tree stay below all participating windows in the same order.
  // 2) Windows above the top-most participating window stay above all
  //    participating windows in the same order.
  // 3) Windows between the bottom-most and top-most participating windows
  //    stay in that middle range in the same order.
  void ReorderChildWindows();

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // ViewObserver:
  void OnViewIsDeleting(View* observed_view) override;

  // The observation of the window of native widget that owns `this`.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      parent_window_observation_{this};

  // The observation of the root view of the native widget that owns `this`.
  base::ScopedObservation<View, ViewObserver> view_observation_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WINDOW_REORDERER_H_
