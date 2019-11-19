// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_factory.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_cgl.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_stub.h"
#include "ui/gl/gl_switches.h"

#if defined(USE_EGL)
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface_egl.h"
#endif  // defined(USE_EGL)

namespace gl {
namespace init {

namespace {

// A "no-op" surface. It is not required that a CGLContextObj have an
// associated drawable (pbuffer or fullscreen context) in order to be
// made current. Everywhere this surface type is used, we allocate an
// FBO at the user level as the drawable of the associated context.
class NoOpGLSurface : public GLSurface {
 public:
  explicit NoOpGLSurface(const gfx::Size& size) : size_(size) {}

  // Implement GLSurface.
  bool Initialize(GLSurfaceFormat format) override { return true; }
  void Destroy() override {}
  bool IsOffscreen() override { return true; }
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override {
    NOTREACHED() << "Cannot call SwapBuffers on a NoOpGLSurface.";
    return gfx::SwapResult::SWAP_FAILED;
  }
  gfx::Size GetSize() override { return size_; }
  void* GetHandle() override { return nullptr; }
  void* GetDisplay() override { return nullptr; }
  bool IsSurfaceless() const override { return true; }
  GLSurfaceFormat GetFormat() override { return GLSurfaceFormat(); }

 protected:
  ~NoOpGLSurface() override {}

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(NoOpGLSurface);
};

}  // namespace

std::vector<GLImplementation> GetAllowedGLImplementations() {
  std::vector<GLImplementation> impls;
  impls.push_back(kGLImplementationDesktopGLCoreProfile);
  impls.push_back(kGLImplementationDesktopGL);
  impls.push_back(kGLImplementationAppleGL);
#if defined(USE_EGL)
  impls.push_back(kGLImplementationEGLGLES2);
  impls.push_back(kGLImplementationEGLANGLE);
  impls.push_back(kGLImplementationSwiftShaderGL);
#endif  // defined(USE_EGL)
  return impls;
}

bool GetGLWindowSystemBindingInfo(const GLVersionInfo& gl_info,
                                  GLWindowSystemBindingInfo* info) {
  return false;
}

scoped_refptr<GLContext> CreateGLContext(GLShareGroup* share_group,
                                         GLSurface* compatible_surface,
                                         const GLContextAttribs& attribs) {
  TRACE_EVENT0("gpu", "gl::init::CreateGLContext");
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
      return InitializeGLContext(new GLContextCGL(share_group),
                                 compatible_surface, attribs);
#if defined(USE_EGL)
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
    case kGLImplementationSwiftShaderGL:
      return InitializeGLContext(new GLContextEGL(share_group),
                                 compatible_surface, attribs);
#endif  // defined(USE_EGL)
    case kGLImplementationMockGL:
      return new GLContextStub(share_group);
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

scoped_refptr<GLSurface> CreateViewGLSurface(gfx::AcceleratedWidget window) {
  TRACE_EVENT0("gpu", "gl::init::CreateViewGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
    case kGLImplementationSwiftShaderGL: {
      NOTIMPLEMENTED() << "No onscreen support on Mac.";
      return nullptr;
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
    const gfx::Size& size, GLSurfaceFormat format) {
  TRACE_EVENT0("gpu", "gl::init::CreateOffscreenGLSurface");
  switch (GetGLImplementation()) {
    case kGLImplementationDesktopGL:
    case kGLImplementationDesktopGLCoreProfile:
    case kGLImplementationAppleGL:
      return InitializeGLSurfaceWithFormat(
          new NoOpGLSurface(size), format);
#if defined(USE_EGL)
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
    case kGLImplementationSwiftShaderGL:
      if (GLSurfaceEGL::IsEGLSurfacelessContextSupported() &&
          size.width() == 0 && size.height() == 0) {
        return InitializeGLSurfaceWithFormat(new SurfacelessEGL(size), format);
      } else {
        return InitializeGLSurfaceWithFormat(new PbufferGLSurfaceEGL(size),
                                             format);
      }
#endif  // defined(USE_EGL)
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

bool InitializeExtensionSettingsOneOffPlatform() {
  GLImplementation implementation = GetGLImplementation();
  DCHECK_NE(kGLImplementationNone, implementation);
  // TODO(zmo): Implement this if needs arise.
  return true;
}

}  // namespace init
}  // namespace gl
