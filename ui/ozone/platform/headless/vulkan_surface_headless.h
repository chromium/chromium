// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_HEADLESS_VULKAN_SURFACE_HEADLESS_H_
#define UI_OZONE_PLATFORM_HEADLESS_VULKAN_SURFACE_HEADLESS_H_

#include <vulkan/vulkan.h>
#include <memory>

#include "gpu/vulkan/vulkan_surface.h"

namespace ui {

class VulkanSurfaceHeadless : public gpu::VulkanSurface {
 public:
  static std::unique_ptr<VulkanSurfaceHeadless> Create(
      VkInstance vk_instance,
      gfx::AcceleratedWidget widget);
  VulkanSurfaceHeadless(VkInstance vk_instance,
                        VkSurfaceKHR vk_surface,
                        gfx::AcceleratedWidget widget);

  VulkanSurfaceHeadless(const VulkanSurfaceHeadless&) = delete;
  VulkanSurfaceHeadless& operator=(const VulkanSurfaceHeadless&) = delete;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_HEADLESS_VULKAN_SURFACE_HEADLESS_H_
