// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_DISPLAY_EGL_UTIL_H_
#define UI_GL_GL_DISPLAY_EGL_UTIL_H_

#include <vector>

#include "third_party/khronos/EGL/egl.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

// Utility singleton class that helps to set additional egl properties. This
// class should be implemented by each platform except Ozone. In case of Ozone,
// there is a common implementation that forwards calls to a public interface of
// a platform.
// The reason why it is defined here in ui/gl is that ui/gl cannot depend on
// ozone and we have to provide an interface here. ui/gl/init will provide an
// implementation for this utility class upon initialization of gl.
class GL_EXPORT GLDisplayEglUtil {
 public:
  // Returns either set instance or stub instance.
  static GLDisplayEglUtil* GetInstance();

  static void SetInstance(GLDisplayEglUtil* gl_display_util);

  // Returns display attributes for the given |platform_type|. Each platform can
  // have different attributes.
  virtual void GetPlatformExtraDisplayAttribs(
      EGLenum platform_type,
      std::vector<EGLAttrib>* attributes) = 0;

  // Sets custom alpha and buffer size for a given platform. By default, the
  // values are not modified.
  virtual void ChoosePlatformCustomAlphaAndBufferSize(EGLint* alpha_size,
                                                      EGLint* buffer_size) = 0;

 protected:
  virtual ~GLDisplayEglUtil() = default;
};

}  // namespace gl

#endif  // UI_GL_GL_DISPLAY_EGL_UTIL_H_
