// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_WINDOW_SHAPE_H_
#define UI_VIEWS_WINDOW_WINDOW_SHAPE_H_

#include "ui/views/views_export.h"

class SkPath;

namespace gfx {
class Size;
}

namespace views {

// Sets the window mask to a style that most likely matches
// ui/resources/window_*
VIEWS_EXPORT void GetDefaultWindowMask(const gfx::Size& size,
                                       SkPath* window_mask);

}  // namespace views

#endif  // UI_VIEWS_WINDOW_WINDOW_SHAPE_H_
