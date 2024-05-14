// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_display_initializer.h"

namespace gl {
namespace init {

GLDisplay* InitializeGLOneOffPlatform(gl::GpuPreference gpu_preference) {
  GLDisplayEGL* display = GetDisplayEGL(gpu_preference);
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      if (!InitializeDisplay(display,
                             EGLDisplayPlatform(EGL_DEFAULT_DISPLAY))) {
        LOG(ERROR) << "GLDisplayEGL::Initialize failed.";
        return nullptr;
      }
      break;
    default:
      break;
  }
  return display;
}

bool InitializeStaticGLBindings(GLImplementationParts implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  switch (implementation.gl) {
    case kGLImplementationEGLANGLE:
      SetGLImplementationParts(implementation);
      if (!InitializeStaticANGLEEGL()) {
        return false;
      }
      InitializeStaticGLBindingsGL();
      InitializeStaticGLBindingsEGL();
      return true;
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      SetGLImplementationParts(implementation);
      InitializeStaticGLBindingsGL();
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return false;
}

void ShutdownGLPlatform(GLDisplay* display) {
  if (display) {
    display->Shutdown();
  }
  ClearBindingsEGL();
  ClearBindingsGL();
}

}  // namespace init
}  // namespace gl
