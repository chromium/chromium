// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_egl_api_implementation.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_stub.h"

namespace gl {
namespace init {

namespace {

// Used to render into an already current context+surface,
// that we do not have ownership of (draw callback).
// TODO(boliu): Make this inherit from GLContextEGL.
class GLNonOwnedContext : public GLContextReal {
 public:
  explicit GLNonOwnedContext(GLShareGroup* share_group);

  GLNonOwnedContext(const GLNonOwnedContext&) = delete;
  GLNonOwnedContext& operator=(const GLNonOwnedContext&) = delete;

  // Implement GLContext.
  bool InitializeImpl(GLSurface* compatible_surface,
                      const GLContextAttribs& attribs) override;
  bool MakeCurrentImpl(GLSurface* surface) override;
  void ReleaseCurrent(GLSurface* surface) override {}
  bool IsCurrent(GLSurface* surface) override;
  void* GetHandle() override { return nullptr; }

 protected:
  ~GLNonOwnedContext() override { OnContextWillDestroy(); }

 private:
  EGLDisplay display_;
};

GLNonOwnedContext::GLNonOwnedContext(GLShareGroup* share_group)
    : GLContextReal(share_group), display_(nullptr) {}

bool GLNonOwnedContext::InitializeImpl(GLSurface* compatible_surface,
                                       const GLContextAttribs& attribs) {
  display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  return true;
}

bool GLNonOwnedContext::MakeCurrentImpl(GLSurface* surface) {
  BindGLApi();
  SetCurrent(surface);
  InitializeDynamicBindings();
  return true;
}

bool GLNonOwnedContext::IsCurrent(GLSurface* surface) {
  return GetRealCurrent() == this;
}

}  // namespace

std::vector<GLImplementationParts> GetAllowedGLImplementations() {
  std::vector<GLImplementationParts> impls;
  impls.emplace_back(kGLImplementationEGLGLES2);
  impls.emplace_back(kGLImplementationEGLANGLE);
  return impls;
}

bool GetGLWindowSystemBindingInfo(const GLVersionInfo& gl_info,
                                  GLWindowSystemBindingInfo* info) {
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
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
    case kGLImplementationMockGL:
    case kGLImplementationStubGL: {
      scoped_refptr<GLContextStub> stub_context =
          MakeRefCounted<GLContextStub>(share_group);
      if (GetGLImplementation() == kGLImplementationStubGL) {
        stub_context->SetUseStubApi(true);
      }
      // The stub ctx needs to be initialized so that the gl::GLContext can
      // store the |compatible_surface|.
      stub_context->Initialize(compatible_surface, attribs);
      return stub_context;
    }
    case kGLImplementationDisabled:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    default:
      if (compatible_surface->GetHandle() ||
          compatible_surface->IsSurfaceless()) {
        return InitializeGLContext(new GLContextEGL(share_group),
                                   compatible_surface, attribs);
      } else {
        return InitializeGLContext(new GLNonOwnedContext(share_group),
                                   compatible_surface, attribs);
      }
  }
}

scoped_refptr<GLSurface> CreateViewGLSurface(GLDisplay* display,
                                             gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");
  CHECK_NE(kGLImplementationNone, GetGLImplementation());
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      if (window != gfx::kNullAcceleratedWidget) {
        return InitializeGLSurface(new NativeViewGLSurfaceEGL(
            display->GetAs<gl::GLDisplayEGL>(),
            ScopedANativeWindow::Wrap(window), nullptr));
      } else {
        return InitializeGLSurface(new GLSurfaceStub());
      }
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

scoped_refptr<GLSurface> CreateOffscreenGLSurface(GLDisplay* display,
                                                  const gfx::Size& size) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");
  CHECK_NE(kGLImplementationNone, GetGLImplementation());
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE: {
      GLDisplayEGL* display_egl = display->GetAs<gl::GLDisplayEGL>();
      if (display_egl->IsEGLSurfacelessContextSupported() &&
          (size.width() == 0 && size.height() == 0)) {
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
    case kGLImplementationEGLGLES2:
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
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      return InitializeExtensionSettingsOneOffEGL(
          static_cast<GLDisplayEGL*>(display));
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
