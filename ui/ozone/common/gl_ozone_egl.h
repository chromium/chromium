// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_GL_OZONE_EGL_H_
#define UI_OZONE_COMMON_GL_OZONE_EGL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/khronos/EGL/eglplatform.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/gl_ozone.h"

namespace ui {

// A partial implementation of GLOzone for EGL.
class GLOzoneEGL : public GLOzone {
 public:
  GLOzoneEGL() {}
  ~GLOzoneEGL() override {}

  // GLOzone:
  bool InitializeGLOneOffPlatform() override;
  bool InitializeStaticGLBindings(gl::GLImplementation implementation) override;
  void InitializeDebugGLBindings() override;
  void SetDisabledExtensionsPlatform(
      const std::string& disabled_extensions) override;
  bool InitializeExtensionSettingsOneOffPlatform() override;
  void ShutdownGL() override;
  bool GetGLWindowSystemBindingInfo(
      const gl::GLVersionInfo& gl_info,
      gl::GLWindowSystemBindingInfo* info) override;
  scoped_refptr<gl::GLContext> CreateGLContext(
      gl::GLShareGroup* share_group,
      gl::GLSurface* compatible_surface,
      const gl::GLContextAttribs& attribs) override;
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) override = 0;
  scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gfx::AcceleratedWidget window) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override = 0;

 protected:
  // Returns native platform display handle. This is used to obtain the EGL
  // display connection for the native display.
  virtual EGLNativeDisplayType GetNativeDisplay() = 0;

  // Sets up GL bindings for the native surface.
  virtual bool LoadGLES2Bindings(gl::GLImplementation implementation) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLOzoneEGL);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_GL_OZONE_EGL_H_
