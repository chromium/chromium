// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_angle_util_vulkan.h"

#include "third_party/angle/include/EGL/egl.h"
#include "third_party/angle/include/EGL/eglext.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {
namespace {
EGLDeviceEXT GetEGLDeviceFromANGLE() {
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay();
  if (egl_display == EGL_NO_DISPLAY) {
    LOG(ERROR) << "Failed to retrieve EGLDisplay";
    return nullptr;
  }

  if (!gl::g_driver_egl.client_ext.b_EGL_EXT_device_query) {
    LOG(ERROR) << "EGL_EXT_device_query not supported";
    return nullptr;
  }

  intptr_t egl_device = 0;
  if (!eglQueryDisplayAttribEXT(egl_display, EGL_DEVICE_EXT, &egl_device)) {
    LOG(ERROR) << "eglQueryDisplayAttribEXT failed";
    return nullptr;
  }

  if (!egl_device) {
    LOG(ERROR) << "Failed to retrieve EGLDeviceEXT";
    return nullptr;
  }
  return reinterpret_cast<EGLDeviceEXT>(egl_device);
}

gfx::ExtensionSet ToExtensionSet(intptr_t extensions) {
  const char* const* extensions_ptr =
      reinterpret_cast<const char* const*>(extensions);
  gfx::ExtensionSet extension_set;
  // SAFETY: required from OpenGL C style API.
  for (const char* const* p = extensions_ptr; *p != nullptr;
       UNSAFE_BUFFERS(++p)) {
    extension_set.insert(*p);
  }
  return extension_set;
}

}  // namespace

VkInstance QueryVkInstanceFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return VK_NULL_HANDLE;

  intptr_t instance = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_INSTANCE_ANGLE,
                               &instance)) {
    LOG(ERROR) << "Failed to retrieve VkInstance";
    return VK_NULL_HANDLE;
  }

  return reinterpret_cast<VkInstance>(instance);
}

GL_EXPORT uint32_t QueryVkVersionFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return 0;

  intptr_t version = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_VERSION_ANGLE,
                               &version)) {
    LOG(ERROR) << "Failed to retrieve vulkan version";
    return 0;
  }

  return static_cast<uint32_t>(version);
}

VkPhysicalDevice QueryVkPhysicalDeviceFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return VK_NULL_HANDLE;

  intptr_t physical_device = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_PHYSICAL_DEVICE_ANGLE,
                               &physical_device)) {
    LOG(ERROR) << "Failed to retrieve VkPhysicalDevice";
    return VK_NULL_HANDLE;
  }

  return reinterpret_cast<VkPhysicalDevice>(physical_device);
}

VkDevice QueryVkDeviceFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return VK_NULL_HANDLE;

  intptr_t device = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_DEVICE_ANGLE, &device)) {
    LOG(ERROR) << "Failed to retrieve VkDevice";
    return VK_NULL_HANDLE;
  }

  return reinterpret_cast<VkDevice>(device);
}

VkQueue QueryVkQueueFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return VK_NULL_HANDLE;

  intptr_t queue = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_QUEUE_ANGLE, &queue)) {
    LOG(ERROR) << "Failed to retrieve VkQueue";
    return VK_NULL_HANDLE;
  }

  return reinterpret_cast<VkQueue>(queue);
}

int QueryVkQueueFramiliyIndexFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return -1;

  intptr_t index = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_QUEUE_FAMILIY_INDEX_ANGLE,
                               &index)) {
    LOG(ERROR) << "Failed to retrieve queue familiy index";
    return -1;
  }

  return index;
}

gfx::ExtensionSet QueryVkDeviceExtensionsFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return {};

  intptr_t extensions = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_DEVICE_EXTENSIONS_ANGLE,
                               &extensions)) {
    LOG(ERROR) << "Failed to retrieve vulkan enabled device extensions";
    return {};
  }

  return ToExtensionSet(extensions);
}

gfx::ExtensionSet QueryVkInstanceExtensionsFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return {};

  intptr_t extensions = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_INSTANCE_EXTENSIONS_ANGLE,
                               &extensions)) {
    LOG(ERROR) << "Failed to retrieve vulkan enabled instance extensions";
    return {};
  }

  return ToExtensionSet(extensions);
}

const VkPhysicalDeviceFeatures2KHR* QueryVkEnabledDeviceFeaturesFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return nullptr;

  intptr_t features = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_FEATURES_ANGLE,
                               &features)) {
    LOG(ERROR) << "Failed to retrieve vulkan enabled device features";
    return nullptr;
  }

  return reinterpret_cast<const VkPhysicalDeviceFeatures2KHR*>(features);
}

PFN_vkGetInstanceProcAddr QueryVkGetInstanceProcAddrFromANGLE() {
  EGLDeviceEXT egl_device = GetEGLDeviceFromANGLE();
  if (!egl_device)
    return nullptr;

  intptr_t proc = 0;
  if (!eglQueryDeviceAttribEXT(egl_device, EGL_VULKAN_GET_INSTANCE_PROC_ADDR,
                               &proc)) {
    LOG(ERROR) << "Failed to retrieve vkGetInstanceProcAddr";
    return nullptr;
  }

  return reinterpret_cast<PFN_vkGetInstanceProcAddr>(proc);
}

}  // namespace gl
