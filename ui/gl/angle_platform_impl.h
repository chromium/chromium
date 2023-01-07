// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_ANGLE_PLATFORM_IMPL_H_
#define UI_GL_ANGLE_PLATFORM_IMPL_H_

// Implements the ANGLE platform interface, for functionality like
// histograms and trace profiling.

#include "ui/gl/gl_export.h"

typedef void* EGLDisplay;

namespace angle {

GL_EXPORT bool InitializePlatform(EGLDisplay display);
GL_EXPORT void ResetPlatform(EGLDisplay display);

}  // namespace angle

#endif  // UI_GL_ANGLE_PLATFORM_IMPL_H_
