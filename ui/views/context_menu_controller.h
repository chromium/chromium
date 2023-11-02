// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTEXT_MENU_CONTROLLER_H_
#define UI_VIEWS_CONTEXT_MENU_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Point;
}

namespace views {
class View;

// ContextMenuController is responsible for showing the context menu for a
// View. To use a ContextMenuController invoke set_context_menu_controller on a
// View. When the appropriate user gesture occurs ShowContextMenu is invoked
// on the ContextMenuController.
//
// Setting a ContextMenuController on a view makes the view process mouse
// events.
//
// It is up to subclasses that do their own mouse processing to invoke
// the appropriate ContextMenuController method, typically by invoking super's
// implementation for mouse processing.
class VIEWS_EXPORT ContextMenuController {
 public:
  ContextMenuController();

  ContextMenuController(const ContextMenuController&) = delete;
  ContextMenuController& operator=(const ContextMenuController&) = delete;

  // Invoked to show the context menu for |source|. |point| is in screen
  // coordinates. This method also prevents reentrant calls.
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type);

 protected:
  virtual ~ContextMenuController();

 private:
  // Subclasses should override this method.
  virtual void ShowContextMenuForViewImpl(View* source,
                                          const gfx::Point& point,
                                          ui::MenuSourceType source_type) = 0;

  // Used as a flag to prevent a re-entrancy in ShowContextMenuForView().
  // This is most relevant to Linux, where spawning the textfield context menu
  // spins a nested message loop that processes input events, which may attempt
  // to trigger another context menu.
  bool is_opening_ = false;

  base::WeakPtrFactory<ContextMenuController> weak_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_CONTEXT_MENU_CONTROLLER_H_
