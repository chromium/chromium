// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BLIT_H_
#define UI_GFX_BLIT_H_

#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"

class SkCanvas;

namespace gfx {

class Rect;
class Vector2d;

// Scrolls the given subset of the given canvas by the given offset.
// The canvas should not have a clip or a transform applied, since platforms
// may implement those operations differently.
GFX_EXPORT void ScrollCanvas(SkCanvas* canvas,
                             const Rect& clip,
                             const Vector2d& offset);

}  // namespace gfx

#endif  // UI_GFX_BLIT_H_
