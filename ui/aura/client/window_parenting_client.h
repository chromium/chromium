// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_WINDOW_PARENTING_CLIENT_H_
#define UI_AURA_CLIENT_WINDOW_PARENTING_CLIENT_H_

#include "ui/aura/aura_export.h"

namespace gfx {
class Rect;
}

namespace aura {
class Window;
namespace client {

// Implementations of this object are used to help locate a default parent for
// NULL-parented Windows.
class AURA_EXPORT WindowParentingClient {
 public:
  virtual ~WindowParentingClient() {}

  // Called by the Window when it looks for a default parent. Returns the
  // window that |window| should be added to instead. NOTE: this may have
  // side effects. It should only be used when |window| is going to be
  // immediately added.
  virtual Window* GetDefaultParent(Window* window, const gfx::Rect& bounds) = 0;
};

// Set/Get a window tree client for the RootWindow containing |window|. |window|
// must not be NULL.
AURA_EXPORT void SetWindowParentingClient(
    Window* window,
    WindowParentingClient* window_tree_client);

AURA_EXPORT WindowParentingClient* GetWindowParentingClient(Window* window);

// Adds |window| to an appropriate parent by consulting an implementation of
// WindowParentingClient attached at the root Window containing |context|. The
// final
// location may be a window hierarchy other than the one supplied via
// |context|, which must not be NULL. |screen_bounds| may be empty.
AURA_EXPORT void ParentWindowWithContext(Window* window,
                                         Window* context,
                                         const gfx::Rect& screen_bounds);

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_WINDOW_PARENTING_CLIENT_H_
