// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_GL_EGL_UTILITY_H_
#define UI_OZONE_PUBLIC_PLATFORM_GL_EGL_UTILITY_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "base/component_export.h"
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

  // Returns whether the platform supports setting transparent background for
  // windows.
  virtual bool IsTransparentBackgroundSupported() const = 0;

  // Fills in the platform specific bits of the GPU extra info holder.
  // |enable_native_gpu_memory_buffers| should be taken from GpuPreferences.
  virtual void CollectGpuExtraInfo(bool enable_native_gpu_memory_buffers,
                                   gfx::GpuExtraInfo& gpu_extra_info) const = 0;

  // X11 specific; returns whether the test configuration supports alpha for
  // window visuals.
  virtual bool X11DoesVisualHaveAlphaForTest() const = 0;

  // X11 specific; returns whether the platform supports visuals.
  virtual bool HasVisualManager();

  // X11 specific; sets new visuals.
  // Must be called only if the X11 visual manager is available.
  // Should be called when the updated GPU info is available.
  // Returns whether the visuals provided were valid.
  virtual bool UpdateVisualsOnGpuInfoChanged(bool software_rendering,
                                             uint32_t default_visual_id,
                                             uint32_t transparent_visual_id);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_GL_EGL_UTILITY_H_
