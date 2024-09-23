// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/vsync_provider_win.h"

namespace gl {
namespace init {

std::vector<GLImplementationParts> GetAllowedGLImplementations() {
  std::vector<GLImplementationParts> impls;
  impls.emplace_back(kGLImplementationEGLANGLE);
  return impls;
}

bool GetGLWindowSystemBindingInfo(const GLVersionInfo& gl_info,
                                  GLWindowSystemBindingInfo* info) {
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE:
      return GetGLWindowSystemBindingInfoEGL(info);
    default:
      return false;
  }
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE:
      return InitializeGLContext(new GLContextEGL(share_group),
                                 compatible_surface, attribs);
    case kGLImplementationMockGL:
    case kGLImplementationStubGL: {
      scoped_refptr<GLContextStub> stub_context =
          base::MakeRefCounted<GLContextStub>(share_group);
      if (GetGLImplementation() == kGLImplementationStubGL) {
        stub_context->SetUseStubApi(true);
      }
      // The stub ctx needs to be initialized so that the gl::GLContext can
      // store the |compatible_surface|.
      stub_context->Initialize(compatible_surface, attribs);
      return stub_context;
    }
    case kGLImplementationDisabled:
      return nullptr;
    default:
      DUMP_WILL_BE_NOTREACHED();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateViewGLSurface(GLDisplay* display,
                                             gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE: {
      DCHECK_NE(window, gfx::kNullAcceleratedWidget);
      return InitializeGLSurface(base::MakeRefCounted<NativeViewGLSurfaceEGL>(
          display->GetAs<gl::GLDisplayEGL>(), window,
          std::make_unique<VSyncProviderWin>(window)));
    }
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub());
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateOffscreenGLSurface(GLDisplay* display,
                                                  const gfx::Size& size) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationEGLANGLE: {
      GLDisplayEGL* display_egl = display->GetAs<gl::GLDisplayEGL>();
      if (display_egl->IsEGLSurfacelessContextSupported() &&
          size.width() == 0 && size.height() == 0) {
        return InitializeGLSurface(new SurfacelessEGL(display_egl, size));
      } else {
        return InitializeGLSurface(new PbufferGLSurfaceEGL(display_egl, size));
      }
    }
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub());
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

void SetDisabledExtensionsPlatform(const std::string& disabled_extensions) {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  switch (implementation) {
    case kGLImplementationEGLANGLE:
      SetDisabledExtensionsEGL(disabled_extensions);
      break;
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

bool InitializeExtensionSettingsOneOffPlatform(GLDisplay* display) {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  switch (implementation) {
    case kGLImplementationEGLANGLE:
      return InitializeExtensionSettingsOneOffEGL(
          display->GetAs<GLDisplayEGL>());
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

}  // namespace init
}  // namespace gl
