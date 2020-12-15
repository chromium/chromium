// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/gl_egl_utility_x11.h"

#include "ui/base/x/x11_gl_egl_utility.h"
#include "ui/base/x/x11_util.h"
#include "ui/gl/gl_utils.h"

namespace ui {

GLEGLUtilityX11::GLEGLUtilityX11() = default;
GLEGLUtilityX11::~GLEGLUtilityX11() = default;

void GLEGLUtilityX11::GetAdditionalEGLAttributes(
    EGLenum platform_type,
    std::vector<EGLAttrib>* display_attributes) {
  GetPlatformExtraDisplayAttribs(platform_type, display_attributes);
}

void GLEGLUtilityX11::ChooseEGLAlphaAndBufferSize(EGLint* alpha_size,
                                                  EGLint* buffer_size) {
  ChoosePlatformCustomAlphaAndBufferSize(alpha_size, buffer_size);
}

bool GLEGLUtilityX11::IsTransparentBackgroundSupported() const {
  return ui::IsTransparentBackgroundSupported();
}

void GLEGLUtilityX11::CollectGpuExtraInfo(
    bool enable_native_gpu_memory_buffers,
    gfx::GpuExtraInfo& gpu_extra_info) const {
  gl::CollectX11GpuExtraInfo(enable_native_gpu_memory_buffers, gpu_extra_info);
}

bool GLEGLUtilityX11::X11DoesVisualHaveAlphaForTest() const {
  return ui::DoesVisualHaveAlphaForTest();
}

bool GLEGLUtilityX11::HasVisualManager() {
  return true;
}

bool GLEGLUtilityX11::UpdateVisualsOnGpuInfoChanged(
    bool software_rendering,
    uint32_t default_visual_id,
    uint32_t transparent_visual_id) {
  return ui::UpdateVisualsOnGpuInfoChanged(
      software_rendering, static_cast<x11::VisualId>(default_visual_id),
      static_cast<x11::VisualId>(transparent_visual_id));
}

}  // namespace ui
