// Copyright 2016 The Chromium Authors. All rights reserved.
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

#if defined(USE_OZONE)
#include "ui/gl/init/ozone_util.h"
#endif

#if defined(USE_X11)
#include "ui/base/ui_base_features.h"
#include "ui/gl/init/gl_factory_linux_x11.h"
#endif

namespace gl {
namespace init {

std::vector<GLImplementationParts> GetAllowedGLImplementations() {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    return gl::init::GetAllowedGLImplementationsX11();
#endif

#if defined(USE_OZONE)
  DCHECK(GetSurfaceFactoryOzone());
  return GetSurfaceFactoryOzone()->GetAllowedGLImplementations();
#else
  NOTREACHED();
  return {};
#endif
}

bool GetGLWindowSystemBindingInfo(const GLVersionInfo& gl_info,
                                  GLWindowSystemBindingInfo* info) {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    return gl::init::GetGLWindowSystemBindingInfoX11(gl_info, info);
#endif

#if defined(USE_OZONE)
  if (HasGLOzone())
    return GetGLOzone()->GetGLWindowSystemBindingInfo(gl_info, info);
#endif

  return false;
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    return gl::init::CreateGLContextX11(share_group, compatible_surface,
                                        attribs);
#endif

#if defined(USE_OZONE)
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");

  if (HasGLOzone()) {
    return GetGLOzone()->CreateGLContext(share_group, compatible_surface,
                                         attribs);
  }

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
      return scoped_refptr<GLContext>(new GLContextStub(share_group));
    case kGLImplementationStubGL: {
      scoped_refptr<GLContextStub> stub_context =
          new GLContextStub(share_group);
      stub_context->SetUseStubApi(true);
      return stub_context;
    }
    case kGLImplementationDisabled:
      return nullptr;
    default:
      NOTREACHED() << "Expected Mock or Stub, actual:" << GetGLImplementation();
  }
#endif

  return nullptr;
}

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    return gl::init::CreateViewGLSurfaceX11(window);
#endif

#if defined(USE_OZONE)
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");

  if (HasGLOzone())
    return GetGLOzone()->CreateViewGLSurface(window);

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub());
    default:
      NOTREACHED() << "Expected Mock or Stub, actual:" << GetGLImplementation();
  }
#endif

  return nullptr;
}

scoped_refptr<GLSurface> CreateSurfacelessViewGLSurface(
    gfx::AcceleratedWidget window) {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform()) {
    return nullptr;
  }
#endif

#if defined(USE_OZONE)
  TRACE_EVENT0("gpu", "gl::init::CreateSurfacelessViewGLSurface");

  if (HasGLOzone())
    return GetGLOzone()->CreateSurfacelessViewGLSurface(window);
#endif

  return nullptr;
}

scoped_refptr<GLSurface> CreateOffscreenGLSurfaceWithFormat(
    const gfx::Size& size, GLSurfaceFormat format) {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    return gl::init::CreateOffscreenGLSurfaceWithFormatX11(size, format);
#endif

#if defined(USE_OZONE)
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");

  if (!format.IsCompatible(GLSurfaceFormat())) {
    NOTREACHED() << "FATAL: Ozone only supports default-format surfaces.";
    return nullptr;
  }

  if (HasGLOzone())
    return GetGLOzone()->CreateOffscreenGLSurface(size);

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return InitializeGLSurface(new GLSurfaceStub);
    default:
      NOTREACHED() << "Expected Mock or Stub, actual:" << GetGLImplementation();
  }
#endif

  return nullptr;
}

void SetDisabledExtensionsPlatform(const std::string& disabled_extensions) {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform()) {
    gl::init::SetDisabledExtensionsPlatformX11(disabled_extensions);
    return;
  }
#endif

#if defined(USE_OZONE)
  if (HasGLOzone()) {
    GetGLOzone()->SetDisabledExtensionsPlatform(disabled_extensions);
    return;
  }

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      break;
    default:
      NOTREACHED() << "Expected Mock or Stub, actual:" << GetGLImplementation();
  }
#endif
}

bool InitializeExtensionSettingsOneOffPlatform() {
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    return gl::init::InitializeExtensionSettingsOneOffPlatformX11();
#endif

#if defined(USE_OZONE)
  if (HasGLOzone())
    return GetGLOzone()->InitializeExtensionSettingsOneOffPlatform();

  switch (GetGLImplementation()) {
    case kGLImplementationMockGL:
    case kGLImplementationStubGL:
      return true;
    default:
      NOTREACHED() << "Expected Mock or Stub, actual:" << GetGLImplementation();
      return false;
  }
#endif
  return false;
}

}  // namespace init
}  // namespace gl
