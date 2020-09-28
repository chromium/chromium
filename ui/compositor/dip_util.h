// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_DIP_UTIL_H_
#define UI_COMPOSITOR_DIP_UTIL_H_

#include "ui/compositor/compositor_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class Layer;

COMPOSITOR_EXPORT gfx::Rect ConvertRectToPixel(
    const Layer* layer,
    const gfx::Rect& rect_in_dip);
}  // namespace ui

#endif  // UI_COMPOSITOR_DIP_UTIL_H_
