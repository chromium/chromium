// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#import <UIKit/UIKit.h>

#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_stub.h"

namespace gl {
namespace init {

std::vector<GLImplementationParts> GetAllowedGLImplementations() {
  std::vector<GLImplementationParts> impls;
  impls.emplace_back(GLImplementationParts(kGLImplementationEGLANGLE));
  return impls;
}

bool GetGLWindowSystemBindingInfo(const GLVersionInfo& gl_info,
                                  GLWindowSystemBindingInfo* info) {
  return false;
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE:
      return InitializeGLContext(new GLContextEGL(share_group),
                                 compatible_surface, attribs);
    case kGLImplementationMockGL:
      return new GLContextStub;
    case kGLImplementationStubGL: {
      scoped_refptr<GLContextStub> stub_context =
          new GLContextStub(share_group);
      stub_context->SetUseStubApi(true);
      return stub_context;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateViewGLSurface(GLDisplay* display,
                                             gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");
  CHECK_NE(kGLImplementationNone, GetGLImplementation());
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE:
      if (window != gfx::kNullAcceleratedWidget) {
        return InitializeGLSurface(new NativeViewGLSurfaceEGL(
            display->GetAs<gl::GLDisplayEGL>(), window.layer, nullptr));
      } else {
        return InitializeGLSurface(new GLSurfaceStub());
      }
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateOffscreenGLSurfaceWithFormat(
    GLDisplay* display,
    const gfx::Size& size,
    GLSurfaceFormat format) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE: {
      GLDisplayEGL* display_egl = display->GetAs<gl::GLDisplayEGL>();
      if (display_egl->IsEGLSurfacelessContextSupported() &&
          size.width() == 0 && size.height() == 0) {
        return InitializeGLSurfaceWithFormat(
            new SurfacelessEGL(display_egl, size), format);
      } else {
        return InitializeGLSurfaceWithFormat(
            new PbufferGLSurfaceEGL(display_egl, size), format);
      }
    }
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return new GLSurfaceStub;
    default:
      NOTREACHED();
      return nullptr;
  }
}

void SetDisabledExtensionsPlatform(const std::string& disabled_extensions) {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  // TODO(zmo): Implement this if needs arise.
}

bool InitializeExtensionSettingsOneOffPlatform(GLDisplay* display) {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  // TODO(zmo): Implement this if needs arise.
  return true;
}

}  // namespace init
}  // namespace gl
