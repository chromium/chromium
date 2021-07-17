// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INIT_GL_FACTORY_LINUX_X11_H_
#define UI_GL_INIT_GL_FACTORY_LINUX_X11_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_format.h"
#include "ui/gl/gpu_preference.h"

namespace gl {

class GLContext;
class GLShareGroup;
class GLSurface;

struct GLContextAttribs;
struct GLVersionInfo;

// Note that this is a temporary implementation for Linux/X11 GL. It is called
// through GLFactoryOzone, and will be removed as soon as Linux/Ozone is
// the default. Comments have been copied from gl_factory.h
//
// TODO(msisov): remove this once Ozone is the default on Linux.
namespace init {

// Returns a list of allowed GL implementations. The default implementation will
// be the first item.
std::vector<GLImplementationParts> GetAllowedGLImplementationsX11();

// Initializes GL bindings and extension settings.
bool InitializeGLOneOffX11();

// Initializes GL bindings without initializing extension settings.
bool InitializeGLNoExtensionsOneOffX11(bool init_bindings);

// Initializes GL bindings - load dlls and get proc address according to gl
// command line switch.
bool InitializeStaticGLBindingsOneOffX11();

// Initialize plaiform dependent extension settings, including bindings,
// capabilities, etc.
bool InitializeExtensionSettingsOneOffPlatformX11();

// Initializes GL bindings using the provided parameters. This might be required
// for use in tests.
bool InitializeStaticGLBindingsImplementationX11(GLImplementation impl,
                                                 bool fallback_to_software_gl);

// Initializes GL platform using the provided parameters. This might be required
// for use in tests. This should be called only after GL bindings are initilzed
// successfully.
bool InitializeGLOneOffPlatformImplementationX11(bool fallback_to_software_gl,
                                                 bool disable_gl_drawing,
                                                 bool init_extensions);

// Clears GL bindings and resets GL implementation.
void ShutdownGLX11(bool due_to_fallback);

// Return information about the GL window system binding implementation (e.g.,
// EGL, GLX, WGL). Returns true if the information was retrieved successfully.
bool GetGLWindowSystemBindingInfoX11(const GLVersionInfo& gl_info,
                                     GLWindowSystemBindingInfo* info);

// Creates a GL context that is compatible with the given surface.
// |share_group|, if non-null, is a group of contexts which the internally
// created OpenGL context shares textures and other resources.
scoped_refptr<GLContext> CreateGLContextX11(GLShareGroup* share_group,
                                            GLSurface* compatible_surface,
                                            const GLContextAttribs& attribs);

// Creates a GL surface that renders directly to a view.
scoped_refptr<GLSurface> CreateViewGLSurfaceX11(gfx::AcceleratedWidget window);

// Creates a GL surface used for offscreen rendering.
scoped_refptr<GLSurface> CreateOffscreenGLSurfaceX11(const gfx::Size& size);

scoped_refptr<GLSurface> CreateOffscreenGLSurfaceWithFormatX11(
    const gfx::Size& size,
    GLSurfaceFormat format);

// Set platform dependent disabled extensions and re-initialize extension
// bindings.
void SetDisabledExtensionsPlatformX11(const std::string& disabled_extensions);

}  // namespace init

}  // namespace gl

#endif  // UI_GL_INIT_GL_FACTORY_LINUX_X11_H_
