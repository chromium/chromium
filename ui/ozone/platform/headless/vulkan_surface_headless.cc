// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/vulkan_surface_headless.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

// static
std::unique_ptr<VulkanSurfaceHeadless> VulkanSurfaceHeadless::Create(
    VkInstance vk_instance,
    gfx::AcceleratedWidget widget) {
  VkSurfaceKHR vk_surface;
  const VkHeadlessSurfaceCreateInfoEXT surface_create_info = {
      .sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT,
      .pNext = nullptr,
      .flags = 0,
  };
  VkResult result = vkCreateHeadlessSurfaceEXT(
      vk_instance, &surface_create_info, nullptr, &vk_surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateHeadlessSurfaceEXT() failed: " << result;
    return nullptr;
  }
  return std::make_unique<VulkanSurfaceHeadless>(vk_instance, vk_surface,
                                                 widget);
}

VulkanSurfaceHeadless::VulkanSurfaceHeadless(VkInstance vk_instance,
                                             VkSurfaceKHR vk_surface,
                                             gfx::AcceleratedWidget widget)
    : gpu::VulkanSurface(vk_instance, widget, vk_surface) {}

}  // namespace ui
