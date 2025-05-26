// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include <EGL/egl.h>

extern "C" {
// The ANGLE internal eglGetProcAddress
EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY
EGL_GetProcAddress(const char* procname);
}

namespace gl {
namespace init {

bool InitializeStaticANGLEEGL() {
  SetGLGetProcAddressProc(&EGL_GetProcAddress);
  return true;
}

}  // namespace init
}  // namespace gl
