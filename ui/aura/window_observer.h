// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_OBSERVER_H_
#define UI_AURA_WINDOW_OBSERVER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/aura/aura_export.h"
#include "ui/compositor/property_change_reason.h"

namespace gfx {
class Rect;
class Transform;
}  // namespace gfx

namespace aura {

class Window;

class AURA_EXPORT WindowObserver {
 public:
  struct HierarchyChangeParams {
    enum HierarchyChangePhase {
      HIERARCHY_CHANGING,
      HIERARCHY_CHANGED
    };

    Window* target;     // The window that was added or removed.
    Window* new_parent;
    Window* old_parent;
    HierarchyChangePhase phase;
    Window* receiver;   // The window receiving the notification.
  };

  WindowObserver();

  // Called when a window is added or removed. Notifications are sent to the
  // following hierarchies in this order:
  // 1. |target|.
  // 2. |target|'s child hierarchy.
  // 3. |target|'s parent hierarchy in its |old_parent|
  //        (only for Changing notifications).
  // 3. |target|'s parent hierarchy in its |new_parent|.
  //        (only for Changed notifications).
  // This sequence is performed via the Changing and Changed notifications below
  // before and after the change is committed.
  virtual void OnWindowHierarchyChanging(const HierarchyChangeParams& params) {}
  virtual void OnWindowHierarchyChanged(const HierarchyChangeParams& params) {}

  // Invoked when |new_window| has been added as a child of this window.
  virtual void OnWindowAdded(Window* new_window) {}

  // Invoked prior to removing |window| as a child of this window.
  virtual void OnWillRemoveWindow(Window* window) {}

  // Invoked when this window's parent window changes.  |parent| may be NULL.
  virtual void OnWindowParentChanged(Window* window, Window* parent) {}

  // Invoked when SetProperty(), ClearProperty(), or
  // NativeWidgetAura::SetNativeWindowProperty() is called on the window.
  // |key| is either a WindowProperty<T>* (SetProperty, ClearProperty)
  // or a const char* (SetNativeWindowProperty). Either way, it can simply be
  // compared for equality with the property constant. |old| is the old property
  // value, which must be cast to the appropriate type before use.
  virtual void OnWindowPropertyChanged(Window* window,
                                       const void* key,
                                       intptr_t old) {}

  // Invoked when SetVisible() is invoked on a window. |visible| is the
  // value supplied to SetVisible(). If |visible| is true, window->IsVisible()
  // may still return false. See description in Window::IsVisible() for details.
  virtual void OnWindowVisibilityChanging(Window* window, bool visible) {}

  // When the visibility of a Window changes OnWindowVisibilityChanged() is
  // called for all observers attached to descendants of the Window as well
  // as all observers attached to ancestors of the Window. The Window supplied
  // to OnWindowVisibilityChanged() is the Window that Show()/Hide() was called
  // on.
  virtual void OnWindowVisibilityChanged(Window* window, bool visible) {}

  // Invoked when the bounds of the |window|'s layer change. |old_bounds| and
  // |new_bounds| are in parent coordinates. |reason| indicates whether the
  // bounds were set directly or by an animation. This will be called at every
  // step of a bounds animation. The client can determine whether the animation
  // is ending by calling window->layer()->GetAnimator()->IsAnimatingProperty(
  // ui::LayerAnimationElement::BOUNDS).
  virtual void OnWindowBoundsChanged(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds,
                                     ui::PropertyChangeReason reason) {}

  // Invoked before Window::SetTransform() sets the transform of a window.
  virtual void OnWindowTargetTransformChanging(
      Window* window,
      const gfx::Transform& new_transform) {}

  // Invoked when the transform of |window| is set (even if it didn't change).
  // |reason| indicates whether the transform was set directly or by an
  // animation. This won't necessarily be called at every step of an animation.
  // However, it will always be called before the first frame of the animation
  // is rendered and when the animation ends. The client can determine whether
  // the animation is ending by calling
  // window->layer()->GetAnimator()->IsAnimatingProperty(
  // ui::LayerAnimationElement::TRANSFORM).
  virtual void OnWindowTransformed(Window* window,
                                   ui::PropertyChangeReason reason) {}

  // Invoked when the opacity of the |window|'s layer is set (even if it didn't
  // change). |reason| indicates whether the opacity was set directly or by an
  // animation. This won't necessarily be called at every step of an animation.
  // However, it will always be called before the first frame of the animation
  // is rendered and when the animation ends. The client can determine whether
  // the animation is ending by calling
  // window->layer()->GetAnimator()->IsAnimatingProperty(
  // ui::LayerAnimationElement::OPACITY).
  virtual void OnWindowOpacitySet(Window* window,
                                  ui::PropertyChangeReason reason) {}

  // Invoked when the alpha shape of the |window|'s layer is set.
  virtual void OnWindowAlphaShapeSet(Window* window) {}

  // Invoked when |window|'s position among its siblings in the stacking order
  // has changed.
  virtual void OnWindowStackingChanged(Window* window) {}

  // Invoked when the Window is being destroyed (i.e. from the start of its
  // destructor). This is called before the window is removed from its parent.
  virtual void OnWindowDestroying(Window* window) {}

  // Invoked when the Window has been destroyed (i.e. at the end of
  // its destructor). This is called after the window is removed from
  // its parent.  Window automatically removes its WindowObservers
  // before calling this method, so the following code is no op.
  //
  // void MyWindowObserver::OnWindowDestroyed(aura::Window* window) {
  //    window->RemoveObserver(this);
  // }
  virtual void OnWindowDestroyed(Window* window) {}

  // Called when a Window has been added to a RootWindow.
  virtual void OnWindowAddedToRootWindow(Window* window) {}

  // Called when a Window is about to be removed from a root Window.
  // |new_root| contains the new root Window if it is being added to one
  // atomically.
  virtual void OnWindowRemovingFromRootWindow(Window* window,
                                              Window* new_root) {}

  // Called from SetBoundsInScreen() when a window is moving to a new display as
  // the result of changing bounds. |new_display_id| is the specified new
  // display id. This is called before the bounds are actually changed.
  virtual void OnWillMoveWindowToDisplay(Window* window,
                                         int64_t new_display_id) {}

  // Called from SetBoundsInScreen() when the task of the window moving to a new
  // display finished. Sometimes the window may stay in the old display, but
  // this will be called anyways.
  virtual void OnDidMoveWindowToDisplay(Window* window) {}

  // Called when the window title has changed.
  virtual void OnWindowTitleChanged(Window* window) {}

  // Called when the window's layer is recreated. The new layer may not carry
  // animations from the old layer and animation observers attached to the old
  // layer won't automatically be attached to the new layer. Clients that need
  // to know when window animations end should implement this method and call
  // window->layer()->GetAnimator()->
  // (is_animating|IsAnimatingProperty|IsAnimatingOnePropertyOf)() from it.
  virtual void OnWindowLayerRecreated(Window* window) {}

  // Called when the app embedded in |window| disconnects (is no longer
  // embedded).
  virtual void OnEmbeddedAppDisconnected(Window* window);

 protected:
  virtual ~WindowObserver();

 private:
  friend class Window;

  // Called when this is added as an observer on |window|.
  void OnObservingWindow(Window* window);

  // Called when this is removed from the observers on |window|.
  void OnUnobservingWindow(Window* window);

  // Tracks the number of windows being observed to track down
  // http://crbug.com/365364.
  int observing_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserver);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_OBSERVER_H_
