// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/init/gl_display_egl_util_ozone.h"

#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_gl_egl_utility.h"

namespace gl {

// static
GLDisplayEglUtilOzone* GLDisplayEglUtilOzone::GetInstance() {
  static base::NoDestructor<GLDisplayEglUtilOzone> instance;
  return instance.get();
}

void GLDisplayEglUtilOzone::GetPlatformExtraDisplayAttribs(
    EGLenum platform_type,
    std::vector<EGLAttrib>* attributes) {
  auto* utility = ui::OzonePlatform::GetInstance()->GetPlatformGLEGLUtility();
  if (utility)
    utility->GetAdditionalEGLAttributes(platform_type, attributes);
}

void GLDisplayEglUtilOzone::ChoosePlatformCustomAlphaAndBufferSize(
    EGLint* alpha_size,
    EGLint* buffer_size) {
  auto* utility = ui::OzonePlatform::GetInstance()->GetPlatformGLEGLUtility();
  if (utility)
    utility->ChooseEGLAlphaAndBufferSize(alpha_size, buffer_size);
}

GLDisplayEglUtilOzone::GLDisplayEglUtilOzone() = default;

GLDisplayEglUtilOzone::~GLDisplayEglUtilOzone() = default;

}  // namespace gl
