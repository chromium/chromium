// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BUTTON_DRAG_UTILS_H_
#define UI_VIEWS_BUTTON_DRAG_UTILS_H_

#include "base/strings/string16.h"
#include "ui/views/views_export.h"

class GURL;

namespace gfx {
class ImageSkia;
class Point;
}

namespace ui {
class OSExchangeData;
}

namespace views {
class Widget;
}

namespace button_drag_utils {

// Sets url and title on data as well as setting a suitable image for dragging.
// The image looks like that of the bookmark buttons. |press_pt| is optional
// offset; otherwise, it centers the drag image.
VIEWS_EXPORT void SetURLAndDragImage(const GURL& url,
                                     const base::string16& title,
                                     const gfx::ImageSkia& icon,
                                     const gfx::Point* press_pt,
                                     const views::Widget& widget,
                                     ui::OSExchangeData* data);

// As above, but only sets the image.
VIEWS_EXPORT void SetDragImage(const GURL& url,
                               const base::string16& title,
                               const gfx::ImageSkia& icon,
                               const gfx::Point* press_pt,
                               const views::Widget& widget,
                               ui::OSExchangeData* data);

}  // namespace button_drag_utils

#endif  // UI_VIEWS_BUTTON_DRAG_UTILS_H_
