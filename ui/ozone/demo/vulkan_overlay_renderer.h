// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_VULKAN_OVERLAY_RENDERER_H_
#define UI_OZONE_DEMO_VULKAN_OVERLAY_RENDERER_H_

#include <vulkan/vulkan_core.h>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/demo/renderer_base.h"

namespace gfx {
class NativePixmap;
class GpuFence;
}  // namespace gfx

namespace gpu {
class VulkanDeviceQueue;
class VulkanImplementation;
class VulkanCommandBuffer;
class VulkanCommandPool;
}  // namespace gpu

namespace ui {
class OverlaySurface;
class PlatformWindowSurface;
class SurfaceFactoryOzone;

class VulkanOverlayRenderer : public RendererBase {
 public:
  VulkanOverlayRenderer(std::unique_ptr<PlatformWindowSurface> window_surface,
                        std::unique_ptr<OverlaySurface> overlay_surface,
                        SurfaceFactoryOzone* surface_factory_ozone,
                        gpu::VulkanImplementation* vulkan_instance,
                        gfx::AcceleratedWidget widget,
                        const gfx::Size& size);

  VulkanOverlayRenderer(const VulkanOverlayRenderer&) = delete;
  VulkanOverlayRenderer& operator=(const VulkanOverlayRenderer&) = delete;

  ~VulkanOverlayRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  class Buffer {
   public:
    Buffer(const gfx::Size& size,
           scoped_refptr<gfx::NativePixmap> native_pixmap,
           VkDeviceMemory vk_device_memory,
           VkImage vk_image,
           VkImageView vk_image_view,
           VkFramebuffer vk_framebuffer,
           std::unique_ptr<gpu::VulkanCommandBuffer> command_buffer,
           VkFence fence);
    ~Buffer();

    static std::unique_ptr<Buffer> Create(
        SurfaceFactoryOzone* surface_factory_ozone,
        gpu::VulkanImplementation* vulkan_implementation,
        gpu::VulkanDeviceQueue* vulkan_device_queue,
        gpu::VulkanCommandPool* vulkan_command_pool,
        VkRenderPass vk_render_pass,
        gfx::AcceleratedWidget widget,
        const gfx::Size& size);

    const gfx::Size& size() const { return size_; }
    const scoped_refptr<gfx::NativePixmap>& native_pixmap() const {
      return native_pixmap_;
    }
    VkDeviceMemory vk_device_memory() const { return vk_device_memory_; }
    VkImage vk_image() const { return vk_image_; }
    VkImageView vk_image_view() const { return vk_image_view_; }
    VkFramebuffer vk_framebuffer() const { return vk_framebuffer_; }
    gpu::VulkanCommandBuffer* command_buffer() const {
      return command_buffer_.get();
    }
    VkFence fence() const { return fence_; }

   private:
    const scoped_refptr<gfx::NativePixmap> native_pixmap_;
    const gfx::Size size_;
    const VkDeviceMemory vk_device_memory_;
    const VkImage vk_image_;
    const VkImageView vk_image_view_;
    const VkFramebuffer vk_framebuffer_;
    const std::unique_ptr<gpu::VulkanCommandBuffer> command_buffer_;
    const VkFence fence_;
  };

  void DestroyRenderPass();
  void DestroyBuffers();
  void RecreateBuffers();
  void RenderFrame();
  std::unique_ptr<gfx::GpuFence> SubmitFence(VkFence fence);
  void SubmitFrame(const Buffer* buffer);
  void PostRenderFrameTask();
  void OnFrameSubmitted(uint64_t frame_sequence, gfx::SwapResult swap_result);
  void OnFramePresented(uint64_t frame_sequence,
                        const gfx::PresentationFeedback& feedback);
  void OnFrameReleased(uint64_t frame_sequence);

  uint64_t frame_sequence_ = 0;
  int next_buffer_ = 0;
  size_t in_use_buffers_ = 0;
  std::unique_ptr<Buffer> buffers_[2];

  std::unique_ptr<PlatformWindowSurface> window_surface_;

  const raw_ptr<SurfaceFactoryOzone> surface_factory_ozone_;
  const raw_ptr<gpu::VulkanImplementation> vulkan_implementation_;
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue_;
  std::unique_ptr<gpu::VulkanCommandPool> command_pool_;
  std::unique_ptr<OverlaySurface> overlay_surface_;

  VkRenderPass render_pass_ = VK_NULL_HANDLE;

  base::WeakPtrFactory<VulkanOverlayRenderer> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_VULKAN_OVERLAY_RENDERER_H_
