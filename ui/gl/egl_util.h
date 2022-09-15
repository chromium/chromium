// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_EGL_UTIL_H_
#define UI_GL_EGL_UTIL_H_

#include <stdint.h>

#include "ui/gl/gl_export.h"

namespace ui {

GL_EXPORT const char* GetEGLErrorString(uint32_t error);

// Returns the last EGL error as a string.
GL_EXPORT const char* GetLastEGLErrorString();

}  // namespace ui

#endif  // UI_GL_EGL_UTIL_H_
