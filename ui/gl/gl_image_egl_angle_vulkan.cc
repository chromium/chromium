// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_egl_angle_vulkan.h"

#include <memory>

#include "base/logging.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

GLImageEGLAngleVulkan::GLImageEGLAngleVulkan(const gfx::Size& size)
    : GLImageEGL(size) {}

GLImageEGLAngleVulkan::~GLImageEGLAngleVulkan() = default;

bool GLImageEGLAngleVulkan::Initialize(unsigned int texture) {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  if (egl_display == EGL_NO_DISPLAY) {
    LOG(ERROR) << "Failed to retrieve EGLDisplay";
    return false;
  }

  GLContext* current_context = GLContext::GetCurrent();
  if (!current_context || !current_context->IsCurrent(nullptr)) {
    LOG(ERROR) << "No gl context bound to the current thread";
    return false;
  }

  EGLContext context_handle =
      reinterpret_cast<EGLContext>(current_context->GetHandle());
  DCHECK_NE(context_handle, EGL_NO_CONTEXT);
  bool result = GLImageEGL::Initialize(
      context_handle, EGL_GL_TEXTURE_2D_KHR,
      reinterpret_cast<EGLClientBuffer>(texture), nullptr);

  if (!result) {
    LOG(ERROR) << "Create EGLImage from texture failed";
    return false;
  }

  return true;
}

VkImage GLImageEGLAngleVulkan::ExportVkImage(VkImageCreateInfo* info) {
  DCHECK(info);

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
  if (egl_display == EGL_NO_DISPLAY) {
    LOG(ERROR) << "Failed to retrieve EGLDisplay";
    return VK_NULL_HANDLE;
  }

  VkImage vk_image = VK_NULL_HANDLE;
  if (!eglExportVkImageANGLE(egl_display, egl_image_, &vk_image, info)) {
    LOG(ERROR) << "Export VkImage from EGLImage failed";
    return VK_NULL_HANDLE;
  }

  DCHECK_NE(vk_image, VK_NULL_HANDLE);
  DCHECK_EQ(info->sType, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
  DCHECK_EQ(size_, gfx::Size(info->extent.width, info->extent.height));
  return vk_image;
}

}  // namespace gl
