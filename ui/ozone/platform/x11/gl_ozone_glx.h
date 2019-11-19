// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_GL_OZONE_GLX_H_
#define UI_OZONE_PLATFORM_X11_GL_OZONE_GLX_H_

#include "base/macros.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/gl_ozone.h"

namespace ui {

class GLOzoneGLX : public GLOzone {
 public:
  GLOzoneGLX() {}
  ~GLOzoneGLX() override {}

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
      gfx::AcceleratedWidget window) override;
  scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gfx::AcceleratedWidget window) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLOzoneGLX);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_GL_OZONE_GLX_H_
