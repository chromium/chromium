// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_INITIALIZER_LINUX_X11_H_
#define UI_GL_INIT_GL_INITIALIZER_LINUX_X11_H_

#include "ui/gl/gl_implementation.h"

namespace gl {

// Note that this is a temporary gl implementation for Linux/X11 GL. It is
// called through GLFactoryOzone, and will be removed as soon as Linux/Ozone is
// default.
//
// TODO(msisov): remove this once Ozone is default on Linux.
namespace init {

// Performs platform dependent one-off GL initialization, calling into the
// appropriate GLSurface code to initialize it. To perform one-off GL
// initialization you should use InitializeGLOneOff() or
// InitializeStaticGLBindingsOneOff() +
// InitializeGLNoExtensionsOneOff(). For tests possibly
// InitializeStaticGLBindingsImplementation() +
// InitializeGLOneOffPlatformImplementation() instead.
bool InitializeGLOneOffPlatformX11();

// Initializes a particular GL implementation.
bool InitializeStaticGLBindingsX11(GLImplementationParts implementation);

// Clears GL bindings for all implementations supported by platform.
void ShutdownGLPlatformX11();

}  // namespace init

}  // namespace gl

#endif  // UI_GL_INIT_GL_INITIALIZER_LINUX_X11_H_
