// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_egl_angle_vulkan.h"

#include <memory>

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface_egl.h"

#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#define EGL_VULKAN_IMAGE_ANGLE 0x34D3
#define EGL_VULKAN_IMAGE_CREATE_INFO_HI_ANGLE 0x34D4
#define EGL_VULKAN_IMAGE_CREATE_INFO_LO_ANGLE 0x34D5

namespace gl {

GLImageEGLAngleVulkan::GLImageEGLAngleVulkan(const gfx::Size& size)
    : GLImageEGL(size) {}

GLImageEGLAngleVulkan::~GLImageEGLAngleVulkan() = default;

bool GLImageEGLAngleVulkan::Initialize(VkImage image,
                                       const VkImageCreateInfo* create_info,
                                       unsigned int internal_format) {
  DCHECK(image != VK_NULL_HANDLE);
  DCHECK(create_info);

  uint64_t info = reinterpret_cast<uint64_t>(create_info);
  EGLint attribs[] = {
      EGL_VULKAN_IMAGE_CREATE_INFO_HI_ANGLE,
      static_cast<EGLint>((info >> 32) & 0xffffffff),
      EGL_VULKAN_IMAGE_CREATE_INFO_LO_ANGLE,
      static_cast<EGLint>(info & 0xffffffff),
      EGL_TEXTURE_INTERNAL_FORMAT_ANGLE,
      static_cast<EGLint>(internal_format),
      EGL_NONE,
  };

  bool result = GLImageEGL::Initialize(
      EGL_NO_CONTEXT, EGL_VULKAN_IMAGE_ANGLE,
      reinterpret_cast<EGLClientBuffer>(&image), attribs);

  if (!result) {
    LOG(ERROR) << "Create EGLImage from VkImage failed";
    return false;
  }

  return true;
}

}  // namespace gl
