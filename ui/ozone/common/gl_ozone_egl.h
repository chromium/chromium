// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_GL_OZONE_EGL_H_
#define UI_OZONE_COMMON_GL_OZONE_EGL_H_

#include "base/functional/callback.h"
#include "third_party/khronos/EGL/eglplatform.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/public/gl_ozone.h"

namespace ui {

// A partial implementation of GLOzone for EGL.
class GLOzoneEGL : public GLOzone {
 public:
  GLOzoneEGL() {}

  GLOzoneEGL(const GLOzoneEGL&) = delete;
  GLOzoneEGL& operator=(const GLOzoneEGL&) = delete;

  ~GLOzoneEGL() override {}

  // GLOzone:
  gl::GLDisplay* InitializeGLOneOffPlatform(
      bool supports_angle,
      std::vector<gl::DisplayType> init_displays,
      gl::GpuPreference gpu_preference) override;
  bool InitializeStaticGLBindings(
      const gl::GLImplementationParts& implementation) override;
  void SetDisabledExtensionsPlatform(
      const std::string& disabled_extensions) override;
  bool InitializeExtensionSettingsOneOffPlatform(
      gl::GLDisplay* display) override;
  void ShutdownGL(gl::GLDisplay* display) override;
  bool CanImportNativePixmap(gfx::BufferFormat format) override;
  std::unique_ptr<NativePixmapGLBinding> ImportNativePixmap(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::BufferPlane plane,
      gfx::Size plane_size,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id) override;
  bool GetGLWindowSystemBindingInfo(
      const gl::GLVersionInfo& gl_info,
      gl::GLWindowSystemBindingInfo* info) override;
  scoped_refptr<gl::GLContext> CreateGLContext(
      gl::GLShareGroup* share_group,
      gl::GLSurface* compatible_surface,
      const gl::GLContextAttribs& attribs) override;
  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override = 0;
  scoped_refptr<gl::Presenter> CreateSurfacelessViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override;
  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override = 0;

 protected:
  // Returns native platform display handle and platform type as per
  // EGL platform extensions.
  // This is used to obtain the EGL display connection for the native display.
  virtual gl::EGLDisplayPlatform GetNativeDisplay() = 0;

  // Sets up GL bindings for the native surface.
  virtual bool LoadGLES2Bindings(
      const gl::GLImplementationParts& implementation) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_GL_OZONE_EGL_H_
