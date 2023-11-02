// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_

#include <stddef.h>

#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center_export.h"

namespace message_center {

// Given the size of an image, returns the size of the properly scaled-up image
// which fits into |container_size|.
MESSAGE_CENTER_EXPORT gfx::Size GetImageSizeForContainerSize(
    const gfx::Size& container_size,
    const gfx::Size& image_size);

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
