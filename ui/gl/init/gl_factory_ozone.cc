// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/init/ozone_util.h"
#include "ui/gl/presenter.h"

namespace gl {
namespace init {

std::vector<GLImplementationParts> GetAllowedGLImplementations() {
  DCHECK(GetSurfaceFactoryOzone());
  return GetSurfaceFactoryOzone()->GetAllowedGLImplementations();
}

bool GetGLWindowSystemBindingInfo(const GLVersionInfo& gl_info,
                                  GLWindowSystemBindingInfo* info) {
  return HasGLOzone()
             ? GetGLOzone()->GetGLWindowSystemBindingInfo(gl_info, info)
             : false;
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");

  if (HasGLOzone()) {
    return GetGLOzone()->CreateGLContext(share_group, compatible_surface,
                                         attribs);
  }

  scoped_refptr<GLContextStub> context;
  switch (GetGLImplementation()) {
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
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Expected Mock or Stub, actual:" << GetGLImplementation();
  }
  return nullptr;
}

scoped_refptr<GLSurface> CreateViewGLSurface(GLDisplay* display,
                                             gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");

  if (HasGLOzone())
    return GetGLOzone()->CreateViewGLSurface(display, window);

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub());
    default:
      NOTREACHED_IN_MIGRATION()
          << "Expected Mock or Stub, actual:" << GetGLImplementation();
  }

  return nullptr;
}

scoped_refptr<Presenter> CreateSurfacelessViewGLSurface(
    GLDisplay* display,
    gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateSurfacelessViewGLSurface");
  return HasGLOzone()
             ? GetGLOzone()->CreateSurfacelessViewGLSurface(display, window)
             : nullptr;
}

scoped_refptr<GLSurface> CreateOffscreenGLSurface(GLDisplay* display,
                                                  const gfx::Size& size) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");

  if (HasGLOzone())
    return GetGLOzone()->CreateOffscreenGLSurface(display, size);

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub);
    default:
      NOTREACHED_IN_MIGRATION()
          << "Expected Mock or Stub, actual:" << GetGLImplementation();
  }
  return nullptr;
}

void SetDisabledExtensionsPlatform(const std::string& disabled_extensions) {
  if (HasGLOzone()) {
    GetGLOzone()->SetDisabledExtensionsPlatform(disabled_extensions);
    return;
  }

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Expected Mock or Stub, actual:" << GetGLImplementation();
  }
}

bool InitializeExtensionSettingsOneOffPlatform(GLDisplay* display) {
  if (HasGLOzone())
    return GetGLOzone()->InitializeExtensionSettingsOneOffPlatform(display);

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return true;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Expected Mock or Stub, actual:" << GetGLImplementation();
      return false;
  }
}

}  // namespace init
}  // namespace gl
