// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/vulkan_renderer.h"

#include <vulkan/vulkan.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_swap_chain.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace ui {

namespace {
VkPipelineStageFlags GetPipelineStageFlags(const VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return VK_PIPELINE_STAGE_HOST_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT |
             VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
             VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT |
             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    default:
      NOTREACHED_IN_MIGRATION() << "layout=" << layout;
  }
  return 0;
}

VkAccessFlags GetAccessMask(const VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return 0;
    case VK_IMAGE_LAYOUT_GENERAL:
      DLOG(WARNING) << "VK_IMAGE_LAYOUT_GENERAL is used.";
      return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
             VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT |
             VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_HOST_WRITE_BIT |
             VK_ACCESS_HOST_READ_BIT;
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
      return VK_ACCESS_HOST_WRITE_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return 0;
    default:
      NOTREACHED_IN_MIGRATION() << "layout=" << layout;
  }
  return 0;
}
}  // namespace

VulkanRenderer::VulkanRenderer(
    std::unique_ptr<PlatformWindowSurface> window_surface,
    std::unique_ptr<gpu::VulkanSurface> vulkan_surface,
    gpu::VulkanImplementation* vulkan_implementation,
    gfx::AcceleratedWidget widget,
    const gfx::Size& size)
    : RendererBase(widget, size),
      window_surface_(std::move(window_surface)),
      vulkan_implementation_(vulkan_implementation),
      vulkan_surface_(std::move(vulkan_surface)),
      size_(size) {}

VulkanRenderer::~VulkanRenderer() {
  DestroyFramebuffers();
  DestroyRenderPass();
  vulkan_surface_->Destroy();
  vulkan_surface_.reset();
  command_pool_->Destroy();
  command_pool_.reset();
  device_queue_->Destroy();
  device_queue_.reset();
  window_surface_.reset();
}

