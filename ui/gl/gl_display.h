// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_DISPLAY_H_
#define UI_GL_GL_DISPLAY_H_

#include "ui/gl/gl_export.h"

#if defined(USE_EGL)
typedef void* EGLDisplay;
#endif  // defined(USE_EGL)

namespace gl {

class GL_EXPORT GLDisplay {
 public:
  GLDisplay();

  GLDisplay(const GLDisplay&) = delete;
  GLDisplay& operator=(const GLDisplay&) = delete;

  virtual ~GLDisplay();

  virtual void* GetDisplay() = 0;
};

#if defined(USE_EGL)
class GL_EXPORT GLDisplayEGL : public GLDisplay {
 public:
  GLDisplayEGL();
  explicit GLDisplayEGL(EGLDisplay display);

  GLDisplayEGL(const GLDisplayEGL&) = delete;
  GLDisplayEGL& operator=(const GLDisplayEGL&) = delete;

  ~GLDisplayEGL() override;

  EGLDisplay GetDisplay() override;
  void SetDisplay(EGLDisplay display);

 private:
  EGLDisplay display_;
};
#endif  // defined(USE_EGL)

#if defined(USE_GLX)
class GL_EXPORT GLDisplayX11 : public GLDisplay {
 public:
  GLDisplayX11();

  GLDisplayX11(const GLDisplayX11&) = delete;
  GLDisplayX11& operator=(const GLDisplayX11&) = delete;

  ~GLDisplayX11() override;

  void* GetDisplay() override;
};
#endif  // defined(USE_GLX)

}  // namespace gl

#endif  // UI_GL_GL_DISPLAY_H_