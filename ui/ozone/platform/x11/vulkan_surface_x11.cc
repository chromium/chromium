// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/vulkan_surface_x11.h"

#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_xrandr_interval_only_vsync_provider.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/window_event_manager.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

// static
std::unique_ptr<VulkanSurfaceX11> VulkanSurfaceX11::Create(
    VkInstance vk_instance,
    x11::Window parent_window) {
  auto* connection = x11::Connection::Get();
  auto geometry = connection->GetGeometry(parent_window).Sync();
  if (!geometry) {
    LOG(ERROR) << "GetGeometry failed for window "
               << static_cast<uint32_t>(parent_window) << ".";
    return nullptr;
  }

  auto window = connection->GenerateId<x11::Window>();
  connection->CreateWindow(x11::CreateWindowRequest{
      .wid = window,
      .parent = parent_window,
      .width = geometry->width,
      .height = geometry->height,
      .c_class = x11::WindowClass::InputOutput,
  });
  if (connection->MapWindow({window}).Sync().error) {
    LOG(ERROR) << "Failed to create or map window.";
    return nullptr;
  }

  // TODO(penghuang): using the same xcb connection for VulkanSurface.
  VkSurfaceKHR vk_surface;
  const VkXcbSurfaceCreateInfoKHR surface_create_info = {
      .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
      .connection = connection->GetXlibDisplay().GetXcbConnection(),
      .window = static_cast<xcb_window_t>(window),
  };
  VkResult result = vkCreateXcbSurfaceKHR(vk_instance, &surface_create_info,
                                          nullptr, &vk_surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateXcbSurfaceKHR() failed: " << result;
    return nullptr;
  }
  return std::make_unique<VulkanSurfaceX11>(vk_instance, vk_surface,
                                            parent_window, window);
}

// When the screen is off or the window is offscreen, the X server keeps
// compositing windows with a 1Hz fake vblank.  However, there is an X server
// bug: the requested hardware vblanks are lost when screen turns off, which
// results in that tjh FIFO swapchain hangs.
//
// We work around the issue by setting the 2 seconds timeout for
// vkAcquireNextImageKHR().  When timeout happens, we consider the swapchain
// hang happened, and then make the surface lost, so a new swapchain will
// be created.
VulkanSurfaceX11::VulkanSurfaceX11(VkInstance vk_instance,
                                   VkSurfaceKHR vk_surface,
                                   x11::Window parent_window,
                                   x11::Window window)
    : gpu::VulkanSurface(
          vk_instance,
          static_cast<gfx::AcceleratedWidget>(window),
          vk_surface,
          /*acquire_next_image_timeout_ns=*/base::Time::kNanosecondsPerSecond *
              2,
          std::make_unique<ui::XrandrIntervalOnlyVSyncProvider>()),
      parent_window_(parent_window),
      window_(window),
      event_selector_(
          x11::Connection::Get()->ScopedSelectEvent(window,
                                                    x11::EventMask::Exposure)) {
  x11::Connection::Get()->AddEventObserver(this);
}

VulkanSurfaceX11::~VulkanSurfaceX11() {
  x11::Connection::Get()->RemoveEventObserver(this);
}

void VulkanSurfaceX11::Destroy() {
  VulkanSurface::Destroy();
  event_selector_.Reset();
  if (window_ != x11::Window::None) {
    auto* connection = x11::Connection::Get();
    connection->DestroyWindow({window_});
    window_ = x11::Window::None;
    connection->Flush();
  }
}

bool VulkanSurfaceX11::Reshape(const gfx::Size& size,
                               gfx::OverlayTransform pre_transform) {
  DCHECK_EQ(pre_transform, gfx::OVERLAY_TRANSFORM_NONE);

  // Vulkan WSI uses a separate xcb connection, so we need to synchronize
  // ConfigureWindow call.
  x11::Connection::Get()
      ->ConfigureWindow(x11::ConfigureWindowRequest{
          .window = window_, .width = size.width(), .height = size.height()})
      .Sync();

  return VulkanSurface::Reshape(size, pre_transform);
}

void VulkanSurfaceX11::OnEvent(const x11::Event& event) {
  auto* expose = event.As<x11::ExposeEvent>();
  if (!expose || expose->window != window_) {
    return;
  }

  x11::ExposeEvent forwarded_event = *expose;
  forwarded_event.window = parent_window_;
  x11::Connection::Get()->SendEvent(forwarded_event, parent_window_,
                                    x11::EventMask::Exposure);
  x11::Connection::Get()->Flush();
}

}  // namespace ui
