// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_ANGLE_UTIL_VULKAN_H_
#define UI_GL_GL_ANGLE_UTIL_VULKAN_H_

#include <vulkan/vulkan_core.h>

#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Vulkan related helper functions.
GL_EXPORT VkInstance QueryVkInstanceFromANGLE();
GL_EXPORT uint32_t QueryVkVersionFromANGLE();
GL_EXPORT VkPhysicalDevice QueryVkPhysicalDeviceFromANGLE();
GL_EXPORT VkDevice QueryVkDeviceFromANGLE();
GL_EXPORT VkQueue QueryVkQueueFromANGLE();
GL_EXPORT int QueryVkQueueFramiliyIndexFromANGLE();
GL_EXPORT gfx::ExtensionSet QueryVkInstanceExtensionsFromANGLE();
GL_EXPORT gfx::ExtensionSet QueryVkDeviceExtensionsFromANGLE();
GL_EXPORT const VkPhysicalDeviceFeatures2KHR*
QueryVkEnabledDeviceFeaturesFromANGLE();
GL_EXPORT PFN_vkGetInstanceProcAddr QueryVkGetInstanceProcAddrFromANGLE();

}  // namespace gl

#endif  // UI_GL_GL_ANGLE_UTIL_VULKAN_H_
