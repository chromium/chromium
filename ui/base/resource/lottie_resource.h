// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_LOTTIE_RESOURCE_H_
#define UI_BASE_RESOURCE_LOTTIE_RESOURCE_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "build/build_config.h"

namespace gfx {
class ImageSkia;
}

namespace ui {
class ImageModel;

// Used for loading a Lottie asset intended as a still image (not animated).
gfx::ImageSkia ParseLottieAsStillImage(std::vector<uint8_t> data);

// Used for loading a Lottie asset intended as a still image (not animated),
// with support for using different colors in light mode, dark mode, and
// "elevated" dark mode (see |views::Widget::InitParams::background_elevation|).
ui::ImageModel ParseLottieAsThemedStillImage(std::vector<uint8_t> data);

}  // namespace ui

#endif  // UI_BASE_RESOURCE_LOTTIE_RESOURCE_H_
