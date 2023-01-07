// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/views_utilities_aura.h"

#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace views {

aura::Window* GetWindowParentIncludingTransient(aura::Window* window) {
  aura::Window* transient_parent = wm::GetTransientParent(window);
  if (transient_parent)
    return transient_parent;

  return window->parent();
}

}  // namespace views
