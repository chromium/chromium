// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_VULKAN_SURFACE_X11_H_
#define UI_OZONE_PLATFORM_X11_VULKAN_SURFACE_X11_H_

#include <vulkan/vulkan.h>

#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"

namespace x11 {
class ScopedEventSelector;
}

namespace ui {

class VulkanSurfaceX11 : public gpu::VulkanSurface, public x11::EventObserver {
 public:
  static std::unique_ptr<VulkanSurfaceX11> Create(VkInstance vk_instance,
                                                  x11::Window parent_window);
  VulkanSurfaceX11(VkInstance vk_instance,
                   VkSurfaceKHR vk_surface,
                   x11::Window parent_window,
                   x11::Window window);

  VulkanSurfaceX11(const VulkanSurfaceX11&) = delete;
  VulkanSurfaceX11& operator=(const VulkanSurfaceX11&) = delete;

  ~VulkanSurfaceX11() override;

  // gpu::VulkanSurface:
  void Destroy() override;
  bool Reshape(const gfx::Size& size,
               gfx::OverlayTransform pre_transform) override;

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& xevent) override;

  const x11::Window parent_window_;
  x11::Window window_;
  x11::ScopedEventSelector event_selector_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_VULKAN_SURFACE_X11_H_
