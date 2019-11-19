// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_WINDOW_RESIZE_UTILS_H_
#define UI_VIEWS_WINDOW_WINDOW_RESIZE_UTILS_H_

#include "base/macros.h"
#include "ui/views/views_export.h"

namespace gfx {
class Size;
class Rect;
}  // namespace gfx

namespace views {

enum class HitTest {
  kBottom,
  kBottomLeft,
  kBottomRight,
  kLeft,
  kRight,
  kTop,
  kTopLeft,
  kTopRight
};

class VIEWS_EXPORT WindowResizeUtils {
 public:
  // Force the min and max window sizes to adhere to the aspect ratio.
  // |aspect_ratio| must be valid and is found using width / height.
  static void SizeMinMaxToAspectRatio(float aspect_ratio,
                                      gfx::Size* min_window_size,
                                      gfx::Size* max_window_size);

  // Updates |rect| to adhere to the |aspect_ratio| of the window, if it has
  // been set. |param| refers to the edge of the window being sized.
  // |min_window_size| and |max_window_size| are expected to adhere to the
  // given aspect ratio.
  // |aspect_ratio| must be valid and is found using width / height.
  // TODO(apacible): |max_window_size| is expected to be non-empty. Handle
  // unconstrained max sizes and sizing when windows are maximized.
  static void SizeRectToAspectRatio(HitTest param,
                                    float aspect_ratio,
                                    const gfx::Size& min_window_size,
                                    const gfx::Size& max_window_size,
                                    gfx::Rect* rect);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WindowResizeUtils);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_WINDOW_RESIZE_UTILS_H_
