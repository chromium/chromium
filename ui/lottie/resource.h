// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LOTTIE_RESOURCE_H_
#define UI_LOTTIE_RESOURCE_H_

#include "base/component_export.h"

namespace base {
class RefCountedString;
}

namespace gfx {
class ImageSkiaRep;
}

namespace lottie {

// Used for loading a Lottie asset intended as a still image (not animated).
COMPONENT_EXPORT(UI_LOTTIE)
gfx::ImageSkiaRep ParseLottieAsStillImage(
    const base::RefCountedString& bytes_string);

}  // namespace lottie

#endif  // UI_LOTTIE_RESOURCE_H_
