// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_EGL_UTIL_H_
#define UI_OZONE_COMMON_EGL_UTIL_H_

#include <stdint.h>

#include "ui/gl/gl_implementation.h"

namespace ui {

bool LoadDefaultEGLGLES2Bindings(const gl::GLImplementationParts& impl);

void* /* EGLConfig */ ChooseEGLConfig(void* /* EGLConfig */ display,
                                      const int32_t* attributes);

}  // namespace ui

#endif  // UI_OZONE_COMMON_EGL_UTIL_H_
