// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LOTTIE_RESOURCE_H_
#define UI_LOTTIE_RESOURCE_H_

#include <string>

#include "base/component_export.h"
#include "build/chromeos_buildflags.h"

namespace gfx {
class ImageSkia;
}

namespace ui {
class ImageModel;
}

namespace lottie {

// Used for loading a Lottie asset intended as a still image (not animated).
COMPONENT_EXPORT(UI_LOTTIE)
gfx::ImageSkia ParseLottieAsStillImage(const std::string& bytes_string);

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Used for loading a Lottie asset intended as a still image (not animated),
// with support for using different colors in light mode, dark mode, and
// "elevated" dark mode (see |views::Widget::InitParams::background_elevation|).
COMPONENT_EXPORT(UI_LOTTIE)
ui::ImageModel ParseLottieAsThemedStillImage(const std::string& bytes_string);
#endif

}  // namespace lottie

#endif  // UI_LOTTIE_RESOURCE_H_