bool VulkanRenderer::Initialize() {
  TRACE_EVENT1("ozone", "VulkanRenderer::Initialize", "widget", widget_);

  device_queue_ = gpu::CreateVulkanDeviceQueue(
      vulkan_implementation_,
      gpu::VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
          gpu::VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);
  if (!device_queue_) {
    LOG(FATAL) << "Failed to init device queue";
  }

  if (!vulkan_surface_->Initialize(
          device_queue_.get(), gpu::VulkanSurface::DEFAULT_SURFACE_FORMAT)) {
    LOG(FATAL) << "Failed to init surface";
  }

  VkAttachmentDescription render_pass_attachments[] = {{
      /* .flags = */ 0,
      /* .format = */ vulkan_surface_->surface_format().format,
      /* .samples = */ VK_SAMPLE_COUNT_1_BIT,
      /* .loadOp = */ VK_ATTACHMENT_LOAD_OP_CLEAR,
      /* .storeOp = */ VK_ATTACHMENT_STORE_OP_STORE,
      /* .stencilLoadOp = */ VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      /* .stencilStoreOp = */ VK_ATTACHMENT_STORE_OP_DONT_CARE,
      /* .initialLayout = */ VK_IMAGE_LAYOUT_UNDEFINED,
      /* .finalLayout = */ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  }};

  VkAttachmentReference color_attachment_references[] = {
      {/* .attachment = */ 0,
       /* .layout = */ VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription render_pass_subpasses[] = {{
      /* .flags = */ 0,
      /* .pipelineBindPoint = */ VK_PIPELINE_BIND_POINT_GRAPHICS,
      /* .inputAttachmentCount = */ 0,
      /* .pInputAttachments = */ nullptr,
      /* .colorAttachmentCount = */ std::size(color_attachment_references),
      /* .pColorAttachments = */ color_attachment_references,
      /* .pResolveAttachments = */ nullptr,
      /* .pDepthStencilAttachment = */ nullptr,
      /* .preserveAttachmentCount = */ 0,
      /* .pPreserveAttachments = */ nullptr,
  }};

  VkRenderPassCreateInfo render_pass_create_info = {
      /* .sType = */ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      /* .pNext = */ nullptr,
      /* .flags = */ 0,
      /* .attachmentCount = */ std::size(render_pass_attachments),
      /* .pAttachments = */ render_pass_attachments,
      /* .subpassCount = */ std::size(render_pass_subpasses),
      /* .pSubpasses = */ render_pass_subpasses,
      /* .dependencyCount = */ 0,
      /* .pDependencies = */ nullptr,
  };

  CHECK_EQ(vkCreateRenderPass(device_queue_->GetVulkanDevice(),
                              &render_pass_create_info, nullptr, &render_pass_),
           VK_SUCCESS);

  command_pool_ = std::make_unique<gpu::VulkanCommandPool>(device_queue_.get());
  CHECK(command_pool_->Initialize());

  RecreateFramebuffers();

  // Schedule the initial render.
  PostRenderFrameTask();
  return true;
}

void VulkanRenderer::DestroyRenderPass() {
  if (render_pass_ == VK_NULL_HANDLE)
    return;

  vkDestroyRenderPass(device_queue_->GetVulkanDevice(), render_pass_, nullptr);
  render_pass_ = VK_NULL_HANDLE;
}

void VulkanRenderer::DestroyFramebuffers() {
  VkDevice vk_device = device_queue_->GetVulkanDevice();

  VkResult result = vkQueueWaitIdle(device_queue_->GetVulkanQueue());
  CHECK_EQ(result, VK_SUCCESS);

  for (std::unique_ptr<Framebuffer>& framebuffer : framebuffers_) {
    if (!framebuffer)
      continue;

    framebuffer->command_buffer()->Destroy();
    vkDestroyFramebuffer(vk_device, framebuffer->vk_framebuffer(), nullptr);
    vkDestroyImageView(vk_device, framebuffer->vk_image_view(), nullptr);
    framebuffer.reset();
  }
}

void VulkanRenderer::RecreateFramebuffers() {
  TRACE_EVENT0("ozone", "VulkanRenderer::RecreateFramebuffers");

  DestroyFramebuffers();

  vulkan_surface_->Reshape(size_, gfx::OVERLAY_TRANSFORM_NONE);

  gpu::VulkanSwapChain* vulkan_swap_chain = vulkan_surface_->swap_chain();
  const uint32_t num_images = vulkan_swap_chain->num_images();
  framebuffers_.resize(num_images);
}

void VulkanRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "VulkanRenderer::RenderFrame");

  VkClearValue clear_value = {
      /* .color = */ {/* .float32 = */ {.5f, 1.f - NextFraction(), .5f, 1.f}}};

  gpu::VulkanSwapChain* vulkan_swap_chain = vulkan_surface_->swap_chain();
  {
    gpu::VulkanSwapChain::ScopedWrite scoped_write(vulkan_swap_chain);
    const uint32_t image = scoped_write.image_index();

    auto& framebuffer = framebuffers_[image];
    if (!framebuffer) {
      framebuffer = Framebuffer::Create(
          device_queue_.get(), command_pool_.get(), render_pass_,
          vulkan_surface_.get(), scoped_write.image());
      CHECK(framebuffer);
    }

    gpu::VulkanCommandBuffer& command_buffer = *framebuffer->command_buffer();

    {
      gpu::ScopedSingleUseCommandBufferRecorder recorder(command_buffer);
      {
        VkImageLayout old_layout = scoped_write.image_layout();
        VkImageLayout layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkImageMemoryBarrier image_memory_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = GetAccessMask(old_layout),
            .dstAccessMask = GetAccessMask(layout),
            .oldLayout = old_layout,
            .newLayout = layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = scoped_write.image(),
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        vkCmdPipelineBarrier(
            recorder.handle(), GetPipelineStageFlags(old_layout),
            GetPipelineStageFlags(layout), 0 /* dependencyFlags */,
            0 /* memoryBarrierCount */, nullptr /* pMemoryBarriers */,
            0 /* bufferMemoryBarrierCount */,
            nullptr /* pBufferMemoryBarriers */, 1, &image_memory_barrier);
      }

      VkRenderPassBeginInfo begin_info = {
          /* .sType = */ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          /* .pNext = */ nullptr,
          /* .renderPass = */ render_pass_,
          /* .framebuffer = */ framebuffer->vk_framebuffer(),
          /* .renderArea = */
          {
              /* .offset = */ {
                  /* .x = */ 0,
                  /* .y = */ 0,
              },
              /* .extent = */
              {
                  /* .width = */ static_cast<uint32_t>(
                      vulkan_swap_chain->size().width()),
                  /* .height = */
                  static_cast<uint32_t>(vulkan_swap_chain->size().height()),
              },
          },
          /* .clearValueCount = */ 1,
          /* .pClearValues = */ &clear_value,
      };

      vkCmdBeginRenderPass(recorder.handle(), &begin_info,
                           VK_SUBPASS_CONTENTS_INLINE);

      vkCmdEndRenderPass(recorder.handle());

      // Transfer image layout back to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for
      // presenting.
      {
        VkImageLayout old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkImageLayout layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkImageMemoryBarrier image_memory_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = GetAccessMask(old_layout),
            .dstAccessMask = GetAccessMask(layout),
            .oldLayout = old_layout,
            .newLayout = layout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = scoped_write.image(),
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };
        vkCmdPipelineBarrier(
            recorder.handle(), GetPipelineStageFlags(old_layout),
            GetPipelineStageFlags(layout), 0 /* dependencyFlags */,
            0 /* memoryBarrierCount */, nullptr /* pMemoryBarriers */,
            0 /* bufferMemoryBarrierCount */,
            nullptr /* pBufferMemoryBarriers */, 1, &image_memory_barrier);
      }
    }
    VkSemaphore begin_semaphore = scoped_write.begin_semaphore();
    VkSemaphore end_semaphore = scoped_write.end_semaphore();
    CHECK(command_buffer.Submit(1, &begin_semaphore, 1, &end_semaphore));
  }
  vulkan_surface_->SwapBuffers(
      base::DoNothingAs<void(const gfx::PresentationFeedback&)>());

  PostRenderFrameTask();
}

