// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_VISUAL_PICKER_GLX_H_
#define UI_GFX_X_VISUAL_PICKER_GLX_H_

#include "ui/gfx/x/xproto.h"

namespace x11 {

class Connection;

// Picks the best X11 visuals to use for GL.  This class is adapted from GTK's
// pick_better_visual_for_gl.  Tries to find visuals that
// 1. Support GL
// 2. Support double buffer
// 3. Have an alpha channel only if we want one
// `system_visual` and `rgba_visual` are output parameters populated with the
// visuals, or are left unchanged if GLX isn't available.
void PickBestVisuals(Connection* connection,
                     VisualId& system_visual,
                     VisualId& rgba_visual);

}  // namespace x11

#endif  // UI_GFX_X_VISUAL_PICKER_GLX_H_
