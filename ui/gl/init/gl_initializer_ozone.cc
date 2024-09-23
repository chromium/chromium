// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_display_egl_util_ozone.h"
#include "ui/gl/init/gl_display_initializer.h"
#include "ui/gl/init/ozone_util.h"
#include "ui/ozone/public/ozone_platform.h"

namespace gl {
namespace init {

GLDisplay* InitializeGLOneOffPlatform(gl::GpuPreference gpu_preference) {
  if (HasGLOzone()) {
    gl::GLDisplayEglUtil::SetInstance(gl::GLDisplayEglUtilOzone::GetInstance());
    bool supports_angle = false;
    std::vector<gl::DisplayType> init_displays;
    GetDisplayInitializationParams(&supports_angle, &init_displays);
    return GetGLOzone()->InitializeGLOneOffPlatform(
        supports_angle, init_displays, gpu_preference);
  }

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return GetDisplayEGL(gpu_preference);
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return nullptr;
}

bool InitializeStaticGLBindings(GLImplementationParts implementation) {
  // Prevent reinitialization with a different implementation. Once the gpu
  // unit tests have initialized with kGLImplementationMock, we don't want to
  // later switch to another GL implementation.
  DCHECK_EQ(kGLImplementationNone, GetGLImplementation());

  if (HasGLOzone(implementation)) {
    return GetGLOzone(implementation)
        ->InitializeStaticGLBindings(implementation);
  }

  switch (implementation.gl) {
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
  if (HasGLOzone()) {
    GetGLOzone()->ShutdownGL(display);
    return;
  }

  ClearBindingsGL();
}

}  // namespace init
}  // namespace gl
