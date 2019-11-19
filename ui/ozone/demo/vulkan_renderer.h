// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_VULKAN_RENDERER_H_
#define UI_OZONE_DEMO_VULKAN_RENDERER_H_

#include <vulkan/vulkan.h>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/demo/renderer_base.h"

namespace gpu {
class VulkanDeviceQueue;
class VulkanImplementation;
class VulkanCommandBuffer;
class VulkanCommandPool;
class VulkanSurface;
}  // namespace gpu

namespace ui {
class PlatformWindowSurface;

class VulkanRenderer : public RendererBase {
 public:
  VulkanRenderer(std::unique_ptr<PlatformWindowSurface> window_surface,
                 std::unique_ptr<gpu::VulkanSurface> vulkan_surface,
                 gpu::VulkanImplementation* vulkan_instance,
                 gfx::AcceleratedWidget widget,
                 const gfx::Size& size);
  ~VulkanRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  class Framebuffer {
   public:
    Framebuffer(VkImageView vk_image_view,
                VkFramebuffer vk_framebuffer,
                std::unique_ptr<gpu::VulkanCommandBuffer> command_buffer);
    ~Framebuffer();

    static std::unique_ptr<Framebuffer> Create(
        gpu::VulkanDeviceQueue* vulkan_device_queue,
        gpu::VulkanCommandPool* vulkan_command_pool,
        VkRenderPass vk_render_pass,
        gpu::VulkanSurface* vulkan_surface,
        VkImage image);

    VkImageView vk_image_view() const { return vk_image_view_; }
    VkFramebuffer vk_framebuffer() const { return vk_framebuffer_; }
    gpu::VulkanCommandBuffer* command_buffer() const {
      return command_buffer_.get();
    }

   private:
    const VkImageView vk_image_view_;
    const VkFramebuffer vk_framebuffer_;
    const std::unique_ptr<gpu::VulkanCommandBuffer> command_buffer_;
  };

  void DestroyRenderPass();
  void DestroyFramebuffers();
  void RecreateFramebuffers();
  void RenderFrame();
  void PostRenderFrameTask();

  std::unique_ptr<PlatformWindowSurface> window_surface_;

  std::vector<std::unique_ptr<Framebuffer>> framebuffers_;

  gpu::VulkanImplementation* const vulkan_implementation_;
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue_;
  std::unique_ptr<gpu::VulkanCommandPool> command_pool_;
  std::unique_ptr<gpu::VulkanSurface> vulkan_surface_;
  gfx::Size size_;

  VkRenderPass render_pass_ = VK_NULL_HANDLE;

  base::WeakPtrFactory<VulkanRenderer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VulkanRenderer);
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_VULKAN_RENDERER_H_
