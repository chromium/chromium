// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUTTON_DRAG_UTILS_H_
#define UI_VIEWS_BUTTON_DRAG_UTILS_H_

#include <string>

#include "ui/views/views_export.h"

class GURL;

namespace gfx {
class ImageSkia;
class Point;
}  // namespace gfx

namespace ui {
class OSExchangeData;
}

namespace button_drag_utils {

// Sets url and title on data as well as setting a suitable image for dragging.
// The image looks like that of the bookmark buttons. |press_pt| is optional
// offset; otherwise, it centers the drag image.
VIEWS_EXPORT void SetURLAndDragImage(const GURL& url,
                                     const std::u16string& title,
                                     const gfx::ImageSkia& icon,
                                     const gfx::Point* press_pt,
                                     ui::OSExchangeData* data);

// As above, but only sets the image.
VIEWS_EXPORT void SetDragImage(const GURL& url,
                               const std::u16string& title,
                               const gfx::ImageSkia& icon,
                               const gfx::Point* press_pt,
                               ui::OSExchangeData* data);

}  // namespace button_drag_utils

#endif  // UI_VIEWS_BUTTON_DRAG_UTILS_H_
