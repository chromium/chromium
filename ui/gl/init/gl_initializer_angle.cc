// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_initializer.h"

#include <EGL/egl.h>

namespace gl {
namespace init {

bool InitializeStaticANGLEEGL() {
#pragma push_macro("eglGetProcAddress")
#undef eglGetProcAddress
  SetGLGetProcAddressProc(&eglGetProcAddress);
#pragma pop_macro("eglGetProcAddress")
  return true;
}

}  // namespace init
}  // namespace gl
