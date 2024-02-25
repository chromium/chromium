// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_gl_egl_utility.h"

#include "base/containers/contains.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/visual_manager.h"
#include "ui/gl/gl_surface_egl.h"

#ifndef EGL_ANGLE_x11_visual
#define EGL_ANGLE_x11_visual 1
#define EGL_X11_VISUAL_ID_ANGLE 0x33A3
#endif /* EGL_ANGLE_x11_visual */

#ifndef EGL_ANGLE_platform_angle_null
#define EGL_ANGLE_platform_angle_null 1
#define EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE 0x33AE
#endif /* EGL_ANGLE_platform_angle_null */

#ifndef EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE
#define EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE 0x348F
#endif

#ifndef EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE 0x3487
#endif

#ifndef EGL_ANGLE_platform_angle
#define EGL_ANGLE_platform_angle 1
#define EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE 0x348F
#endif /* EGL_ANGLE_platform_angle */

#ifndef EGL_EXT_platform_x11
#define EGL_EXT_platform_x11 1
#define EGL_PLATFORM_X11_EXT 0x31D5
#endif /* EGL_EXT_platform_x11 */

namespace ui {

void GetPlatformExtraDisplayAttribs(EGLenum platform_type,
                                    std::vector<EGLAttrib>* attributes) {
  // ANGLE_NULL and SwiftShader backends don't use the visual,
  // and may run without X11 where we can't get it anyway.
  if ((platform_type != EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE) &&
      !base::Contains(*attributes,
                      EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE)) {
    x11::VisualId visual_id;
    x11::Connection::Get()->GetOrCreateVisualManager().ChooseVisualForWindow(
        true, &visual_id, nullptr, nullptr, nullptr);
    attributes->push_back(EGL_X11_VISUAL_ID_ANGLE);
    attributes->push_back(static_cast<EGLAttrib>(visual_id));
    attributes->push_back(EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE);
    attributes->push_back(EGL_PLATFORM_X11_EXT);
  }
}

void ChoosePlatformCustomAlphaAndBufferSize(EGLint* alpha_size,
                                            EGLint* buffer_size) {
  // If we're using ANGLE_NULL, we may not have a display, in which case we
  // can't use XVisualManager.
  if (gl::GLSurfaceEGL::GetGLDisplayEGL()->GetNativeDisplay().GetDisplay() !=
      EGL_DEFAULT_DISPLAY) {
    uint8_t depth;
    x11::Connection::Get()->GetOrCreateVisualManager().ChooseVisualForWindow(
        true, nullptr, &depth, nullptr, nullptr);
    *buffer_size = depth;
    *alpha_size = *buffer_size == 32 ? 8 : 0;
  }
}

}  // namespace ui
