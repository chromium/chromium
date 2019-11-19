// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_GL_OZONE_H_
#define UI_OZONE_PUBLIC_GL_OZONE_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gpu_preference.h"

namespace gl {
class GLContext;
class GLShareGroup;
class GLSurface;

struct GLContextAttribs;
struct GLVersionInfo;
}

namespace ui {

// Interface that has all of the required methods for an Ozone platform to
// implement a GL implementation. Functions in gl_factory.h and gl_initializer.h
// will delegate to functions in this interface.
class COMPONENT_EXPORT(OZONE_BASE) GLOzone {
 public:
  virtual ~GLOzone() {}

  // Initializes static GL bindings and sets GL implementation.
  virtual bool InitializeStaticGLBindings(
      gl::GLImplementation implementation) = 0;

  // Performs any one off initialization for GL implementation.
  virtual bool InitializeGLOneOffPlatform() = 0;

  // Initializes static debug GL bindings.
  virtual void InitializeDebugGLBindings() = 0;

  // Disables the specified extensions in the window system bindings,
  // e.g., GLX, EGL, etc. This is part of the GPU driver bug workarounds
  // mechanism.
  virtual void SetDisabledExtensionsPlatform(
      const std::string& disabled_extensions) = 0;

  // Initializes extension related settings for window system bindings that
  // will be affected by SetDisabledExtensionsPlatform(). This function is
  // called after SetDisabledExtensionsPlatform() to finalize the bindings.
  virtual bool InitializeExtensionSettingsOneOffPlatform() = 0;

  // Clears static GL bindings.
  virtual void ShutdownGL() = 0;

  // Returns information about the GL window system binding implementation (eg.
  // EGL, GLX, WGL). Returns true if the information was retrieved successfully.
  virtual bool GetGLWindowSystemBindingInfo(
      const gl::GLVersionInfo& gl_info,
      gl::GLWindowSystemBindingInfo* info) = 0;

  // Creates a GL context that is compatible with the given surface.
  // |share_group|, if not null, is a group of contexts which the internally
  // created OpenGL context shares textures and other resources.
  virtual scoped_refptr<gl::GLContext> CreateGLContext(
      gl::GLShareGroup* share_group,
      gl::GLSurface* compatible_surface,
      const gl::GLContextAttribs& attribs) = 0;

  // Creates a GL surface that renders directly to a view.
  virtual scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gfx::AcceleratedWidget window) = 0;

  // Creates a GL surface that renders directly into a window with surfaceless
  // semantics. The surface is not backed by any buffers and is used for
  // overlay-only displays. This will return null if surfaceless mode is
  // unsupported.
  // TODO(spang): Consider deprecating this and using OverlaySurface for GL.
  virtual scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gfx::AcceleratedWidget window) = 0;

  // Creates a GL surface used for offscreen rendering.
  virtual scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      const gfx::Size& size) = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_GL_OZONE_H_
