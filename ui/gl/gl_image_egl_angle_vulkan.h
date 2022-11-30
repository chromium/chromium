// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_EGL_ANGLE_VULKAN_H_
#define UI_GL_GL_IMAGE_EGL_ANGLE_VULKAN_H_

#include <stdint.h>
#include <vulkan/vulkan_core.h>

#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image_egl.h"

namespace gl {

class GL_EXPORT GLImageEGLAngleVulkan : public GLImageEGL {
 public:
  explicit GLImageEGLAngleVulkan(const gfx::Size& size);

  GLImageEGLAngleVulkan(const GLImageEGLAngleVulkan&) = delete;
  GLImageEGLAngleVulkan& operator=(const GLImageEGLAngleVulkan&) = delete;

  bool Initialize(VkImage image,
                  const VkImageCreateInfo* create_info,
                  unsigned int internal_format);

 protected:
  ~GLImageEGLAngleVulkan() override;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_EGL_ANGLE_VULKAN_H_
