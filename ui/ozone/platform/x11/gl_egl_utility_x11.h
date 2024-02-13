// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_GL_EGL_UTILITY_X11_H_
#define UI_OZONE_PLATFORM_X11_GL_EGL_UTILITY_X11_H_

#include "ui/ozone/public/platform_gl_egl_utility.h"

namespace ui {

// Allows EGL to ask platforms for platform specific EGL attributes.
class GLEGLUtilityX11 : public PlatformGLEGLUtility {
 public:
  GLEGLUtilityX11();
  ~GLEGLUtilityX11() override;
  GLEGLUtilityX11(const GLEGLUtilityX11& util) = delete;
  GLEGLUtilityX11& operator=(const GLEGLUtilityX11& util) = delete;

  // PlatformGLEGLUtility overrides:
  void GetAdditionalEGLAttributes(
      EGLenum platform_type,
      std::vector<EGLAttrib>* display_attributes) override;
  void ChooseEGLAlphaAndBufferSize(EGLint* alpha_size,
                                   EGLint* buffer_size) override;
  void CollectGpuExtraInfo(bool enable_native_gpu_memory_buffers,
                           gfx::GpuExtraInfo& gpu_extra_info) const override;
  bool HasVisualManager() override;
  std::optional<base::ScopedEnvironmentVariableOverride>
  MaybeGetScopedDisplayUnsetForVulkan() override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_GL_EGL_UTILITY_X11_H_
