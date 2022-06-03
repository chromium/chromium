// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_DISPLAY_EGL_UTIL_OZONE_H_
#define UI_GL_INIT_GL_DISPLAY_EGL_UTIL_OZONE_H_

#include <vector>

#include "base/no_destructor.h"
#include "ui/gl/gl_display_egl_util.h"

namespace gl {

// Forwards calls to PlatformGLEGLUtility. It might be implemented by some
// platforms.
class GLDisplayEglUtilOzone : public GLDisplayEglUtil {
 public:
  static GLDisplayEglUtilOzone* GetInstance();

  // GLDisplayEglUtil overrides:
  void GetPlatformExtraDisplayAttribs(
      EGLenum platform_type,
      std::vector<EGLAttrib>* attributes) override;
  void ChoosePlatformCustomAlphaAndBufferSize(EGLint* alpha_size,
                                              EGLint* buffer_size) override;

 private:
  friend base::NoDestructor<GLDisplayEglUtilOzone>;

  GLDisplayEglUtilOzone();
  ~GLDisplayEglUtilOzone() override;
  GLDisplayEglUtilOzone(const GLDisplayEglUtilOzone& util) = delete;
  GLDisplayEglUtilOzone& operator=(const GLDisplayEglUtilOzone& util) = delete;
};

}  // namespace gl

#endif  // UI_GL_INIT_GL_DISPLAY_EGL_UTIL_OZONE_H_
