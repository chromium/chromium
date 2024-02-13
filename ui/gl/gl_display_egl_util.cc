// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_display_egl_util.h"

#include <optional>

#include "base/no_destructor.h"
#include "base/scoped_environment_variable_override.h"

namespace gl {

namespace {

static GLDisplayEglUtil* g_instance = nullptr;

class GLDisplayEglUtilStub : public GLDisplayEglUtil {
 public:
  static GLDisplayEglUtilStub* GetInstance() {
    static base::NoDestructor<GLDisplayEglUtilStub> instance;
    return instance.get();
  }
  void GetPlatformExtraDisplayAttribs(
      EGLenum platform_type,
      std::vector<EGLAttrib>* attributes) override {}

  void ChoosePlatformCustomAlphaAndBufferSize(EGLint* alpha_size,
                                              EGLint* buffer_size) override {}
  std::optional<base::ScopedEnvironmentVariableOverride>
  MaybeGetScopedDisplayUnsetForVulkan() override {
    return std::nullopt;
  }

 private:
  friend base::NoDestructor<GLDisplayEglUtilStub>;

  GLDisplayEglUtilStub() = default;
  ~GLDisplayEglUtilStub() override = default;
  GLDisplayEglUtilStub(const GLDisplayEglUtilStub& util) = delete;
  GLDisplayEglUtilStub& operator=(const GLDisplayEglUtilStub& util) = delete;
};

}  // namespace

// static
GLDisplayEglUtil* GLDisplayEglUtil::GetInstance() {
  // If a platform specific impl is not set, create a stub instance.
  if (!g_instance)
    SetInstance(GLDisplayEglUtilStub::GetInstance());
  return g_instance;
}

// static
void GLDisplayEglUtil::SetInstance(GLDisplayEglUtil* gl_display_util) {
  g_instance = gl_display_util;
}

}  // namespace gl
