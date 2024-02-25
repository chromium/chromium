// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_gl_egl_utility.h"

#include "base/containers/contains.h"

// From ANGLE's egl/eglext.h. Follows the same approach as in
// ui/gl/gl_surface_egl.cc
#ifndef EGL_ANGLE_platform_angle_device_type_swiftshader
#define EGL_ANGLE_platform_angle_device_type_swiftshader
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE 0x3487
#endif /* EGL_ANGLE_platform_angle_device_type_swiftshader */

#ifndef EGL_ANGLE_platform_angle
#define EGL_ANGLE_platform_angle 1
#define EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE 0x348F
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE 0x3209
#endif /* EGL_ANGLE_platform_angle */

#ifndef EGL_ANGLE_platform_angle_vulkan
#define EGL_ANGLE_platform_angle_vulkan 1
#define EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE 0x34A5
#endif /* EGL_ANGLE_platform_angle_vulkan */

#ifndef EGL_ANGLE_platform_angle_device_type_egl_angle
#define EGL_ANGLE_platform_angle_device_type_egl_angle
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE 0x348E
#endif /* EGL_ANGLE_platform_angle_device_type_egl_angle */

#ifndef EGL_ANGLE_platform_angle_opengl
#define EGL_ANGLE_platform_angle_opengl 1
#define EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE 0x320D
#define EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE 0x320E
#endif /* EGL_ANGLE_platform_angle_opengl */

namespace ui {

WaylandGLEGLUtility::WaylandGLEGLUtility() = default;

WaylandGLEGLUtility::~WaylandGLEGLUtility() = default;

void WaylandGLEGLUtility::GetAdditionalEGLAttributes(
    EGLenum platform_type,
    std::vector<EGLAttrib>* display_attributes) {
  if (base::Contains(*display_attributes,
                     EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE)) {
    display_attributes->push_back(
        EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE);
    display_attributes->push_back(
        EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE);
    return;
  }

  if (std::find(display_attributes->begin(), display_attributes->end(),
                EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE) !=
          display_attributes->end() ||
      std::find(display_attributes->begin(), display_attributes->end(),
                EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE) !=
          display_attributes->end()) {
    display_attributes->push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE);
    display_attributes->push_back(EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE);
    return;
  }
}

void WaylandGLEGLUtility::ChooseEGLAlphaAndBufferSize(EGLint* alpha_size,
                                                      EGLint* buffer_size) {}

void WaylandGLEGLUtility::CollectGpuExtraInfo(
    bool enable_native_gpu_memory_buffers,
    gfx::GpuExtraInfo& gpu_extra_info) const {}

bool WaylandGLEGLUtility::HasVisualManager() {
  return false;
}

}  // namespace ui
