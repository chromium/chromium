// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_context_stub.h"

#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_stub_api.h"

namespace gl {

GLContextStub::GLContextStub() : GLContextStub(nullptr) {}
GLContextStub::GLContextStub(GLShareGroup* share_group)
    : GLContextReal(share_group),
      use_stub_api_(false),
      version_str_("OpenGL ES 3.0") {
  SetExtensionsString("GL_EXT_framebuffer_object");
}

bool GLContextStub::Initialize(GLSurface* compatible_surface,
                               const GLContextAttribs& attribs) {
  return true;
}

bool GLContextStub::MakeCurrent(GLSurface* surface) {
  DCHECK(surface);
  BindGLApi();
  SetCurrent(surface);
  InitializeDynamicBindings();
  return true;
}

void GLContextStub::ReleaseCurrent(GLSurface* surface) {
  SetCurrent(nullptr);
}

bool GLContextStub::IsCurrent(GLSurface* surface) {
  return GetRealCurrent() == this;
}

void* GLContextStub::GetHandle() {
  return nullptr;
}

std::string GLContextStub::GetGLVersion() {
  return version_str_;
}

std::string GLContextStub::GetGLRenderer() {
  return std::string("CHROMIUM");
}

unsigned int GLContextStub::CheckStickyGraphicsResetStatus() {
  DCHECK(IsCurrent(nullptr));
  if ((graphics_reset_status_ == GL_NO_ERROR) && HasRobustness()) {
    graphics_reset_status_ = glGetGraphicsResetStatusARB();
  }
  return graphics_reset_status_;
}

void GLContextStub::SetUseStubApi(bool stub_api) {
  use_stub_api_ = stub_api;
}

void GLContextStub::SetExtensionsString(const char* extensions) {
  SetExtensionsFromString(extensions);
}

void GLContextStub::SetGLVersionString(const char* version_str) {
  version_str_ = std::string(version_str ? version_str : "");
}

bool GLContextStub::HasRobustness() {
  return HasExtension("GL_ARB_robustness") ||
         HasExtension("GL_KHR_robustness") || HasExtension("GL_EXT_robustness");
}

#if defined(OS_MACOSX)
void GLContextStub::FlushForDriverCrashWorkaround() {}
#endif

GLContextStub::~GLContextStub() {}

GLApi* GLContextStub::CreateGLApi(DriverGL* driver) {
  if (use_stub_api_) {
    GLStubApi* stub_api = new GLStubApi();
    if (!version_str_.empty()) {
      stub_api->set_version(version_str_);
    }
    if (!extension_string().empty()) {
      stub_api->set_extensions(extension_string());
    }
    return stub_api;
  }

  return GLContext::CreateGLApi(driver);
}

}  // namespace gl