void VulkanRenderer::PostRenderFrameTask() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&VulkanRenderer::RenderFrame,
                                weak_ptr_factory_.GetWeakPtr()));
}

VulkanRenderer::Framebuffer::Framebuffer(
    VkImageView vk_image_view,
    VkFramebuffer vk_framebuffer,
    std::unique_ptr<gpu::VulkanCommandBuffer> command_buffer)
    : vk_image_view_(vk_image_view),
      vk_framebuffer_(vk_framebuffer),
      command_buffer_(std::move(command_buffer)) {}

VulkanRenderer::Framebuffer::~Framebuffer() {}

std::unique_ptr<VulkanRenderer::Framebuffer>
VulkanRenderer::Framebuffer::Create(gpu::VulkanDeviceQueue* vulkan_device_queue,
                                    gpu::VulkanCommandPool* vulkan_command_pool,
                                    VkRenderPass vk_render_pass,
                                    gpu::VulkanSurface* vulkan_surface,
                                    VkImage image) {
  gpu::VulkanSwapChain* vulkan_swap_chain = vulkan_surface->swap_chain();
  const VkDevice vk_device = vulkan_device_queue->GetVulkanDevice();
  VkImageViewCreateInfo vk_image_view_create_info = {
      /* .sType = */ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      /* .pNext = */ nullptr,
      /* .flags = */ 0,
      /* .image = */ image,
      /* .viewType = */ VK_IMAGE_VIEW_TYPE_2D,
      /* .format = */ vulkan_surface->surface_format().format,
      /* .components = */
      {
          /* .r = */ VK_COMPONENT_SWIZZLE_IDENTITY,
          /* .b = */ VK_COMPONENT_SWIZZLE_IDENTITY,
          /* .g = */ VK_COMPONENT_SWIZZLE_IDENTITY,
          /* .a = */ VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      /* .subresourceRange = */
      {
          /* .aspectMask = */ VK_IMAGE_ASPECT_COLOR_BIT,
          /* .baseMipLevel = */ 0,
          /* .levelCount = */ 1,
          /* .baseArrayLayer = */ 0,
          /* .layerCount = */ 1,
      },
  };

  VkResult result;
  VkImageView vk_image_view = VK_NULL_HANDLE;
  result = vkCreateImageView(vk_device, &vk_image_view_create_info, nullptr,
                             &vk_image_view);
  if (result != VK_SUCCESS) {
    LOG(FATAL) << "Failed to create a Vulkan image view.";
  }
  VkFramebufferCreateInfo vk_framebuffer_create_info = {
      /* .sType = */ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      /* .pNext = */ nullptr,
      /* .flags = */ 0,
      /* .renderPass = */ vk_render_pass,
      /* .attachmentCount = */ 1,
      /* .pAttachments = */ &vk_image_view,
      /* .width = */ static_cast<uint32_t>(vulkan_swap_chain->size().width()),
      /* .height = */ static_cast<uint32_t>(vulkan_swap_chain->size().height()),
      /* .layers = */ 1,
  };

  VkFramebuffer vk_framebuffer = VK_NULL_HANDLE;
  result = vkCreateFramebuffer(vk_device, &vk_framebuffer_create_info, nullptr,
                               &vk_framebuffer);
  if (result != VK_SUCCESS) {
    LOG(FATAL) << "Failed to create a Vulkan framebuffer.";
  }

  auto command_buffer = std::make_unique<gpu::VulkanCommandBuffer>(
      vulkan_device_queue, vulkan_command_pool, true /* primary */);
  CHECK(command_buffer->Initialize());

  return std::make_unique<VulkanRenderer::Framebuffer>(
      vk_image_view, vk_framebuffer, std::move(command_buffer));
}

}  // namespace ui
