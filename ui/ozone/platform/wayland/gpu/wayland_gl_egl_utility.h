// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_GL_EGL_UTILITY_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_GL_EGL_UTILITY_H_

#include "ui/ozone/public/platform_gl_egl_utility.h"

namespace ui {

// Allows EGL to ask platforms for platform specific EGL attributes.
class WaylandGLEGLUtility : public PlatformGLEGLUtility {
 public:
  WaylandGLEGLUtility();
  ~WaylandGLEGLUtility() override;
  WaylandGLEGLUtility(const WaylandGLEGLUtility& util) = delete;
  WaylandGLEGLUtility& operator=(const WaylandGLEGLUtility& util) = delete;

  // PlatformGLEGLUtility overrides:
  void GetAdditionalEGLAttributes(
      EGLenum platform_type,
      std::vector<EGLAttrib>* display_attributes) override;
  void ChooseEGLAlphaAndBufferSize(EGLint* alpha_size,
                                   EGLint* buffer_size) override;
  void CollectGpuExtraInfo(bool enable_native_gpu_memory_buffers,
                           gfx::GpuExtraInfo& gpu_extra_info) const override;
  bool HasVisualManager() override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_GL_EGL_UTILITY_H_
