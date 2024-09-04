// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_CREATE_GR_GL_INTERFACE_H_
#define UI_GL_INIT_CREATE_GR_GL_INTERFACE_H_

#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"
#include "ui/gl/init/gl_init_export.h"

namespace gl {
struct GLVersionInfo;
class ProgressReporter;
}

namespace gl {
namespace init {

// Creates a GrGLInterface by taking function pointers from the current
// GL bindings.
GL_INIT_EXPORT sk_sp<GrGLInterface> CreateGrGLInterface(
    const gl::GLVersionInfo& version_info,
    gl::ProgressReporter* progress_reporter = nullptr);

}  // namespace init
}  // namespace gl

#endif  // UI_GL_INIT_CREATE_GR_GL_INTERFACE_H_
