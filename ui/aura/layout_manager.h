// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_LAYOUT_MANAGER_H_
#define UI_AURA_LAYOUT_MANAGER_H_

#include "ui/aura/aura_export.h"

namespace gfx {
class Rect;
}

namespace aura {
class Window;

// An interface implemented by an object that places child windows.
class AURA_EXPORT LayoutManager {
 public:
  LayoutManager();
  virtual ~LayoutManager();

  // Invoked when the window is resized.
  virtual void OnWindowResized() = 0;

  // Invoked when the window |child| has been added.
  virtual void OnWindowAddedToLayout(Window* child) = 0;

  // Invoked prior to removing |window|.
  virtual void OnWillRemoveWindowFromLayout(Window* child) = 0;

  // Invoked after removing |window|.
  virtual void OnWindowRemovedFromLayout(Window* child) = 0;

  // Invoked when the |SetVisible()| is invoked on the window |child|.
  // |visible| is the value supplied to |SetVisible()|. If |visible| is true,
  // window->IsVisible() may still return false. See description in
  // Window::IsVisible() for details.
  virtual void OnChildWindowVisibilityChanged(Window* child, bool visible) = 0;

  // Invoked when |Window::SetBounds| is called on |child|.
  // Implementation must call |SetChildBoundsDirect| to change the
  // |child|'s bounds. LayoutManager may modify |requested_bounds|
  // before applying, or ignore the request.
  virtual void SetChildBounds(Window* child,
                              const gfx::Rect& requested_bounds) = 0;

 protected:
  // Sets the child's bounds forcibly. LayoutManager is responsible
  // for checking the state and make sure the bounds are correctly
  // adjusted.
  void SetChildBoundsDirect(aura::Window* child, const gfx::Rect& bounds);
};

}  // namespace aura

#endif  // UI_AURA_LAYOUT_MANAGER_H_
