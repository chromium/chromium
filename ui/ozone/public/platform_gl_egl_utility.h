// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_GL_EGL_UTILITY_H_
#define UI_OZONE_PUBLIC_PLATFORM_GL_EGL_UTILITY_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/scoped_environment_variable_override.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/gpu_extra_info.h"

namespace ui {

// Provides platform specific EGL attributes/configs.
class COMPONENT_EXPORT(OZONE_BASE) PlatformGLEGLUtility {
 public:
  PlatformGLEGLUtility();
  virtual ~PlatformGLEGLUtility();

  // Gets additional display attributes based on |platform_type|.
  virtual void GetAdditionalEGLAttributes(
      EGLenum platform_type,
      std::vector<EGLAttrib>* display_attributes) = 0;

  // Chooses alpha and buffer size values.
  virtual void ChooseEGLAlphaAndBufferSize(EGLint* alpha_size,
                                           EGLint* buffer_size) = 0;

  // Fills in the platform specific bits of the GPU extra info holder.
  // |enable_native_gpu_memory_buffers| should be taken from GpuPreferences.
  virtual void CollectGpuExtraInfo(bool enable_native_gpu_memory_buffers,
                                   gfx::GpuExtraInfo& gpu_extra_info) const = 0;

  // X11 specific; returns whether the platform supports visuals.
  virtual bool HasVisualManager();

  // X11 specific; returns scoped unset display env variable if vulkan surface
  // is not supported.
  virtual std::optional<base::ScopedEnvironmentVariableOverride>
  MaybeGetScopedDisplayUnsetForVulkan();
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_GL_EGL_UTILITY_H_
