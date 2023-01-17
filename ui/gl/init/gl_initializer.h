// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_INITIALIZER_H_
#define UI_GL_INIT_GL_INITIALIZER_H_

#include "ui/gl/buildflags.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"

namespace gl {
namespace init {

// Performs platform dependent one-off GL initialization, calling into the
// appropriate GLSurface code to initialize it. To perform one-off GL
// initialization you should use InitializeGLOneOff() or
// InitializeStaticGLBindingsOneOff() +
// InitializeGLNoExtensionsOneOff(). For tests possibly
// InitializeStaticGLBindingsImplementation() +
// InitializeGLOneOffPlatformImplementation() instead.
// |gpu_preference| specifies which GPU to use on a multi-GPU system.
// If its value is kDefault, use the default GPU of the system.
GLDisplay* InitializeGLOneOffPlatform(gl::GpuPreference gpu_preference);

// Initializes a particular GL implementation.
bool InitializeStaticGLBindings(GLImplementationParts implementation);

#if BUILDFLAG(USE_STATIC_ANGLE)
bool InitializeStaticANGLEEGL();
#endif  // BUILDFLAG(USE_STATIC_ANGLE)

// Clears GL bindings for all implementations supported by platform.
// Calling this function a second time on the same |display| is a no-op.
void ShutdownGLPlatform(GLDisplay* display);

}  // namespace init
}  // namespace gl

#endif  // UI_GL_INIT_GL_INITIALIZER_H_
