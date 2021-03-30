// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"

#if defined(USE_OZONE)
#include "ui/gl/init/gl_display_egl_util_ozone.h"
#include "ui/gl/init/ozone_util.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(USE_X11)
#include "ui/base/ui_base_features.h"
#include "ui/gl/init/gl_initializer_linux_x11.h"
#endif

namespace gl {
namespace init {

bool InitializeGLOneOffPlatform() {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    return gl::init::InitializeGLOneOffPlatformX11();
#endif

#if defined(USE_OZONE)
  if (HasGLOzone()) {
    gl::GLDisplayEglUtil::SetInstance(gl::GLDisplayEglUtilOzone::GetInstance());
    return GetGLOzone()->InitializeGLOneOffPlatform();
  }

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return true;
    default:
      NOTREACHED();
  }
#endif
  return false;
}

bool InitializeStaticGLBindings(GLImplementationParts implementation) {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    return gl::init::InitializeStaticGLBindingsX11(implementation);
#endif

#if defined(USE_OZONE)
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
      NOTREACHED();
  }
#endif

  return false;
}

void ShutdownGLPlatform() {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    return gl::init::ShutdownGLPlatformX11();
#endif

#if defined(USE_OZONE)
  if (HasGLOzone()) {
    GetGLOzone()->ShutdownGL();
    return;
  }

  ClearBindingsGL();
#endif
}

}  // namespace init
}  // namespace gl
