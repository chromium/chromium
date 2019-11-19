// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_WGL_H_
#define UI_GL_GL_SURFACE_WGL_H_

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface.h"

namespace gl {

// Base interface for WGL surfaces.
class GL_EXPORT GLSurfaceWGL : public GLSurface {
 public:
  GLSurfaceWGL();

  // Implement GLSurface.
  void* GetDisplay() override;

  static bool InitializeOneOff();
  static bool InitializeExtensionSettingsOneOff();
  static void InitializeOneOffForTesting();
  static HDC GetDisplayDC();

 protected:
  ~GLSurfaceWGL() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLSurfaceWGL);
  static bool initialized_;
};

// A surface used to render to a view.
class GL_EXPORT NativeViewGLSurfaceWGL : public GLSurfaceWGL {
 public:
  explicit NativeViewGLSurfaceWGL(gfx::AcceleratedWidget window);

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  bool Recreate() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  GLSurfaceFormat GetFormat() override;
  void SetVSyncEnabled(bool enabled) override;

 private:
  ~NativeViewGLSurfaceWGL() override;

  GLSurfaceFormat format_;

  gfx::AcceleratedWidget window_;
  gfx::AcceleratedWidget child_window_;
  HDC device_context_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceWGL);
};


// A surface used to render to an offscreen pbuffer.
class GL_EXPORT PbufferGLSurfaceWGL : public GLSurfaceWGL {
 public:
  explicit PbufferGLSurfaceWGL(const gfx::Size& size);

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  GLSurfaceFormat GetFormat() override;

 private:
  ~PbufferGLSurfaceWGL() override;

  gfx::Size size_;
  HDC device_context_;
  void* pbuffer_;

  DISALLOW_COPY_AND_ASSIGN(PbufferGLSurfaceWGL);
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_WGL_H_
