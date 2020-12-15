// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_GL_EGL_UTILITY_H_
#define UI_BASE_X_X11_GL_EGL_UTILITY_H_

#include <vector>

#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

// Returns display attributes for the given |platform_type|.
void GetPlatformExtraDisplayAttribs(EGLenum platform_type,
                                    std::vector<EGLAttrib>* attributes);

// Sets custom alpha and buffer size.
void ChoosePlatformCustomAlphaAndBufferSize(EGLint* alpha_size,
                                            EGLint* buffer_size);

// Returns whether transparent background is suppored.
bool IsTransparentBackgroundSupported();

// Wraps XVisualManager::UpdateVisualsOnGpuInfoChanged(), passes parameters to
// it directly. Returns whether provided visuals are valid.
bool UpdateVisualsOnGpuInfoChanged(bool software_rendering,
                                   x11::VisualId default_visual_id,
                                   x11::VisualId transparent_visual_id);

}  // namespace ui

#endif  // UI_BASE_X_X11_GL_EGL_UTILITY_H_
